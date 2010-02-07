module(..., package.seeall)
package.cpath = "/usr/lib/lua/5.1/?.so;"..package.cpath
package.path ="/usr/share/lua/5.1/?.lua;"..package.path
local lfs = require "lfs"
local alpm = require "lualpm"
local util = require "clydelib.util"
local utilcore = require "clydelib.utilcore"
local packages = require "clydelib.packages"
--require "luarocks.require"
--require "profiler"
--profiler.start("profile")
local eprintf = util.eprintf
local lprintf = util.lprintf
local printf = util.printf
local basename = util.basename
local cleanup = util.cleanup
local tblinsert = util.tblinsert
local realpath = util.realpath
local indentprint = util.indentprint
local list_display = util.list_display
local display_targets = util.display_targets
local display_synctargets = util.display_synctargets
local tbljoin = util.tbljoin
local tblisin = util.tblisin
local fastremove = util.fastremove
local tblremovedupes = util.tblremovedupes
local tbldiff = util.tbldiff
local tblstrdup = util.tblstrdup
local trans_init = util.trans_init
local yesno = util.yesno
local noyes = util.noyes
local trans_release = util.trans_release
local access = utilcore.access
local strerror = utilcore.strerror
local g = utilcore.gettext
--local strsplit = util.strsplit
local pm_targets = pm_targets
local community = community
local dump_pkg_changelog = packages.dump_pkg_changelog
local dump_pkg_files = packages.dump_pkg_files
local dump_pkg_sync = packages.dump_pkg_sync

local function clyde_upgrade(targets)
    local retval = 0
    local data = {}
    local transret
    if (not next(targets)) then
        lprintf("LOG_ERROR", g("no targets specified (use -h for help)\n"))
        return 1
    end

    local temptargs = {}

    for i, targ in ipairs(targets) do
        if targ:match("://") then
            local str = alpm.fetch_pkgurl(targ)
            if (str == nil) then
                return 1
            else
                tblinsert(temptargs, str)
            end
        else
            tblinsert(temptargs, targ)
        end
    end
    targets = nil
    targets = tblstrdup(temptargs)
--    temptargs = nil

    if (trans_init("T_T_UPGRADE", config.flags) == -1) then
        return 1
    end

    printf(g("loading package data...\n"))
    for i, targ in ipairs(targets) do
        if (alpm.trans_addtarget(targ) == -1) then
            eprintf("LOG_ERROR", "'%s': %s\n", targ, alpm.strerrorlast())
            trans_release()
            return 1
        end
    end

    transret, data = alpm.trans_prepare(data)
    --for i, v in ipairs(data) do print(i, v) end

    if (transret == -1) then
        eprintf("LOG_ERROR", g("failed to prepare transaction (%s)\n"), alpm.strerrorlast())
        if (alpm.strerrorlast() == "could not satisfy dependencies") then
            for i, miss in ipairs(data) do
                local dep = miss:miss_get_dep()
                local depstring = dep:dep_compute_string()

                printf(g(":: %s: requires %s\n", miss:miss_get_target(), depstring))
                depstring = nil
            end
        elseif (alpm.strerrorlast() == g("conflicting dependencies")) then
            for i, conflict in ipairs(data) do
                printf(g(":: %s: conflicts with %s\n"), conflict:conflict_get_package1(),
                    conflict:conflict_get_package2())
            end
        elseif (alpm.strerrorlast() == g("conflicting files")) then
            for i, conflict in ipairs(data) do
                --TODO: figure out how to do this...
            end
            printf(g("\nerrors occurred, no packages were upgraded.\n"))
        end
    trans_release()
    data = nil
    end
    if (alpm.trans_commit({}) == -1) then
        eprintf("LOG_ERROR", g("failed to commit transaction (%s)\n"), alpm.strerrorlast())
        trans_release()
        return 1
    end
    if (trans_release() == -1) then
        retval = 1
    end
    return retval

end



function main(targets)
--    local result, err = pcall(clyde_upgrade, targets)
    --result = true
    result = clyde_upgrade(targets)
--local err = ""
    if (not result) then
        --if (err:match("interrupted!")) then
        --    printf("\nInterrupt signal received\n\n")
        --    cleanup(result)
        --else
            --printf("%s\n", err)
        --end
        result = 1
    end
    
    return result
end