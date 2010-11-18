module(..., package.seeall)
local lfs = require "lfs"
local alpm = require "lualpm"
local util = require "clydelib.util"
local utilcore = require "clydelib.utilcore"
local packages = require "clydelib.packages"
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
local pm_targets = pm_targets
local community = community
local dump_pkg_changelog = packages.dump_pkg_changelog
local dump_pkg_files = packages.dump_pkg_files
local dump_pkg_sync = packages.dump_pkg_sync
local C = colorize

local function clyde_remove(targets)
    local retval = 0
    local function removecleanup()
        if (trans_release() == -1) then
            print("trans_release fail")
            retval = 1
        end
        return(retval)
    end

    local transret
    local data = {}
    if (not next(targets)) then
        lprintf("LOG_ERROR", g("no targets specified (use -h for help)\n"))
        return 1
    end

    if (trans_init(config.flags) == -1) then
        return 1
    end

    for i, targ in ipairs(targets) do
        if (alpm.remove_target(targ) == -1) then
            if (alpm.pm_errno() == "P_E_PKG_NOT_FOUND") then
                printf(g("%s not found, searching for group...\n"), targ)
                local grp = db_local:db_readgrp(targ)
                if (not grp) then
                    eprintf("LOG_ERROR", g("'%s': not found in local db\n"), targ)
                    retval = 1
                    return removecleanup()
                else
                    local packages = grp:grp_get_pkgs()
                    local pkgnames = {}
                    for i, pkgname in ipairs(packages) do
                        tblinsert(pkgnames, pkgname:pkg_get_name())
                    end
                    printf(g(C.blub("::")..C.bright(" group %s:\n")), targ)
                    list_display("   ", pkgnames)
                    local all = yesno(g("    Remove whole content?"))
                    for i, pkgn in ipairs(pkgnames) do
                        if (all or yesno(g(C.yelb("::")..C.bright(" Remove %s from group %s?")), pkgn, targ)) then
                            if (alpm.remove_target(pkgn) == -1) then
                                eprintf("LOG_ERROR", "'%s': %s\n", targ, alpm.strerrorlast())
                                retval = 1
                                pkgnames = nil
                                return removecleanup()
                            end
                        end
                    end
                    pkgnames = nil
                end
            else
                eprintf("LOG_ERROR", "'%s': %s\n", targ, alpm.strerrorlast())
                retval = 1
                return removecleanup()
            end
        end
    end

    transret, data = alpm.trans_prepare(data)

    if (transret == -1) then
        eprintf("LOG_ERROR", g("failed to prepare transaction (%s)\n"), alpm.strerrorlast())
        if (alpm.pm_errno() == "P_E_UNSATISFIED_DEPS") then
            for i, miss in ipairs(data) do
                local dep = miss:miss_get_dep()
                local depstring = dep:dep_compute_string()
                printf(g(C.blub("::")..C.bright(" %s: requires %s\n")), miss:miss_get_target(), depstring)
                depstring = nil
            end
            data =  nil
        end
        retval = 1
        return removecleanup()
    end

    local holdpkg = false
    local transpkgs = alpm.trans_get_remove()
    for i, pkg in ipairs(transpkgs) do
        if (tblisin(config.holdpkg, pkg:pkg_get_name())) then
            lprintf("LOG_WARNING", g("%s is designated as a HoldPkg.\n"), pkg:pkg_get_name())
            holdpkg = true
        end
    end
    if (holdpkg and (noyes(g("HoldPkg was found in target list. Do you want to continue?"))
            == false)) then
        retval = true
        return removecleanup()
    end

    if (config.flags["recurse"] or config.flags["cascade"]) then
        local pkglist = alpm.trans_get_remove()
        display_targets(pkglist, false)
        printf("\n")

        if (yesno(g("Do you want to remove these packages?")) == false) then
            retval = 1
            return removecleanup()
        end
    end

    if (alpm.trans_commit({}) == -1) then
        eprintf("LOG_ERROR", g("failed to commit transaction (%s)\n"), alpm.strerrorlast())
        retval = 1
        return removecleanup()
    end
    return removecleanup()
end

function main(targets)
    local result = clyde_remove(targets)
    if (not result) then
        result = 1
    end
    return result
end
