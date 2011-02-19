module(..., package.seeall)
---downloading AUR stuff---
local lfs     = require "lfs"
local socket  = require "socket"
local http    = require "socket.http"
local ltn12   = require "ltn12"
local zlib    = require "zlib"
local yajl    = require "yajl"
local C       = colorize
local alpm    = require "lualpm"

local util    = require "clydelib.util"
local printf  = util.printf
local eprintf = util.eprintf
local yesno   = util.yesno
local noyes   = util.noyes

local utilcore = require "clydelib.utilcore"
local geteuid = utilcore.getuid
local umask   = utilcore.umask

local upgrade  = require "clydelib.upgrade"

local ssl = require "ssl"
-- credit for params and create goes to James McLaughlin
local params = {
    mode = "client",
    protocol = "sslv23",
    cafile = "/etc/ssl/certs/ca-certificates.crt",
    verify = "peer",
    options = "all",
}

local AURURI    = "http://aur.archlinux.org:443"
local PBURIFMT  = AURURI .. "/packages/%s/%s/PKGBUILD"
local PKGURIFMT = AURURI .. "/packages/%s/%s.tar.gz"

function srcpkguri ( pkgname )
    return string.format( PKGURIFMT, pkgname, pkgname )
end

function pkgbuilduri ( pkgname )
    return string.format( PBURIFMT, pkgname, pkgname )
end

-- Create a URI for RPC calls
local VALID_RPC_METHOD = { search = true, info = true, msearch = true }
function rpcuri ( method, arg )
    if not method or not VALID_RPC_METHOD[ method ] then
        error( method .. " is not a valid AUR RPC method" )
    end

    return AURURI .. "/rpc.php?type=" .. method .. "&arg=" .. arg
end

function get_builduser()
    return config.op_s_build_user or os.getenv("SUDO_USER") or "root"
end

function get_builddir ()
    return config.builddir or "/tmp/clyde-" .. get_builduser()
end

local try = socket.try
local protect = socket.protect
function create_socket()
    local t = {c=try(socket.tcp())}

    function idx (tbl, key)
        --print("idx " .. key)
        return function (prxy, ...)
                   local c = prxy.c
                   return c[key](c,...)
               end
    end

    function t:connect(host, port)
        --print ("proxy connect ", host, port)
        try(self.c:connect(host, port))
        --print ("connected")
        self.c = try(ssl.wrap(self.c,params))
        --print("wrapped")
        try(self.c:dohandshake())
        --print("handshaked")
        return 1
    end

    return setmetatable(t, {__index = idx})
end

function make_builddir ( pkgname )
    local bdir = get_builddir()
    if not pcall( lfs.opendir, bdir ) then lfs.mkdir( bdir ) end
    local pkgdir = bdir .. "/" .. pkgname
    if not pcall( lfs.opendir, bdir ) then lfs.mkdir( pkgdir ) end
    return pkgdir
end

function get_content_length ( uri )
    local r, c, h = http.request {
        method = "HEAD", url = uri, create = create_socket,
    }
    if c ~= 200 then return nil
    else return h['content-length'] end
end

-- RPC -----------------------------------------------------------------------

local NEWKEYNAME_FOR = { Description = "desc",
                         NumVotes    = "votes",
                         CategoryID  = "category",
                         LocationID  = "location",
                         OutOfDate   = "outdated" }

local function aur_rpc_keyname ( key )
    return NEWKEYNAME_FOR[ key ] or key:lower()
end

function rpc_search ( query )
    -- Allow search queries to contain regexp anchors... only!
    local regexp
    if query:match( "^^" ) or query:match( "$$" ) then
        regexp = query
        regexp = regexp:gsub("([().%+*?[-])", "%%%1")
        query  = query:gsub( "^^", "" )
        query  = query:gsub( "$$", "" )
    end

    local url    = rpcuri( "search", query )
    local chunks = {}
    local ret, code = http.request { url    = url,
                                     create = create_socket,
                                     sink   = ltn12.sink.table( chunks ) }
    if not ret or code ~= 200 then
        error( "HTTP request for info RPC failed: " .. code )
    end

    local jsontxt = table.concat( chunks, "" )
    if not jsontxt then error( "Failed to search AUR using RPC" ) end

    --[[ Create a custom JSON SAX parser. On results with ~1k entries
         yajl.to_value was bugging out. This is more efficient anyways.
         We can insert values into our results directly... ]]--

    local results = {}

    local in_results, in_pkg, pkgkey, pkginfo = false, false, "", {}
    local parser = yajl.parser {
        events = { open_array  = function ( evts )
                                     in_results = true
                                 end,
                   open_object = function ( evts )
                                     if in_results then in_pkg = true end
                                 end,
                   close       = function ( evts, type )
                                     if type == "array" and in_results then
                                         in_results = false
                                     elseif type == "object" and in_pkg then
                                         in_pkg  = false
                                         -- Prepare pkginfo for a new
                                         -- package JSON-object entry
                                         pkginfo = {}
                                     end
                                 end,
                   object_key  = function ( evts, name )
                                     if not in_pkg then return end
                                     pkgkey = aur_rpc_keyname( name )
                                 end,
                   -- I think AUR does only string datatypes... heh
                   value       = function ( evts, value, type )
                                     if not in_pkg then return end
                                     if pkgkey == "name" then
                                         results[ value ] = pkginfo
                                     elseif pkgkey == "outdated" then
                                         value = ( value == "1" )
                                     end

                                     pkginfo[ pkgkey ] = value
                                 end
           } }

    parser( jsontxt )

    if not regexp then return results end

    -- Filter out results if regexp-anchors were given
    for name, info in pairs( results ) do
        if not name:match( regexp ) then
            results[ name ] = nil
        end
    end
    return results
end

------------------------------------------------------------------------------

function download ( pkgname, destdir )
    local pkgfile = string.format( "%s/%s.src.tar.gz", destdir, pkgname )

    local oldmask = umask( tonumber( "133", 8 ))
    local pkgfh, err = io.open( pkgfile, "w" )
    assert( pkgfh, err )

    print( C.greb("==>") .. C.bright( " Downloading " .. pkgname .. "..."))
    assert( http.request { url    = srcpkguri( pkgname ),
                           create = create_socket,
                           sink   = ltn12.sink.file( pkgfh ) } );
    return pkgfile
end

function getgzip ( geturl )
    local sinktbl = {}

    local r, e = http.request {
        url     = geturl;
        sink    = ltn12.sink.table(sinktbl);
        headers = { ["Accept-Encoding"] = "gzip" };
        create  = create_socket;
    }
    if (e ~= 200) then
        return nil
    end
    local inflated = zlib.inflate(table.concat(sinktbl))
    return inflated:read("*a")
end

function download_extract ( pkgname )
    local builddir = make_builddir( pkgname )
    local pkgpath  = download( pkgname, builddir )
    local pkgfile  = pkgpath:gsub( "^.*/", "" )

    umask( tonumber( "033", 8 ))
    local cmdfmt = "bsdtar -x --file '%s'"
        .. " --no-same-owner --no-same-permissions"
        .. " --directory '%s'"
    local cmdline = string.format( cmdfmt, pkgpath, builddir )
    if os.execute( cmdline ) ~= 0 then
        error( "bsdtar failed to extract " .. pkgpath )
    end

    -- Don't let root hog our package files...
    if utilcore.geteuid() == 0 then
        -- TODO: change owner group to something other than "users"?
        local builduser = get_builduser()
        cmdline = string.format( "chown '%s:users' -R '%s'",
                                 builduser, builddir )

        if os.execute( cmdline ) ~= 0 then
            error( string.format( "failed to chown '%s' to '%s'",
                                  builddir, builduser ))
        end
    end

    -- Return the path to the directory that was (hopefully) extracted
    local extdir = builddir .. "/" .. pkgname
    if not lfs.attributes( extdir ) then
        error( extdir .. " was not extracted" )
    end

    return extdir
end

function customizepkg ( pkgname, pkgdir )
    print( C.greb( "==>" )
       .. C.bright( " Customizing " .. pkgname .. "..." ))

    local editor
    if (not config.editor) then
        printf("No editor is set.\n")
        printf("What editor would you like to use? ")
        editor = io.read()
        if (editor == (" "):rep(#editor)) then
            printf("Defaulting to nano")
            editor = "nano"
        end

        print( "Using " .. editor .. "." )
        print( "To avoid this message in the future please create a "
               .. "config file or use the --editor command line option")

        config.editor = editor -- Avoid asking this repeatedly...
    else
        editor = config.editor
    end

    local function offeredit ( filename )
        local msg = "Edit the " .. filename .. "? (recommended)"
        local confirmedit = yesno( msg )

        -- Changed this to stop asking after the user says yes.
        -- JD
        if confirmedit then
            -- TODO error checking/recovery?
            local olddir = lfs.currentdir()
            assert( olddir, lfs.chdir( pkgdir ))
            os.execute( editor .. " " .. filename )
            assert( lfs.chdir( olddir ))
        end
    end

    print(C.yelb .. C.blink("==>") .. C.yelb(" WARNING: ") .. C.bright
          .. "Packages from the AUR are potentially dangerous!"
          .. C.reset)

    local success, errmsg = pcall(
        function ()
            offeredit( "PKGBUILD" )

            local instfile = util.getbasharray("PKGBUILD", "install")
            if instfile and #instfile > 0 then
                offeredit( instfile )
            end
        end )

    if not success then
        eprintf( "LOG_ERROR", "Error customizing %s: %s",
                 pkgdir, errmsg )
    end
end

function makepkg ( target, mkpkgopts )
    local oldwd = lfs.currentdir()
    assert( lfs.chdir( target ))

    local user = get_builduser()
    local maker

    if (user == "root") then
        maker = function ()
            printf( C.redb("==> ")
            .. C.bright( C.onred( "Running makepkg as root is a bad idea!" )))
            print("")
            printf( C.redb("==> ")
                .. C.bright("To avoid this message please set BuildUser "
                            .. "in clyde.conf\n"))
            local response = noyes( C.redb("==> ")
                                .. C.bright("Continue anyway?"))
            if ( response )  then
                return os.execute("makepkg -f --asroot "..mkpkgopts)
            else
                error( "Build aborted" )
            end
        end
    else
        maker = function ()
            return os.execute("su "..user.." -c 'makepkg -f' "..mkpkgopts)
        end
    end

    local success, err = pcall( function ()
                                    if maker() ~= 0 then
                                        error( "Build failed" )
                                    else
                                        return true
                                    end
                                end )
    lfs.chdir( oldwd )

    if not success then
        eprintf( "LOG_ERROR", "%s\n", err )
        util.cleanup( 1 )
    end

    return
end

function installpkg( target )
    local user     = get_builduser()
    local builddir = get_builddir()

    local pkgdir = os.getenv( "PKGDEST" )
        or util.getbasharrayuser("/etc/makepkg.conf", "PKGDEST", user)

    if not pkgdir or #pkgdir == 0 then
        pkgdir = builddir.."/"..target.."/"..target
    end

    local pkgfiles = {}
    for file in lfs.dir( pkgdir ) do
        if ( file:match( "[.]pkg[.]tar[.]%az$" ) and
             not file:match( "[.]src[.]pkg[.]tar[.]%az$" )) then
            table.insert( pkgfiles, pkgdir .. "/" .. file )
        end
    end

    local maxn = table.maxn( pkgfiles )
    if maxn == 0 then
        eprintf( "LOG_ERROR", "Could not find a built package in %s.",
                 pkgdir )
        util.cleanup( 1 )
    end

    local function pkg_ver ( pkgpath )
        local pkg, retcode = alpm.pkg_load( pkgpath, false )
        if not retcode or retcode ~= 0 then
            error( "Failed to load package file:\n" .. pkgpath .. "\n"
                   .. "ALPM Error: " .. alpm.strerrorlast())
        end
        return pkg:pkg_get_version()
    end

    local function by_version ( left, right )
        return alpm.pkg_vercmp( pkg_ver( left ), pkg_ver( right ))
    end

    table.sort( pkgfiles, by_version ) -- highest version should be last now
    local ret = upgrade.main({ pkgfiles[ maxn ] }) -- expects a table as arg
    if ( ret ~= 0 ) then util.cleanup( ret ) end
    return
end
