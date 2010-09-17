--[[--------------------------------------------------------------------------

dialog.lua - Input from and output to the user

--]]--------------------------------------------------------------------------

local util     = require "clydelib/util"
local each     = util.each
local map      = util.map
local printf   = util.printf
local C        = require "clydelib/colorize"
local utilcore = require "clydelib.utilcore"
local G        = utilcore.gettext

local P = {}
dialog  = P

function P.show_list ( prefix, strings )
    print( "DEBUG starting show_list" )
    local maxcols = util.getcols() - ( #prefix + 1 )
    assert( maxcols > 0,
            "Prefix for indented list is wider than the screen" )

    if #strings == 0 then
        print( prefix .. " None" )
        return
    end

    local lines, spaces, linelen, i, j, max = {}, 0, 0, 1, 0, #strings

    while i + j <= max do
        local str     = strings[ i + j ]
        local nextlen = linelen + spaces + #str

        if ( nextlen >= maxcols ) then
            -- If one string is really really long just make it a line
            -- (this prevents an infinite loop)
            if j == 0 then j = 1 end

            local line = table.concat( strings, " ", i, i+j-1 )
            i = i + j
            j = 0; linelen = 0; spaces = 0
            table.insert( lines, line )
        else
            j       = j       + 1
            linelen = linelen + #str
            spaces  = spaces  + 1
        end
    end

    if i <= max then
        table.insert( lines, table.concat( strings, " ", i ))
    end

    local indent = string.rep( " ", #prefix )
    for i, line in ipairs( lines ) do
        local pre = ( i == 1 and prefix or indent )
        print( pre .. " " .. line )
    end

    return
end

function P.show_install_summary ( params )
    local alpm_pkgs, aur_pkgs, rem_pkgs
        = params.alpm or {}, params.aur or {}, params.rem or {}

    local dltotal, itotal, rmtotal      = 0, 0, 0

    -- First sum up all our download and install sizes...
    for alpm_pkg in each( alpm_pkgs ) do
        dltotal = dltotal + alpm_pkg.pkgobj:pkg_download_size()
        itotal  = itotal  + alpm_pkg.pkgobj:pkg_get_isize()
    end

    for aur_pkg in each( aur_pkgs ) do
        -- Too bad we can't calculate AUR package install size...
        dltotal = dltotal + aur_pkg.pkgobj:download_size()
    end

    -- Add up how many bytes will be freed from removing packages...
    for rem_pkg in each( rem_pkgs ) do
        rmtotal = rmtotal + rem_pkg.pkgobj:pkg_get_size()
    end

    -- if (config.op_s_printuris) then
    --     for i, pkg in ipairs(packages) do
    --         local db = pkg:pkg_get_db()
    --         local dburl = db:db_get_url()
    --         if (dburl) then
    --             printf("%s/%s\n", dburl, pkg:pkg_get_filename())
    --         else
    --             eprintf("LOG_ERROR", g("no URL for package: %s\n"), pkg:pkg_get_name())
    --         end
    --     end
    --     return cleanup( 1 )
    -- end

    local function to_pkg_names ( pkginfo )
        return string.format( "%s-%s-%d", pkginfo.name, pkginfo.version,
                              pkginfo.release )
    end

    local function to_pkg_names_and_size ( pkginfo )
        return string.format( "%s [%.2f MB]", to_pkg_names( pkginfo ),
                              pkginfo.pkgobj:pkg_get_size() / 1024^2 )
    end

    local to_strings = params.showsize and to_pkg_names_and_size
            or to_pkg_names

    -- First print repo packages to be installed...
    if ( next( aur_pkgs ) and next( alpm_pkgs )) then
        print( C.greb( "\n==>" )
            .. C.bright( " Installing the following packages from repos" ))
    end

    if ( next( alpm_pkgs )) then
        local prefix = string.format( G( "Targets (%d):" ), #alpm_pkgs )
        P.show_list( prefix, map( to_strings, alpm_pkgs ))
    end

    -- Then print AUR packages to be installed...
    if ( next( aur_pkgs )) then
        local function to_pkg_names ( pkginfo )
            return string.format( "%s-%s-%s",
                                  pkginfo.name,
                                  pkginfo.version,
                                  pkginfo.release )
        end

        print( C.greb( "\n==>" )
               .. C.bright(" Installing the following packages from AUR" ))
        local prefix = string.format( G("Targets (%d):"), #aur_pkgs )
        P.show_list( prefix, map( to_pkg_names, aur_pkgs ))
    end

    -- Lastly print packages to be removed...
    if next( rem_pkgs ) then
        local prefix = string.format( G("Remove (%d):"), #targets )
        P.show_list( prefix, map( to_strings, rem_pkgs ))
    end

    io.write( "\n" )

    if dltotal > 0 then
        printf( G( "Total Download Size:    %.2f MB\n" ),
                dltotal / (1024.0^2) )
    end
    if itotal > 0 then
        printf( G( "Total Installed Size:   %.2f MB\n" ),
                itotal  / (1024.0^2) )
    end
    if rmtotal > 0 then
        printf( G( "Total Removed Size:   %.2f MB\n" ),
                rmtotal / (1024.0^2) )
    end

    print( "" )

    return
end

return dialog
