--[[

LuAUR - Lua interface to the ArchLinux User Repository
by Justin Davis <jrcd83@gmail.com>

Adapted from clydes builting AUR code.

]]--

local yajl = require "yajl"
local http = require "socket.http"
local core = require "luaur.core"
local util = require "luaur.util"

-- CONSTANTS -----------------------------------------------------------------

local VERSION       = "0.01"
local AUR_BASEURI   = "http://aur.archlinux.org"
local AUR_PKGFMT    = "/packages/%s/%s.tar.gz"
local AUR_USERAGENT = "LuAUR/v" .. VERSION

------------------------------------------------------------------------------

AUR = { basepath = "/tmp/luaur" }
AUR.__index = AUR

function AUR:new ( params )
    local obj = params or { }
    setmetatable( obj, self )
    return obj
end


local VALID_METHOD = { search = true, info = true, msearch = true }

local function aur_rpc_url ( method, arg )
    if not method or not VALID_METHOD[ method ] then
        error( method .. " is not a valid AUR RPC method" )
    end

    return AUR_BASEURI .. "/rpc.php?type=" .. method .. "&arg=" .. arg
end

function AUR:search ( query )
    -- Allow search queries to contain regexp anchors... only!
    local regexp
    if query:match( "^^" ) or query:match( "$$" ) then
        regexp = query
        regexp = regexp:gsub("([().%+*?[-])", "%%%1")
        query  = query:gsub( "^^", "" )
        query  = query:gsub( "$$", "" )
    end

    local url     = aur_rpc_url( "search", query )
    local jsontxt = http.request( url )
    if not jsontxt then
        error( "Failed to search AUR using RPC" )
    end

    --[[ Create a custom JSON SAX parser. On results with ~1k entries
         yajl.to_value was bugging out. This is more efficient anyways.
         We can insert values into our results directly... ]]--

    local results     = {}
    local newname_for = { Description = "desc",
                          NumVotes    = "votes",
                          CategoryID  = "category",
                          LocationID  = "location",
                          OutOfDate   = "outdated" }

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
                                     pkgkey = newname_for[ name ]
                                         or name:lower()
                                 end,
                   -- I think AUR does only string datatypes... heh
                   value       = function ( evts, value, type )
                                     if not in_pkg then return end
                                     if pkgkey == "name" then
                                         results[ value ] = pkginfo
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

function AUR:get ( package )
    local pkg = AURPackage:new { basepath = self.basepath,
                                 dlpath   = self.dlpath,
                                 extpath  = self.extpath,
                                 destpath = self.destpath,
                                 proxy    = self.proxy,
                                 name     = package }

    -- We want to test if the package really exists...
    if not pkg:download_size() then return nil end
    return pkg
end

------------------------------------------------------------------------------

AURPackage = { }
AURPackage.__index = AURPackage

function AURPackage:new ( params ) --name, basepath, proxy )n
    params = params or { }
    assert( params.name, "Parameter 'name' must be specified" )
    assert( ( params.dlpath and params.extpath and params.destpath )
            or params.basepath, [[
Parameter 'basepath' must be specified unless all other paths are provided
]] )

    local dlpath   = params.dlpath   or params.basepath .. "/src"
    local extpath  = params.extpath  or params.basepath .. "/build"
    local destpath = params.destpath or params.basepath .. "/cache"

    local obj    = params
    obj.pkgname  = obj.name .. ".src.tar.gz"
    obj.dlpath   = dlpath
    obj.extpath  = extpath
    obj.destpath = destpath

    setmetatable( obj, self )
    return obj
end

function AURPackage:download_url ( )
    return AUR_BASEURI .. string.format( AUR_PKGFMT, self.name, self.name )
end

function AURPackage:download_size ( )
    if self.dlsize then return self.dlsize end
    
    USERAGENT = AUR_USERAGENT
    local pkgurl = self:download_url()
    local good, status, headers
        = http.request{ url = pkgurl, method = "HEAD", proxy = self.proxy }

    if not good or status ~= 200 then
        return nil
    end

    self.dlsize = tonumber( headers[ "content-length" ] )
    return self.dlsize
end

function AURPackage:download ( callback )
    if self.tgzpath then return self.tgzpath end

    local pkgurl       = self:download_url()
    local pkgname      = self.pkgname
    local pkgpath      = self.dlpath .. "/" .. pkgname

    -- Make sure the destination directory exists...
    rec_mkdir( self.dlpath )

    local pkgfile, err = io.open( pkgpath, "wb" )
    assert( pkgfile, err )

    local dlsink = ltn12.sink.file( pkgfile )

    -- If a callback is provided, call it with the download progress...
    if callback then
        if type(callback) ~= "function" then
            error( "Argument to download method must be a callback func" )
        end

        local current, total = 0, self:download_size()
        local dlfilter = function ( dlchunk )
                             if dlchunk == nil or #dlchunk == 0 then
                                 return dlchunk
                             end
                             current = current + #dlchunk
                             callback( current, total )
                             return dlchunk
                         end
        dlsink = ltn12.sink.chain( dlfilter, dlsink )
    end

    USERAGENT = AUR_USERAGENT
    local good, status = http.request { url    = pkgurl,
                                        proxy  = self.proxy,
                                        sink   = dlsink }

    if not good or status ~= 200 then
        local err
        if status ~= 200 then
            err = "HTTP error status " .. status
        else
            err = status
        end
        error( string.format( "Failed to download %s: %s", pkgurl, err ))
    end

    self.tgzpath = pkgpath
    return pkgpath
end

function AURPackage:extract ( destdir )
    local pkgpath = self:download()

    -- Do not extract files redundantly...
    if self.pkgdir then return self.pkgdir end

    if destdir == nil then
        destdir = self.extpath
    else
        destdir:gsub( "/+$", "" )
    end

    rec_mkdir( destdir )
    local cmd = string.format( "bsdtar -zxf %s -C %s", pkgpath, destdir )
    local ret = os.execute( cmd )
    if ret ~= 0 then
        error( string.format( "bsdtar returned error code %d", ret ))
    end

    self.pkgdir = destdir .. "/" .. self.name
    return self.pkgdir
end

local function unquote_bash ( quoted_text )
    -- Convert bash arrays (surrounded by parens) into tables
    local noparen, subcount = quoted_text:gsub( "^%((.+)%)$", "%1" )
    if subcount > 0 then
        local wordlist = {}
        for word in noparen:gmatch( "(%S+)" ) do
            table.insert( wordlist, unquote_bash( word ))
        end
        return wordlist
    end

    -- Remove double or single quotes from bash strings
    local text = quoted_text
    text = text:gsub( '^"(.+)"$', "%1" )
    text = text:gsub( "^'(.+)'$", "%1" )
    return text
end

local function pkgbuild_fields ( text )
    local results = {}

    -- First find all fields without quoting characters...
    for name, value in text:gmatch( "([%l%d]+)=(%w%S*)" ) do
        results[ name ] = value
    end

    -- Now handle all quoted field values...
    local quoters = { '""', "''", "()" }
    local fmt     = '([%%l%%d]+)=(%%b%s)'

    for i, quotes in ipairs( quoters ) do
        local regexp = string.format( fmt, quotes )
        for name, value in text:gmatch( regexp ) do
            results[ name ] = unquote_bash( value )
        end
    end

    return results
end

-- Downloads, extracts tarball (if needed) and then parses the PKGBUILD...
function AURPackage:get_pkgbuild ( )
    local extdir = self:extract()

    if self.pkgbuild_info then return self.pkgbuild_info end

    local pbpath      = extdir .. "/PKGBUILD"
    local pbfile, err = io.open( pbpath, "r" )
    assert( pbfile, err )

    local pbtext       = pbfile:read( "*a" )
    self.pkgbuild_info = pkgbuild_fields( pbtext )

    pbfile:close()

    return self.pkgbuild_info
end

function AURPackage:_builtpkg_path ( pkgdest )
    local pkgbuild = self:get_pkgbuild()
    local arch     = pkgbuild.arch
    if ( type( arch ) == "table" or arch ~= "any" ) then
        arch = core.arch()
    end
    
    local destfile = string.format( "%s/%s-%s-%d-%s.pkg.tar.xz",
                                    pkgdest, self.name,
                                    pkgbuild.pkgver,
                                    pkgbuild.pkgrel,
                                    arch )
    return destfile
end

function AURPackage:build ( params )
    if self.pkgpath then return self.pkgpath end

    params = params or {}
    local extdir = self:extract( params.buildbase )

    local pkgdest = params.pkgdest
    if pkgdest == nil then
        pkgdest = self.destpath
    else
        pkgdest:gsub( "/+$", "" )
    end
    pkgdest = absdir( pkgdest )

    local destfile = self:_builtpkg_path( pkgdest )
    local testfile = io.open( destfile, "r" )
    if testfile then
        testfile:close()

        -- Use an already created pkgfile if given the 'usecached' param.
        if params.usecached then
            self.pkgpath = destfile
            return destfile
        end
    end

    rec_mkdir( pkgdest)
    local oldir    = chdir( extdir )

    local cmd = "makepkg"
    if params.prefix then cmd = params.prefix .. " " .. cmd end
    if params.args   then cmd = cmd .. " " .. params.args   end

    -- TODO: restore this env variable afterwards
    core.setenv( "PKGDEST", pkgdest )

    local retval = os.execute( cmd )
    if ( retval ~= 0 ) then
        error( "makepkg returned error code " .. retval )
    end

    chdir( oldir )

    -- Make sure the .pkg.tar.gz file was created...
    assert( io.open( destfile, "r" ))

    self.pkgpath = destfile
    return destfile
end

