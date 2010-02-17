module(..., package.seeall)
local lfs = require "lfs"
local alpm = require "lualpm"
local C = colorize
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
local pm_targets = pm_targets
local community = community
local dump_pkg_changelog = packages.dump_pkg_changelog
local dump_pkg_files = packages.dump_pkg_files
local extra = extra
local core = core

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
                break
            else
                localfile = true
            end
        end
        if (file and file.mode == "directory") then
            eprintf("LOG_ERROR", g("cannot determine ownership of a directory\n"))
            ret = ret + 1
        else
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
                ret = ret + 1
            end
        end
    end
    return ret
end

local function query_search(targets)
    local dbcolors = {
        extra = C.greb;
        core = C.redb;
        community = C.magb;
    }
    local searchlist
    local freelist
    local packages = {}
    local repos = alpm.option_get_syncdbs()

    for i, repo in ipairs(repos) do
        local pkgcache = repo:db_get_pkgcache()
        for l, pkg in ipairs(pkgcache) do
            packages[pkg:pkg_get_name()] = repo:db_get_name()
        end
    end

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
        local name = pkg:pkg_get_name()
        if (not config.quiet) then
            if packages[name] then
                local dbcolor = dbcolors[packages[name]] or C.magb
                printf("%s%s %s", dbcolor(packages[name].."/"), C.bright(name), C.greb(pkg:pkg_get_version()))
            else
                printf("%s%s %s", C.yelb("local/"), C.bright(name), C.greb(pkg:pkg_get_version()))
            end
        else
            printf("%s", C.bright(name))
        end

        if (not config.quiet and config.showsize) then
            local mbsize = pkg:pkg_get_size() / (1024 * 1024)

            printf(" [%.2f MB]", mbsize)
        end

        if (not config.quiet) then
            local grp = pkg:pkg_get_groups()
            if (next(grp)) then
                local size = #grp
                printf(C.blub.." (")
                for i, group in ipairs(grp) do
                    printf("%s", group)
                    if i < size then
                        printf(" ")
                    end
                end
                printf(")"..C.reset)
            end

            printf("\n    ")
            indentprint(C.italic(pkg:pkg_get_desc()), 3)
        end
        print()
    end

    if (freelist) then
        searchlist = nil
    end
    return 0
end

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

local function check(pkg)
    local errors = 0
    local pkgname = pkg:pkg_get_name()
    local files = pkg:pkg_get_files()
    for i, filepath in ipairs(files) do
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

local dbcolors = {
        extra = C.greb;
        core = C.redb;
        community = C.magb;
    }

local packagetbl = {}
local repos = {}

local function display(pkg)
    if (not next(packagetbl)) then

        repos = alpm.option_get_syncdbs()

        for i, repo in ipairs(repos) do
            local pkgcache = repo:db_get_pkgcache()
            for l, pkg in ipairs(pkgcache) do
                packagetbl[pkg:pkg_get_name()] = repo:db_get_name()
            end
        end
    end

    ret = 0
    if (config.op_q_info ~= 0) then
        if (config.op_q_isfile) then
           packages.dump_pkg_full(pkg, 0)
        else
            packages.dump_pkg_full(pkg, config.op_q_info)
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
        local name = pkg:pkg_get_name()
        if (not config.quiet) then
            local dbcolor = dbcolors[packagetbl[name]] or C.magb
            if packagetbl[name] then
                printf("%s%s %s\n", dbcolor(packagetbl[name].."/"), C.bright(name), C.greb(pkg:pkg_get_version(pkg)))
            else
                printf("%s%s %s\n", C.yelb("local/"), C.bright(name), C.greb(pkg:pkg_get_version(pkg)))
            end
        else
            printf("%s\n", C.bright(name))
        end
    end

    return ret
end

local function clyde_query(targets)
    local ret = 0
    local match = false
    local pkg, value

    if (config.op_q_search) then
        ret = query_search(targets)
        return ret
    end

    if (config.group > 0) then
        ret = query_group(targets)
        return ret
    end

    if (config.op_q_foreign) then
        local sync_dbs = alpm.option_get_syncdbs()
        if (sync_dbs == nil or #sync_dbs == 0) then
            eprintf("LOG_ERROR", g("no usable package repositories configured.\n"))
            return 1
        end
    end

    if (not next(targets)) then
        if (config.op_q_isfile or config.op_q_owns) then
            eprintf("LOG_ERROR", g("no targets specified (use -h for help)\n"))
            return 1
        end

        local pkgcache = db_local:db_get_pkgcache()

        for k, pkg in ipairs(pkgcache) do
            if (filter(pkg)) then
                value = display(pkg)
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

        if (pkg == nil) then
            eprintf("LOG_ERROR", g("package \"%s\" not found\n"), strname)
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

function main(targets)
    local result = clyde_query(targets)
    if (not result) then
        result = 1
    end

    return result
end
