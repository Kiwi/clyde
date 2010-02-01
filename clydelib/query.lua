module(..., package.seeall)
local lfs = require "lfs"
local alpm = require "lualpm"
local util = require "clydelib.util"
local utilcore = require "clydelib.utilcore"
local packages = require "clydelib.packages"
local printf = util.printf
local basename = util.basename
local cleanup = util.cleanup
local tblinsert = util.tblinsert
local realpath = util.realpath
local indentprint = util.indentprint
local lprintf = util.lprintf
local eprintf = util.eprintf
local g = utilcore.gettext
local access = utilcore.access
local strerror = utilcore.strerror
--local strsplit = util.strsplit
local pm_targets = pm_targets
local community = community
local dump_pkg_changelog = packages.dump_pkg_changelog
local dump_pkg_files = packages.dump_pkg_files
local extra = extra
local core = core
--[[
local function query_fileowner(targets)
    local ret = 0
--printf(realpath("clyde"))
    for i, filepath in ipairs(targets) do
        local found = false
        local file, err = lfs.attributes(filepath)

        if (not file) then
            printf("error: failed to read file '%s': %s\n", filepath, err)
            break
        end

        if (file and file.mode == "directory") then
            printf("error: cannot determine ownership of a directory\n")
            ret = ret + 1
        else
        --local bname = basename(filepath)

        --local root = alpm.option_get_root()

        local pkgcache = db_local:db_get_pkgcache()
        filepath = filepath:gsub("^/","")

            for i, pkg in ipairs(pkgcache) do
                if found then break end
                files = pkg:pkg_get_files()
                for i, pkgfile in ipairs(files) do
                    if found then break end
                    if (filepath == pkgfile) then
                        if (not config.quiet) then
                            printf("/%s is owned by %s %s\n", filepath, pkg:pkg_get_name(), pkg:pkg_get_version())
                        else
                            printf("%s\n", pkg:pkg_get_name())
                        end
                        found = true
                    end
                end
            end
            if (not found) then
                printf("No package owns %s\n", filepath)
                ret = ret + 1
            end
        end
    end
    return ret
end
--]]
---[[
local function query_fileowner(targets)
    local ret = 0
    local found = 0
    local localfile = false
    local allfiles = {}
    local gsubtargets = {}
    for i, filepath in ipairs(targets) do
        local filepth = filepath:gsub("^/","")
        gsubtargets[i] = filepth
    end
    local pkgcache = db_local:db_get_pkgcache()
    for i, pkg in ipairs(pkgcache) do
        local thispkg = {}
        local files = pkg:pkg_get_files()
        local data = {['ver'] = pkg:pkg_get_version(), ['name'] = pkg:pkg_get_name()}
        for i, file in ipairs(files) do
            allfiles[file] = data
            thispkg[file] = true
        end
        local bfound = false
        for i, filepath in ipairs(gsubtargets) do
            if thispkg[filepath] then
                found = found + 1
                bfound = true
            end
            if bfound then
                break
                --a = a + 1
                --print("could have broke".. a)
            end
        end
        if (found == #targets) then break end

    end
    for i, filepath in ipairs(gsubtargets) do
        local file, err = lfs.attributes("/"..filepath)
        if (not file) then
            file, err = lfs.attributes(filepath)
            if (not file) then
                local result
                result, err = access("/"..filepath, "R_OK")

                eprintf("LOG_ERROR", g("failed to read file '%s': %s\n"), filepath, strerror(err))
            --printf("error: failed to read file '%s': %s\n", filepath, strerror(err))
                break
            else
                localfile = true
            end
        end
        if (file and file.mode == "directory") then
            eprintf("LOG_ERROR", g("cannot determine ownership of a directory\n"))
            --printf("error: cannot determine ownership of a directory\n")
            ret = ret + 1
        else
            --filepath = filepath:gsub("^/","")
            local filepathfull
            if localfile then
                filepathfull = lfs.currentdir():gsub("^/", "") .. "/" .. filepath
            else
                filepathfull = filepath
            end
            if allfiles[filepathfull] then
                if (not config.quiet) then
                    if (not localfile) then
                        filepath = "/"..filepath
                    end
                    printf(g("%s is owned by %s %s\n"), filepath, allfiles[filepathfull].name, allfiles[filepathfull].ver)
                else
                    printf("%s\n", allfiles[filepathfull].name)
                end
            else
                if (not localfile) then
                    filepath = "/"..filepath
                end

                eprintf("LOG_ERROR", g("No package owns %s\n"), filepath)
                --printf("error: No package owns /%s\n", filepath)
                ret = ret + 1
            end
        end
    end
    return ret
end

--]]
local function query_search(targets)
    local searchlist
    local freelist
--    local grp

    if (targets and next(targets)) then
        searchlist = db_local:db_search(targets)
        freelist = true
    else
        searchlist = db_local:db_get_pkgcache()
        freelist = false
    end

    if (not searchlist or (searchlist and not next(searchlist))) then
        return 1
    end

    for i, pkg in ipairs(searchlist) do
        if (not config.quiet) then
            printf("local/%s %s", pkg:pkg_get_name(), pkg:pkg_get_version())
        else
            printf("%s", pkg:pkg_get_name())
        end

        if (not config.quiet and config.showsize) then
            local mbsize = pkg:pkg_get_size() / (1024 * 1024)

            printf(" [%.2f MB]", mbsize)
        end

        if (not config.quiet) then
            local grp = pkg:pkg_get_groups()
            if (next(grp)) then
                local size = #grp
                printf(" (")
                for i, group in ipairs(grp) do
                    printf("%s", group)
                    if i < size then
                        printf(" ")
                    end
                end
                printf(")")
            end

            printf("\n    ")
            indentprint(pkg:pkg_get_desc(), 3)
        end
        print()
    end

    if (freelist) then
        searchlist = nil
    end
    return 0
end

--query group
local function query_group(targets)
    local ret = 0

    if (not next(targets)) then
        local cache = db_local:db_get_grpcache()
        for i, grp in ipairs(cache) do

            local grpname = grp:grp_get_name()
            local packages = grp:grp_get_pkgs()

            for j, package in ipairs(packages) do
                printf("%s %s\n", grpname, package:pkg_get_name())
            end
        end
    else
        for i, target in ipairs(targets) do
            local grpname = target
            grp = db_local:db_readgrp(grpname)
            if (grp) then
                local packages = grp:grp_get_pkgs()
                for j, package in ipairs(packages) do
                    if (not config.quiet) then
                        printf("%s %s\n", grpname, package:pkg_get_name())
                    else
                        printf("%s\n", package:pkg_get_name())
                    end
                end
            else
                eprintf("LOG_ERROR", g("group \"%s\" was not found\n"), grpname)
                --printf("error: group \"%s\" was not found\n", grpname)
            end
        end
    end
    return ret
end



local function is_foreign(pkg)
    local pkgname = pkg:pkg_get_name()
    local sync_dbs = alpm.option_get_syncdbs()

    local match = false

    for i, db in ipairs(sync_dbs) do
        local findpkg = db:db_get_pkg(pkgname)
        if (findpkg) then
            match = true
            break
        end
    end
    if (not match) then
        return true
    end
    return false
end

-- is unrequired
local function is_unrequired(pkg)
    local requiredby = pkg:pkg_compute_requiredby()
    if (not next(requiredby)) then
        return true
    else
        return false
    end
end

local function filter(pkg)
    if (config.op_q_explicit and pkg:pkg_get_reason() ~= "P_R_EXPLICIT") then
        return false
    end
    if (config.op_q_deps and pkg:pkg_get_reason() ~= "P_R_DEPEND") then
        return false
    end
    if (config.op_q_foreign and not is_foreign(pkg)) then
        return false
    end
    if (config.op_q_unrequired and not is_unrequired(pkg)) then
        return false
    end
    local syncdbs = alpm.option_get_syncdbs()
    if (config.op_q_upgrade and alpm.sync_newversion(pkg, syncdbs) == nil) then
        return false
    end

    return true
end


--check
local function check(pkg)
--    local root = alpm.option_get_root()
--    local rootlen = #root
--    local allfiles = 0
    local errors = 0

    local pkgname = pkg:pkg_get_name()
    local files = pkg:pkg_get_files()
    for i, filepath in ipairs(files) do
        --local file, err = lfs.symlinkattributes("/"..path)
        local file, err = utilcore.lstat("/"..filepath)
        if (file ~= 0) then
            if (config.quiet) then
                printf("%s %s\n", pkgname, "/"..filepath)
            else
                local result
                result, err = access("/"..filepath, "R_OK")
                printf("warning: %s: %s (%s)\n", pkgname, "/"..filepath, strerror(err))
            end
            errors = errors + 1
        end
    end

    if (not config.quiet) then
        printf(g("%s: %d total files, %d missing file(s)\n"), pkgname, #files, errors)
    end

    if (errors ~= 0) then
        return true
    else
        return false
    end
end

--display
local function display(pkg)
    ret = 0
    if (config.op_q_info ~= 0) then
        if (config.op_q_isfile) then
           packages.dump_pkg_full(pkg, 0)
        else
            packages.dump_pkg_full(pkg, config.op_q_info)
--            dump_pkg_files(pkg)
        end
    end
    if (config.op_q_list) then
        dump_pkg_files(pkg, config.quiet)
    end
    if (config.op_q_changelog) then
       dump_pkg_changelog(pkg)
    end
    if (config.op_q_check) then
        ret = check(pkg)
    end
    if (config.op_q_info == 0 and not config.op_q_list
            and not config.op_q_changelog and not config.op_q_check) then
        if (not config.quiet) then
            printf("%s %s\n", pkg:pkg_get_name(), pkg:pkg_get_version(pkg))
        else
            printf("%s\n", pkg:pkg_get_name())
        end
    end

    return ret
end











--community = alpm.db_register_sync("extra")
--community = community
--query_fileowner(pm_targets)
--query_search(pm_targets)
local function clyde_query(targets)
    local ret = 0
    local match = false
    local pkg, value

    if (config.op_q_search) then
--        print("search")
        ret = query_search(targets)
        return ret
    end

    if (config.group > 0) then
--        print("group")
        ret = query_group(targets)
        return ret
    end

    if (config.op_q_foreign) then
--        print("foreign")
        local sync_dbs = alpm.option_get_syncdbs()
--        for k, v in pairs(sync_dbs) do print (k, v) end
        if (sync_dbs == nil or #sync_dbs == 0) then
            eprintf("LOG_ERROR", g("no usable package repositories configured.\n"))
            --printf("error: no usable package repositories configured.\n")
            return 1
        end
    end

    if (not next(targets)) then
--        print("no targets")
        if (config.op_q_isfile or config.op_q_owns) then
            eprintf("LOG_ERROR", g("no targets specified (use -h for help)\n"))
            --printf("error: no targets specified (use -h for help)\n")
            return 1
        end

        local pkgcache = db_local:db_get_pkgcache()

        for k, pkg in ipairs(pkgcache) do
--            filter(pkg)
           -- printf("OHHHII")
            --print("filter")
            --print("filter")
            if (filter(pkg)) then
                value = display(pkg)
               -- display(pkg)
                if (value ~= 0) then
                    ret = 1
                end
                match = 1
            end
        end
        if (not match) then
            ret = 1
        end
        return ret
    end

    if (config.op_q_owns) then
        ret = query_fileowner(targets)
        return ret
    end

    for i, strname in ipairs(targets) do
        local cont = true
        if (config.op_q_isfile) then
            pkg = alpm.pkg_load(strname, true)
        else
            pkg = db_local:db_get_pkg(strname)
        end

        --filter(pkg)
        if (pkg == nil) then
            eprintf("LOG_ERROR", g("package \"%s\" not found\n"), strname)
            --printf("error: package \"%s\" not found\n", strname)
            ret = 1
            cont = false
        end

        if (cont and filter(pkg) ) then
            value = display(pkg)

            if (not value) then
                ret = 1
            end
            match = true
        end

        if (config.op_q_isfile) then
            pkg:pkg_free()
            pkg = nil
        end
    end

    if (not match) then
        ret = 1
    end

    return ret
end
--for k, v in pairs(pm_targets) do print(k, v) end
--[[
local result, err = pcall(clyde_query, pm_targets)

if (not result) then
    if (err:match("interrupted!")) then
        printf("\nInterrupt signal received\n\n")
    else
        printf(err)
    end
end
--]]


--clyde_query(pm_targets)
function main(targets)
    local result, err = pcall(clyde_query, targets)
--local err = ""
    if (not result) then
        if (err:match("interrupted!")) then
            printf("\nInterrupt signal received\n\n")
            cleanup(result)
        else
            --printf("%s\n", err)
        end
    end
end










--print("HI")
--[[
local tbl = alpm.option_get_syncdbs() --for k,v in ipairs(tbl) do print(k, v:db_get_name()) end
--print("HI")
for k, v in pairs(tbl) do print(k, v:db_get_name()) end
--print("HI")
local  t, hmm, hmm1 = alpm.sync_newversion( tbl)
--for k, v in pairs(t) do print(k, v:db_get_name()) end
--print("HI")
print(#tbl, #t)
print(hmm, hmm1)
for k, v in ipairs(t) do print(k," OH HI",  v:db_get_name()) end
--hurr(tbl)
--]]
