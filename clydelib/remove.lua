module( ..., package.seeall )

local alpm = require "lualpm"

local utilcore = require "clydelib.utilcore"
local g        = utilcore.gettext

local util            = require "clydelib.util"
local eprintf         = util.eprintf
local lprintf         = util.lprintf
local printf          = util.printf
local list_display    = util.list_display
local display_targets = util.display_targets
local trans_init      = util.trans_init
local yesno           = util.yesno
local noyes           = util.noyes
local trans_release   = util.trans_release

local ui = require "clydelib.ui"

local function queue_pkg_remove ( pkgobj )
    if alpm.trans_remove_pkg( pkgobj ) == -1 then
        error{ errmsg = { "'%s': %s\n", pkgobj:pkg_get_name() } }
    end
end

local function queue_grp_remove ( grpobj )
    local grpname  = grpobj:grp_get_name()
    local packages = grpobj:grp_get_pkgs()
    local pkgnames = {}
    for i, obj in ipairs( packages ) do
        table.insert( pkgnames, obj:pkg_get_name())
    end

    print( ui.format( ":: There are %d members in group %s:\n",
                      #packages, grpname ))
    list_display( "    ", pkgnames )
    print()

    -- no translations for the below messages
    local rem_all = yesno( "    Remove entire group?" )
    print()

    local function confirmed ( pkgname )
        return yesno( ui.format( ":: Remove %s from group %s?",
                                 pkgname, grpname ))
    end
                                       
    for i, pkgobj in ipairs( packages ) do
        if rem_all or confirmed( pkgnames[i] ) then
            queue_pkg_remove( pkgobj )
        end
    end
end

local function queue_remove ( target, localdb )
    local pkgobj = localdb:db_get_pkg( target )
    if pkgobj then
        queue_pkg_remove( pkgobj )
        return true
    end

    -- no translation
    -- print( target .. " not found, searching for group..." )
    local grpobj = localdb:db_readgrp( target )
    if grpobj then
        queue_grp_remove( grpobj )
        return true
    end

    return false
end

local function clyde_remove(targets)
    local localdb = alpm.option_get_localdb()

    local found_one
    for i, target in ipairs( targets ) do
        if queue_remove( target, localdb ) then
            found_one = true
        else
            eprintf( "LOG_ERROR", "target not found: %s\n", target )
        end
    end
    if not found_one then return end

    local transret, errlist = alpm.trans_prepare({})
    if transret == -1 then
        error{ errmsg  = { "failed to prepare transaction (%s)\n" };
               errlist = errlist; }
    end

    local found_holdpkg
    for i, pkg in ipairs( alpm.trans_get_remove()) do
        local name = pkg:pkg_get_name()
        if config.holdpkg[ name ] then
            lprintf( "LOG_WARNING",
                     g("%s is designated as a HoldPkg.\n"), name )
            found_holdpkg = true
        end
    end

    local holdmsg = g("HoldPkg was found in target list. "
                      .. "Do you want to continue?")
    if found_holdpkg and not noyes( holdmsg ) then return end

    if config.flags.recurse or config.flags.cascade then
        local pkglist = alpm.trans_get_remove()
        display_targets( pkglist, false )
        print()

        if not yesno( g("Do you want to remove these packages?" )) then
            return
        end
    end

    if alpm.trans_commit({}) == -1 then
        error{ errmsg = { "failed to commit transaction (%s)\n" }}
    end

    return
end

local error_handlers = {
    -- can occur when preparing transaction
    P_E_UNSATISFIED_DEPS =
        function ( err )
            for i, miss in ipairs( err.errlist ) do
                local msg =
                    ui.format( ":: %s: requires %s\n",
                               miss:miss_get_target(),
                               miss:miss_get_dep():dep_compute_string())
                print( msg )
            end
        end ;
}

function main( targets )
    if not next( targets ) then
        lprintf( "LOG_ERROR", g("no targets specified (use -h for help)\n" ))
        return 1
    end

    if trans_init( config.flags ) == -1 then return 1 end

    local success, err = pcall( clyde_remove, targets )
    if trans_release() == -1 then return 1 end

    if not success then
        -- rethrow error if it is not internally generated
        if type( err ) ~= "table" then error( err, 0 ) end

        err.errmsg[1] = g( err.errmsg[1] )
        -- supply strerrorlast even if it is not needed

        eprintf( "LOG_ERROR", unpack( err.errmsg ), alpm.strerrorlast())
        local handler = error_handlers[ alpm.pm_errno() ]
        if handler then handler( err ) end

        return 1
    end

    return 0
end
