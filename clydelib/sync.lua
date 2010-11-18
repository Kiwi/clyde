module(..., package.seeall)
local yajl = require "yajl"
local zlib = require "zlib"
local lfs = require "lfs"
local alpm = require "lualpm"
local C = colorize
local util = require "clydelib.util"
local utilcore = require "clydelib.utilcore"
local packages = require "clydelib.packages"
local aur = require "clydelib.aur"
local upgrade = require "clydelib.upgrade"
local callback = require "clydelib.callback"
local rmrf = util.rmrf
local makepath = util.makepath
local eprintf = util.eprintf
local lprintf = util.lprintf
local printf = util.printf
local basename = util.basename
local cleanup = util.cleanup
local tblinsert = util.tblinsert
local realpath = util.realpath
local indentprint = util.indentprint
local list_display = util.list_display
local display_alpm_targets = util.display_alpm_targets
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
local getbasharray = util.getbasharray
local getbasharrayuser = util.getbasharrayuser
local getpkgbuildarray = util.getpkgbuildarray
local access = utilcore.access
local strerror = utilcore.strerror
local g = utilcore.gettext
local strsplit = util.strsplit
local pm_targets = pm_targets
local community = community
local dump_pkg_changelog = packages.dump_pkg_changelog
local dump_pkg_files = packages.dump_pkg_files
local dump_pkg_sync = packages.dump_pkg_sync

local tblsort = table.sort
local tblconcat = table.concat
require "socket"
local http = require "socket.http"
local url = require "socket.url"
local ltn12 = require "ltn12"
local aururl = "https://aur.archlinux.org:443/rpc.php?"
local aurmethod = {
    ['search'] = "type=search&";
    ['info'] = "type=info&";
    ['msearch'] = "type=msearch&";
}

yajl.to_value = function (string)
    local result
    local stack = {
        function(val) result = val end
    }
    local obj_key
    local events = {
        value = function(_, val)
            stack[#stack](val)
        end,
        open_array = function()
            local arr = {}
            local idx = 1
            stack[#stack](arr)
            table.insert(stack, function(val)
                    arr[idx] = val
                    idx = idx + 1
                    end)
        end,
        open_object = function()
            local obj = {}
            stack[#stack](obj)
            table.insert(stack, function(val)
                    obj[obj_key] = val
                    end)
        end,
        object_key = function(_, val)
            obj_key = val
        end,
        close = function()
            stack[#stack] = nil
        end,
    }

    yajl.parser({ events = events })(string)
    return result
end

local function sortaur(tbl)
    local stbl = {}
    local i = 1
    for k, v in pairs(tbl) do
        stbl[i] = v
        i = i + 1
    end
    tblsort(stbl, function(a, b)
        return tbl[a.name].name < tbl[b.name].name end)
    return stbl
end

local function prune(tbl, terms)
    for k, t in pairs(tbl) do
        for i, term in ipairs(terms) do
            if (not t.description:lower():find(term:lower(), 1, true) and not t.name:lower():find(term:lower(), 1, true)) then
                tbl[k] = nil
            end
        end
    end
end

local function sync_cleandb(dbpath, keep_used)
    local dir = lfs.chdir(dbpath)
    local found = false
    if (not dir) then
        eprintf("LOG_ERROR", g("could not access database directory\n"))
        return 1
    end
    for file in lfs.dir(dbpath) do
        found = false
        repeat
        if (file == "." or file == "..") then
            break
        end
        if (file == "sync" or file == "local") then
            break
        end
        local path = lfs.currentdir().."/"..file
        local attr = lfs.attributes(path)
        assert(type(attr) == "table")
        if (attr.mode ~= "directory") then
            break
        end

        if (keep_used) then
            local syncdbs = alpm.option_get_syncdbs()
            for i, db in ipairs(syncdbs) do
            repeat
                if (db:db_get_name() == file) then
                    found = true
                    break
                end
            until 1
            end
        end

        if (not found) then
            if (not yesno(g("Do you want to remove %s?"), path)) then
                break
            end

            if (rmrf(path)) then
                eprintf("LOG_ERROR", g("could not remove directory\n"))
                return 1
            end
        end
        until 1
    end

    return 0
end

local function sync_cleandb_all()
    local dbpath = alpm.option_get_dbpath()
    printf(g("Database directory: %s\n"), dbpath)
    if (not yesno(g("Do you want to remove unused repositories?"))) then
        return 0
    end

    sync_cleandb(dbpath, false)

    sync_cleandb(dbpath.."sync/", true)

    printf(g("Database directory cleaned up\n"))

    return 0
end

local function sync_cleancache(level)
    local cachedirs = alpm.option_get_cachedirs()
    local cachedir = cachedirs[1]

    if (level == 1) then
        printf(g("Cache directory: %s\n"), cachedir)
        if (config.cleanmethod == "CLEAN_KEEPINST") then
            if (not yesno(g("Do you want to remove uninstalled packages from cache?"))) then
                return 0
            end
        elseif (config.cleanmethod == "CLEAN_KEEPCUR") then
            if (not yesno(g("Do you want to remove outdated packages from cache?"))) then
                return 0
            end
        else
            return 1
        end
        printf(g("removing old packages from cache...\n"))

        local dir = lfs.chdir(cachedir)
        if (not dir) then
            eprintf("LOG_ERROR", g("could not access cache directory"))
            return 1
        end

        for file in lfs.dir(cachedir) do
            local delete = true
            local sync_dbs = alpm.option_get_syncdbs()
            repeat
            if (file == "." or file == "..") then
                break
            end
            local path = cachedir..file
            local localpkg, err = alpm.pkg_load(path, false)
            if (err ~= 0 or not localpkg) then
                if (yesno(g("File %s does not seem to be a valid package, remove it?"), path)) then
                    os.remove(path)
                end
                break
            end

            if (config.cleanmethod == "CLEAN_KEEPINST") then
                local pkg = db_local:db_get_pkg(localpkg:pkg_get_name())
                if (pkg and alpm.pkg_vercmp(localpkg:pkg_get_version(), pkg:pkg_get_version()) == 0) then
                    delete = false
                end
            elseif (config.cleanmethod == "CLEAN_KEEPCUR") then
                for i, db in ipairs(sync_dbs) do
                    repeat
                        local pkg = db:db_get_pkg(localpkg:pkg_get_name())
                        if (pkg and alpm.pkg_vercmp(localpkg:pkg_get_version(), pkg:pkg_get_version()) == 0) then
                            delete = false
                            break
                        end
                    until 1
                end
            else
                delete = false
            end

            localpkg = nil

            if (delete) then
                os.remove(path)
            end
            until 1
        end
    else
        printf(g("Cache directory: %s\n"), cachedir)
        if (not noyes(g("Do you want to remove ALL files from cache?"))) then
            return 0
        end
        printf(g("removing all files from cache...\n"))

        if (rmrf(cachedir)) then
            eprintf("LOG_ERROR", g("could not remove cache directory\n"))
            return 1
        end

        if (makepath(cachedir)) then
            eprintf("LOG_ERROR", g("could not create new cache directory\n"))
            return 1
        end
    end

    return 0
end

local function sync_synctree(level, syncs)
    local success = 0
    local ret

    if (trans_init(config.flags) == -1) then
        return 0
    end

    for i, db in ipairs(syncs) do
        local val = level >=2
        ret = db:db_update(val)
        if (ret < 0) then
            eprintf("LOG_ERROR", g("failed to update %s (%s)\n"),
                db:db_get_name(), alpm.strerrorlast())
        elseif (ret == 1) then
            printf(g(" %s is up to date\n"), db:db_get_name())
            success = 1
        else
            success = 1
        end
    end

    if (trans_release() == -1) then
        return 0
    end

    if (success == 0) then
        eprintf("LOG_ERROR", g("failed to synchronize any database\n"))
    end
    return success > 0 and 1 or 0
end

function sync_search(syncs, targets, shownumbers, install)
    local dbcolors = {
        extra = C.greb;
        core = C.redb;
        community = C.magb;
        testing = C.yelb;
    }
    local yelbold = C.yel..C.onblab..C.reverse
    local found = false
    local ret
    local localdb = alpm.option_get_localdb()
    local pkgcount = 0
    local pkgnames = {}
    local function numerize(count)
        if (shownumbers) then
            return yelbold..count..C.reset.." "
        else
            return ""
        end
    end

    local packages = {}
    local pkgcache = localdb:db_get_pkgcache()
    for i, pkg in ipairs(pkgcache) do
        packages[pkg:pkg_get_name()] = true
    end

    if (not config.op_s_search_aur_only) then
    for i, db in ipairs(syncs) do
        repeat
            if (next(targets)) then
                ret = db:db_search(targets)
            else
                ret = db:db_get_pkgcache()
            end

            if (not next(ret)) then
                break
            else
                found = true
            end



            for i, pkg in ipairs(ret) do
                pkgcount = pkgcount + 1
                pkgnames[pkgcount] = pkg:pkg_get_name()
                --TODO: show versions if different
                if (not config.quiet) then
                    local dbcolor = dbcolors[db:db_get_name()] or C.magb
                   -- if (localdb:db_get_pkg(pkg:pkg_get_name())) then
                   if (packages[pkg:pkg_get_name()]) then
                        printf("%s%s%s %s %s", numerize(pkgcount), dbcolor(db:db_get_name().."/"), C.bright(pkg:pkg_get_name()), C.greb(pkg:pkg_get_version()), yelbold.."[installed]"..C.reset)
                    else
                        printf("%s%s%s %s", numerize(pkgcount), dbcolor(db:db_get_name().."/"), C.bright(pkg:pkg_get_name()), C.greb(pkg:pkg_get_version()))
                    end
                else
                    printf("%s", C.bright(pkg:pkg_get_name()))
                end

                if (not config.quiet and config.showsize) then
                    local mbsize = pkg:pkg_get_size() / (1024 * 1024)

                    printf(" [%.2f MB]", mbsize)
                end

                if (not config.quiet) then
                    local grp = pkg:pkg_get_groups()
                    if (next(grp)) then
                        printf(C.blub.." (")
                        for i, group in ipairs(grp) do
                            printf("%s", group)
                            if (i < #grp) then
                                printf(" ")
                            end
                        end
                        printf(")"..C.reset)
                    end

                    printf("\n    ")
                    indentprint(C.italic(pkg:pkg_get_desc()), 3)
                end
                printf("\n")
            end

        until 1
    end
    end

    if (next(targets) and (not config.op_s_search_repos_only)) then
        if (#targets[1] < 2) then
            lprintf("LOG_WARNING", "First query arg too small to search AUR\n")
            return (not found)
        end

        local pattern
        -- the following gets rid of some common regular expressions
        if ( targets[1]:find("^^") or targets[1]:find("$$") ) then
            lprintf("LOG_DEBUG", "regex detected\n")

            -- escape other regexp special chars so they are taken literally
            pattern = targets[1]:gsub("([().%+*?[-])", "%%%1")
            targets[1] = targets[1]:gsub("^^", "")
            targets[1] = targets[1]:gsub("$$", "")
        end
        local searchurl = aururl..aurmethod.search.."arg="..url.escape(targets[1])
        local aurresults = aur.getgzip(searchurl)
        if (not aurresults) then
            return (not found)
        end
        local jsonresults = yajl.to_value(aurresults) or {}

        local aurpkgs = {}
        if (type(jsonresults.results) ~= "table") then
            jsonresults.results = {}
        end
        for k, v in pairs( jsonresults.results ) do
            if ( not pattern or v.Name:find(pattern)
                             or v.Description:find(pattern) ) then
                aurpkgs[ v.Name ] = { name        = v.Name,
                                      version     = v.Version,
                                      description = v.Description,
                                      votes       = v.NumVotes }
            end
        end
        prune(aurpkgs, targets)
        local sortedpkgs = sortaur(aurpkgs)
        if (next(sortedpkgs)) then
            found = true
        end
        for i, pkg in ipairs(sortedpkgs) do
            pkgcount = pkgcount + 1
            pkgnames[pkgcount] = pkg.name
            --TODO: same as above
            if (not config.quiet) then
                if (localdb:db_get_pkg(pkg.name)) then
                    printf("%s%s%s %s %s %s\n    ", numerize(pkgcount), C.magb("aur/"), C.bright(pkg.name), C.greb(pkg.version), yelbold.."[installed]"..C.reset, yelbold.."("..pkg.votes..")"..C.reset)
                else
                    printf("%s%s%s %s %s\n    ", numerize(pkgcount), C.magb("aur/"), C.bright(pkg.name), C.greb(pkg.version), yelbold.."("..pkg.votes..")"..C.reset)
                end
                indentprint(C.italic(pkg.description), 3)
                printf("\n")
            else
                printf("%s\n", C.bright(pkg.name))
            end
        end
    end

    if (shownumbers and found) then
        bars = C.yelb("==>")
        printf("%s %s\n%s %s\n",
               bars,
               C.bright("Enter #'s (separated by blanks) of packages "
                        .. "to be installed"),
               bars, C.bright(("-"):rep(60)), bars )

        local function ask_package_nums ( maxnum )
            io.write( bars .. " " )
            local nums_choice = io.read()

            if nums_choice == "" then return nil end

            input_numbers = {}
            for num in nums_choice:gmatch( "(%S+)" ) do
                if num:match( "%D" ) then
                    error( "Invalid input", 0 )
                else
                    num = tonumber( num )
                    if ( num < 1 or num > maxnum ) then
                        error( "Out of range", 0 )
                    end
                    table.insert( input_numbers, num )
                end
            end

            return input_numbers
        end

        local max = table.maxn( pkgnames )
        while ( true ) do
            local success, answer
                = pcall( ask_package_nums, max )

            if ( success ) then
                if ( answer ) then
                    for i, num in ipairs( answer ) do
                        table.insert( install, pkgnames[num] )
                    end
                else
                    print( "Aborting." )
                end
                break
            elseif ( answer == "Invalid input" ) then
                print( "Please enter only digits and/or whitespace." )
            elseif ( answer == "Out of range" ) then
                print( "Package numbers must be between 1 and "
                       .. max .. "." )
            else
                error( answer, 0 )
            end
        end
    end
    if found then
        return 0
    else
        return 1
    end
--    return (not found)
end

local function sync_group(level, syncs, targets)
    if (targets and next(targets)) then
        for i, grpname in ipairs(targets) do
            for i, db in ipairs(syncs) do
                local grp = db:db_readgrp(grpname)
                if (grp) then
                    local pkgs = grp:grp_get_pkgs()
                    for i, pkg in ipairs(pkgs) do
                        if (not config.quiet) then
                            printf("%s %s\n", grpname, pkg:pkg_get_name())
                        else
                            printf("%s\n", pkg:pkg_get_name())
                        end
                    end
                end
            end
        end
    else
        for i, db in ipairs(syncs) do
            local grpcache = db:db_get_grpcache()
            for i, grp in ipairs(grpcache) do
                local grpname = grp:grp_get_name()

                if (level > 1) then
                    local grppkgs = grp:grp_get_pkgs()
                    for i, pkg in ipairs(grppkgs) do
                        printf("%s %s\n", grpname, pkg:pkg_get_name())
                    end
                else
                    printf("%s\n", grpname)
                end
            end
        end
    end
    return 0
end

local function sync_info(syncs, targets)
    local ret = 0
    if (next(targets)) then
        for i, target in ipairs(targets) do
            local foundpkg = false
            local slash = target:find("/")
            local pkgstr
            local repo
            local dbmatch = false
            local db

            if (slash) then
                pkgstr = target:sub(slash + 1)
                repo = target:sub(1, slash - 1)
            end

            if (slash and repo ~= "aur") then
                for i, database in ipairs(syncs) do
                    if (repo == database:db_get_name()) then
                        dbmatch = true
                        db = database
                        break
                    end
                end

                if (not dbmatch) then
                    eprintf("LOG_ERROR", g("repository '%s' does not exist\n"), repo)
                    return 1
                end

                local pkgcache = db:db_get_pkgcache()
                for i, pkg in ipairs(pkgcache) do
                    if (pkgstr == pkg:pkg_get_name()) then
                        dump_pkg_sync(pkg, db:db_get_name())
                        foundpkg = true
                        break
                    end
                end

                if (not foundpkg) then
                    eprintf("LOG_ERROR", g("package '%s' was not found in repository '%s'\n"), pkgstr, repo)
                    ret = ret + 1
                end
            elseif repo ~= "aur" then
                pkgstr = target
                for i, db in ipairs(syncs) do
                    local pkgcache = db:db_get_pkgcache()
                    for i, pkg in ipairs(pkgcache) do
                        if (pkg:pkg_get_name() == pkgstr) then
                            dump_pkg_sync(pkg, db:db_get_name())
                            foundpkg = true
                            break
                        end
                    end
                end
            end

            if (repo == "aur") then
                target = pkgstr
            end
            local infourl
            local inforesults
            local jsonresults = {results = {}}
            if (not foundpkg) then
                infourl = aururl..aurmethod.info.."arg="..url.escape(target)
                inforesults = aur.getgzip(infourl)
                if (not inforesults) then
                    return 1
                end
                jsonresults = yajl.to_value(inforesults) or {}

                if (type(jsonresults.results) ~= "table") then
                    jsonresults.results = {}
                end
            end

            if (jsonresults.results.Name)then
                packages.dump_pkg_sync_aur(target)
            elseif (not foundpkg and repo ~= "aur") then
                eprintf("LOG_ERROR", g("package '%s' was not found\n"), pkgstr)
                ret = ret + 1
            elseif (not foundpkg) then
                eprintf("LOG_ERROR", "package '%s' was not found in AUR\n", pkgstr)
                ret = ret + 1
            end
        end
    else
        for i, db in ipairs(syncs) do
            local pkgcache = db:db_get_pkgcache()
            for i, pkg in ipairs(pkgcache) do
                dump_pkg_sync(pkg, db:db_get_name())
            end
        end
    end
    return ret
end

local function sync_list(syncs, targets)
    ls = {}
    if (next(targets)) then
        for i, repo in ipairs(targets) do
            local db = nil

            for i, d in ipairs(syncs) do
                if (repo == d:db_get_name()) then
                    db = d
                    break
                end
            end

            if (db == nil) then
                eprintf("LOG_ERROR", g("repository \"%s\" was not found.\n"), repo)
                return 1
            end
            tblinsert(ls, db)
        end
    else
        ls = syncs
    end

    for i, db in ipairs(ls) do
        local pkgcache = db:db_get_pkgcache()
        for i, pkg in ipairs(pkgcache) do
            if (not config.quiet) then
                printf("%s %s %s\n", db:db_get_name(), pkg:pkg_get_name(),
                    pkg:pkg_get_version())
            else
                printf("%s\n", pkg:pkg_get_name())
            end
        end
    end

    return 0
end

local function syncfirst()
    local db = alpm.option_get_localdb()
    local res = {}
    for i, pkgname in ipairs(config.syncfirst) do
        repeat
            local pkg = db:db_get_pkg(pkgname)
            if (pkg == nil) then
                break
            end
            local package = alpm.sync_newversion(pkg, alpm.option_get_syncdbs())
            if (package) then
                tblinsert(res, pkgname)
            end
        until 1
    end
    return res
end

local function sync_aur_trans(targets)
    local retval = 0
    local found
    local transret
    local data = {}
    local aurpkgs = {}
    local sync_dbs = alpm.option_get_syncdbs()
    local function transcleanup()
        if (trans_release() == -1) then
            retval = 1
        end
        return retval
    end

    if (trans_init(config.flags) == -1) then
        return 1
    end

    if (config.op_s_upgrade > 0) then
        printf(g(C.blub("::")..C.bright(" Starting full system upgrade..\n")))
        alpm.logaction("starting full system upgrade\n")
        local op_s_upgrade = config.op_s_upgrade >= 2 and 1 or 0

        if (alpm.sync_sysupgrade(op_s_upgrade) == -1) then
            eprintf("LOG_ERROR", "%s\n", alpm.strerrorlast())
            retval = 1
            return transcleanup()
        end
    else
        for i, targ in ipairs(targets) do
            repeat
                if (alpm.sync_target(targ) == -1) then
                    found = false
                    if (alpm.pm_errno() == "P_E_TRANS_DUP_TARGET" or
                        alpm.pm_errno() == "P_E_PKG_IGNORED") then
                        lprintf("LOG_WARNING", g("skipping target: %s\n"), targ)
                        break
                    end
                    if (alpm.pm_errno() ~= "P_E_PKG_NOT_FOUND") then
                        eprintf("LOG_ERROR", "'%s': %s\n", targ, alpm.strerrorlast())
                        retval = 1
                        return transcleanup()
                    end
                    printf(g(C.blub("::")..C.bright(" %s package not found, searching for group...\n")), targ)
                    for i, db in ipairs(sync_dbs) do
                        local grp = db:db_readgrp(targ)
                        if (grp) then
                            found = true
                            printf(g(C.blub("::")..C.bright(" group %s (including ignored packages):\n")), targ)
                            local grppkgs = grp:grp_get_pkgs()
                            local pkgs = tblremovedupes(grppkgs)
                            local pkgnames = {}
                            for i, pkgname in ipairs(pkgs) do
                                tblinsert(pkgnames, pkgname:pkg_get_name())
                            end
                            list_display("   ", pkgnames)
                            if (yesno(g(C.yelb("::")..C.bright(" Install whole content?")))) then
                                for i, pkgname in ipairs(pkgnames) do
                                    tblinsert(targets, pkgname)
                                end
                            else
                                for i, pkgname in ipairs(pkgnames) do
                                    if (yesno(g(C.yelb("::")..C.bright(" Install %s from group %s?")), pkgname, targ)) then
                                        tblinsert(targets, pkgname)
                                    end
                                end
                            end
                            pkgnames = nil
                            pkgs = nil
                        end
                    end

                    if (not found) then
                        printf(C.blub("::")..C.bright(" %s group not found, searching AUR...\n"), targ)
                        local infourl = aururl..aurmethod.info.."arg="..url.escape(targ)
                        local inforesults = aur.getgzip(infourl)
                        if (not inforesults) then
                            return 1
                        end
                        local jsonresults =  yajl.to_value(inforesults) or {}

                        if (type(jsonresults.results) ~= "table") then
                            jsonresults.results = {}
                        end

                        if (jsonresults.results.Name) then
                            found = true
                            tblinsert(aurpkgs, targ)
                        end

                    end
--                    local pkgs = db_local:db_get_pkgcache()

                    if (not found) then
                        eprintf("LOG_ERROR", g("'%s': not found in sync db\n"), targ)
                        retval = 1
                        return transcleanup()
                    end
                end
            until 1
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
            end

        elseif (alpm.pm_errno() == "P_E_CONFLICTING_DEPS") then
            for i, conflict in ipairs(data) do
                printf(g(C.blub("::")..
                      C.bright(" %s: conflicts with %s (%s)\n")),
                    conflict:conflict_get_package1(),
                    conflict:conflict_get_package2(),
                    conflict:conflict_get_reason())
            end
        end
        retval = 1
        return transcleanup()
    end

    local packages = alpm.trans_get_add()
    --[[
    if (not next(packages) and not found) then
        printf(g(" local database is up to date\n"));
        return transcleanup()
    end
    --]]

    if (config.op_s_printuris) then
        for i, pkg in ipairs(packages) do
            local db = pkg:pkg_get_db()
            local dburl = db:db_get_url()
            if (dburl) then
                printf("%s/%s\n", dburl, pkg:pkg_get_filename())
            else
                eprintf("LOG_ERROR", g("no URL for package: %s\n"), pkg:pkg_get_name())
            end
        end
        return transcleanup()
    end

--    display_synctargets(packages)
    --TODO: write this function to pretty up install list, or something
--    display_aurtargets(aurpkgs)
--    if (next(aurpkgs)) then
--        printf(C.greb("\n==>")..C.bright(" Installing the following packages from AUR\n"))
--        list_display("", aurpkgs)
--        print(" (Plus dependencies)")
--    end
--    printf("\n")
--[[
    local confirm
    if (config.op_s_downloadonly) then
        confirm = yesno(C.yelb("==>")..C.bright(" Proceed with download?"))
    else
        confirm = yesno(C.yelb("==>")..C.bright(" Proceed with installation?"))
    end
    if (not confirm) then
        return transcleanup()
    end
    --]]
    data = {}
    transret, data = alpm.trans_commit(data)
    if (transret == -1) then
        eprintf("LOG_ERROR", g("failed to commit transaction (%s)\n"), alpm.strerrorlast())
        if (alpm.pm_errno() == "P_E_FILE_CONFLICTS") then
            for i, conflict in ipairs(data) do
                if (conflict:fileconflict_get_type() == "FILECONFLICT_TARGET") then
                    printf(g("%s exists in both '%s' and '%s'\n"),
                    conflict:fileconflict_get_file(),
                    conflict:fileconflict_get_target(),
                    conflict:fileconflict_get_ctarget())
                elseif (conflict:fileconflict_get_type() == "FILECONFLICT_FILESYSTEM") then
                    printf(g("%s: %s exists in filesystem\n"),
                        conflict:fileconflict_get_target(),
                        conflict:fileconflict_get_file())
                end
            end
        elseif (alpm.pm_errno() == "P_E_PKG_INVALID" or alpm.pm_errno() == "P_E_DLT_INVALID") then
            for i, filename in ipairs(data) do
                printf(g("%s is invalid or corrupted\n"), filename)
            end
        end

        printf(g(C.redb("==>")..C.bright("Errors occured, no packages were upgraded.\n")))
        retval = 1
        return transcleanup()
    end

    if (next(aurpkgs)) then
        transcleanup()
        return aur_install(aurpkgs)
    else
        return transcleanup()
    end
end

local function updateprovided(tbl)
    local db_local = alpm.option_get_localdb()
    local pkgcache = db_local:db_get_pkgcache()
    for i, pkg in ipairs(pkgcache) do
        tbl[pkg:pkg_get_name()] = pkg:pkg_get_version()
        local provides = pkg:pkg_get_provides()
        if (next(provides)) then
            for i, prov in ipairs(provides) do
                --TODO: Fix this so that it can handle versions properly
                local p = prov:match("(.+)=") or prov
                local v = prov:match("=(.+)") or nil
                tbl[p] = v or true
            end
        end
    end
end

local function download_extract(target, currentdir)
    local retmv, retex, ret
    local user = util.getbuilduser()
    local myuid = utilcore.geteuid()
    --local oldmask = utilcore.umask(tonumber("700", 8))
    local host = "aur.archlinux.org:443"
    aur.get(host, string.format("/packages/%s/%s.tar.gz", target, target), user)
    aur.dispatcher()
    --utilcore.umask(oldmask)
    if (not currentdir) then
        lfs.chdir("/tmp/clyde-"..user.."/"..target)
        os.execute("chmod 700 /tmp/clyde-"..user.."/"..target)
        os.execute("bsdtar -xf " .. target .. ".tar.gz &>/dev/null")
        if (myuid == 0) then
            os.execute("chown "..user..":users -R /tmp/clyde-"..user)
        end
    else
--        os.execute("mv /tmp/clyde-"..user.."/"..target.."/"..target..".tar.gz "..lfs.currentdir())
        retmv = os.execute(string.format("mv /tmp/clyde-%s/%s/%s.tar.gz %s &>/dev/null",
            user, target, target, lfs.currentdir()))
        retex = os.execute("bsdtar -xf "..target..".tar.gz &>/dev/null")
        if(retex == 0 and retmv == 0) then
            printf(C.greb("==>")..C.bright(" Extracted %s.tar.gz to current directory\n"), target)
        else
            lprintf("LOG_ERROR", "could not extract %s.tar.gz\n", target)
        end
    end
--    os.execute("chmod 700 -R /tmp/clyde/"..target)

end

local function customizepkg(target)
    local user = util.getbuilduser()
    lfs.chdir("/tmp/clyde-"..user.."/"..target.."/"..target)
    if (not config.noconfirm) then
        local editor
        if (not config.editor) then
            printf("No editor is set.\n")
            printf("What editor would you like to use? ")
            editor = io.read()
            if (editor == (" "):rep(#editor)) then
                printf("Defaulting to nano")
                editor = "nano"
            end
            printf("Using %s\n", editor)
            printf("To avoid this message in the future please create a config file or use the --editor command line option\n")
        else
            editor = config.editor
        end

        printf(C.blink..C.redb("    ( Unsupported package from AUR: Potentially dangerous! )"))
        printf("\n")

        repeat
            local response = yesno(C.yelb("==> ")..C.bright("Edit the PKGBUILD (highly recommended for security reasons)?"))
            if (response) then
                os.execute(editor.." PKGBUILD")
            end
        until not response
        local instfile = getbasharray("PKGBUILD", "install")
        if (instfile and #instfile > 0) then
            repeat
                local response = yesno(C.yelb("==> ")..C.bright("Edit "..instfile.." (highly recommended for security reasons)?"))
                if (response) then
                    os.execute(editor.." "..instfile)
                end
            until not response
        end
    end
end

local function makepkg(target, mkpkgopts)
    local user = util.getbuilduser()
    local result
    if (user ~= "root") then
        result = os.execute("su "..user.." -c 'makepkg -f' "..mkpkgopts)
    else
        printf(C.redb("==> ")..C.bright(C.onred("Running makepkg as root is a bad idea!")))
        printf("\n"..C.redb("==> ")..C.bright("To avoid this message please set BuildUser in clyde.conf\n"))
        local response = noyes(C.redb("==> ")..C.bright("Continue anyway?"))
        if (response)  then
            result = os.execute("makepkg -f --asroot "..mkpkgopts)
        else
            cleanup(1)
        end
    end
    if (result ~= 0) then
        eprintf("LOG_ERROR", "%s\n", "Build failed")
        cleanup(1)
    end
end

local function installpkgs(targets)
    if (type(targets) ~= "table") then
        targets = {targets}
    end
    local user = util.getbuilduser()
    local packagedir = getbasharrayuser("/etc/makepkg.conf", "PKGDEST", user)
            or "/tmp/clyde-"..user.."/"..target.."/"..target

    local pkgs = {}
    local toinstall = {}

    for i, target in ipairs(targets) do
        lfs.chdir(packagedir)
        for file in lfs.dir(lfs.currentdir()) do
            local pkg = file:match(".-%.pkg%.tar%.%az")
            if (pkg) then
                tblinsert(pkgs, pkg)
            end
        end
    end

    for i, pkg in ipairs(pkgs) do
        local loaded, err = alpm.pkg_load(pkg, false)
        local pkgver = loaded:pkg_get_version()
        local pacname = loaded:pkg_get_name()
        local found, indx = tblisin(toinstall, pacname)
        local istarget, _ = tblisin(targets, pacname)
        if (found and istarget) then
            local comp = alpm.pkg_vercmp(pkgver, toinstall[indx][pacname].ver)
            if comp == 1 then
                toinstall[indx][pacname].fname = pkg
            end
        elseif (istarget) then
            toinstall[#toinstall+1] = {}
            toinstall[#toinstall][pacname] = {['fname'] = pkg; ['ver'] = pkgver}
        end
    end

    for i, pkgtbl in ipairs(toinstall) do
        for l, pkg in pairs(pkgtbl) do
            toinstall[i] = pkg.fname
            break
        end
    end

    local ret = upgrade.main(toinstall)
    if (ret ~= 0) then
        cleanup(ret)
    end
end

local function getdepends(target, provided)
    local ret, ret2, ret3 = {}, {}, {}
    local sync_dbs = alpm.option_get_syncdbs()
    for i, db in ipairs(sync_dbs) do
        local package = db:db_get_pkg(target)
        if (package) then
            local depends = package:pkg_get_depends()
            for i, dep in ipairs(depends) do
                tblinsert(ret, dep:dep_compute_string())
            end
        end
    end
    local pkgbuildurl = string.format(
            "https://aur.archlinux.org:443/packages/%s/%s/PKGBUILD",
                target, target)
    local pkgbuild = aur.getgzip(pkgbuildurl)
    if not pkgbuild then
        return ret, {}, {}
    end
    local tmp = os.tmpname()
    local tmpfile = io.open(tmp, "w")
    tmpfile:write(pkgbuild)
    tmpfile:close()
    local carch = getbasharray("/etc/makepkg.conf", "CARCH")
    local depends = getpkgbuildarray(carch, tmp, "depends")
    local makedepends = getpkgbuildarray(carch, tmp, "makedepends")
    local optdepends = getpkgbuildarray(carch, tmp, "optdepends")
    os.remove(tmp)
    ret = strsplit(depends, " ") or {}
    ret2 = strsplit(makedepends, " ") or {}
    ret3 = strsplit(optdepends, " ") or {}
    return ret, ret2, ret3
end

local function getalldeps(targs, needs, needsdeps, caninstall, provided)
    local done = true
    local depends, makedepends
    for i, targ in ipairs(targs) do
        if (not tblisin(needs, targ) and((not tblisin(needsdeps, targ) and not tblisin(caninstall, targ)) or not provided[targ])) then
            tblinsert(needs, targ)
        end
        depends, makedepends = getdepends(targ, provided)
        local bcaninstall = true
        depends = tbljoin(depends, makedepends)
        for i, dep in ipairs(depends) do
        repeat
            if dep == "" then break end
            --TODO: Fix this so that it can handle versions properly
            local dep = dep:match("(.+)<") or dep:match("(.+)>") or dep:match("(.+)=") or dep
            depends[i] = dep
            if (not provided[dep] or tblisin(targs, dep))then
                if (not tblisin(needs, dep)) then
                    tblinsert(needs, dep)
                end
                bcaninstall = false
                if (not tblisin(needsdeps, targ) and not tblisin(caninstall, targ)) then
                    done = false
                end
            end
        until 1
        end
        if (bcaninstall) then
            if (not tblisin(caninstall, targ)) then
                tblinsert(caninstall, targ)
            end
        end
    end

    local needsdepstbl = tbldiff(needs, caninstall)
    for i, v in ipairs(needsdeps) do
        i = nil
    end
    for i, v in ipairs(needsdepstbl) do
        tblinsert(needsdeps, v)
    end

    if not done and next(depends) then
        getalldeps(needs, needs, needsdeps, caninstall, provided)
    end
end

local function removeflags(flag)
    local found = true
    local indx
    while found do
        found, indx = tblisin(config.flags, flag)
        if (found) then
            fastremove(config.flags, indx)
        end
    end
end

local function removetblentry(tbl, entry)
    local found = true
    local indx
    while found do
        found, indx = tblisin(tbl, entry)
        if (found) then
            fastremove(tbl, entry)
        end
    end
end

local function pacmaninstallable(target)
    local sync_dbs = alpm.option_get_syncdbs()
    for i, db in ipairs(sync_dbs) do
        local pkg = db:db_get_pkg(target)
        if (pkg) then
            return true
        end
    end
    return false
end

function getpkgbuild(targets)
    local provided = {}
    local needs = {}
    local caninstall = {}
    local needsdeps = {}
    local aurpkgs = {}
    updateprovided(provided)

    getalldeps(targets, needs, needsdeps, caninstall, provided)
    if (config.op_g_get_deps) then
        for i, pkg in ipairs(needs) do
            if (not pacmaninstallable(pkg)) then
           -- tblinsert(pacmanpkgs, pkg)
        --else
            --tblinsert(aurpkgs, pkg)
                download_extract(pkg, true)
            end
        end
    else
        for i, pkg in ipairs(targets) do
            if (not pacmaninstallable(pkg)) then
                download_extract(pkg, true)
            end
        end
    end
end

local function aur_install(targets)
    local mkpkgopts = table.concat(config.mkpkgopts)
    local provided = {}
    local needs = {}
    local caninstall = {}
    local needsdeps = {}
    updateprovided(provided)

--TODO use memonization to improve performance

    local memodepends = {}
    local memomakedepends = {}


    getalldeps(targets, needs, needsdeps, caninstall, provided)

    tflags = {}
    for flag, status in pairs( config.flags ) do
        if status then tflags[flag] = true end
    end

    local installed = 0
    local pacmanpkgs = {}
    local pacmanexplicit = {}
    local pacmandeps = {}
    local aurpkgs = {}
    local noconfirm = config.noconfirm
    for i, pkg in ipairs(needs) do
        if (pacmaninstallable(pkg)) then
            tblinsert(pacmanpkgs, pkg)
        else
            tblinsert(aurpkgs, pkg)
        end
    end

    for i, pkg in ipairs(pacmanpkgs) do
        if (tblisin(targets, pkg) and not tflags["alldeps"]
            or (tflags["allexplicit"] and not tflags["alldeps"])) then
            tblinsert(pacmanexplicit, pkg)
        else
            tblinsert(pacmandeps, pkg)
        end
    end

    config.noconfirm = true
    sync_aur_trans(pacmanexplicit)
    config.flags["alldeps"] = true
    sync_aur_trans(pacmandeps)
    config.flags["alldeps"] = false
    config.noconfirm = noconfirm

    local installedtbl = {}
    local needscount = #needs - #pacmanpkgs
    while (#installedtbl < needscount ) do
        repeat
        updateprovided(provided)
        caninstall, needs, needsdeps = {}, {}, {}
        getalldeps(targets, needs, needsdeps, caninstall, provided)

        for i, pkg in ipairs(aurpkgs) do
            if (tblisin(caninstall, pkg) and not tblisin(installedtbl, pkg)) then
                if (tblisin(targets, pkg) and not tflags["alldeps"])
                    or (tflags["allexplicit"] and not tflags["alldeps"]) then
                    download_extract(pkg)
                    customizepkg(pkg)
                    makepkg(pkg, mkpkgopts)
                    installpkgs(pkg)
                    installed = installed + 1
                    tblinsert(installedtbl, pkg)
                else
                    config.flags["alldeps"] = true
                    download_extract(pkg)
                    customizepkg(pkg)
                    makepkg(pkg, mkpkgopts)
                    installpkgs(pkg)
                    config.flags["alldeps"] = false
                    installed = installed + 1
                    tblinsert(installedtbl, pkg)
                end
            end
        end
        until 1
    end
end

local function trans_aurupgrade(targets)
    local pkgcache = db_local:db_get_pkgcache()
    local foreign = {}
    local possibleaurpkgs = {}
    local aurpkgs = {}
    local len
    for i, pkg in ipairs (pkgcache) do
        local name = pkg:pkg_get_name()
        if (not pacmaninstallable(name)) then
            tblinsert(foreign, {name = name; version = pkg:pkg_get_version()})
            possibleaurpkgs[name] = true
        end
    end

    printf(C.blub("::")..C.bright(" Synchronizing AUR database...\n"))
    local count = 0
    for i, pkg in ipairs(foreign) do
        local message = string.format(" aur%s%3.0f/%3.0f", (" "):rep(20), i, #foreign)
        printf("\r%s", message)
        len = #message
        repeat
        count = count + 1
        local infourl = aururl..aurmethod.info.."arg="..url.escape(pkg.name)
        local inforesults = aur.getgzip(infourl)
        callback.fill_progress(math.floor(count*100/#foreign), math.ceil(count*100/#foreign), util.getcols() - len)
        if (not inforesults) then
            break
        end

        local jsonresults = yajl.to_value(inforesults) or {}

        if (type(jsonresults.results == "table")) then
            if jsonresults.results.Name then
                if possibleaurpkgs[jsonresults.results.Name] then
                    aurpkgs[pkg.name] = jsonresults.results.Version
                    if (alpm.pkg_vercmp(jsonresults.results.Version, pkg.version) == 1) then
                        tblinsert(targets, pkg.name)
                    end
                end
            end
        end
        until 1
    end
end

local function sync_trans(targets)
    local retval = 0
    local found
    local transret
    local data = {}
    local aurpkgs = {}
    local sync_dbs = alpm.option_get_syncdbs()
    local function transcleanup()
        if (trans_release() == -1) then
            retval = 1
        end
        return retval
    end

    if (trans_init(config.flags) == -1) then
        return 1
    end

    if (config.op_s_upgrade > 0) then
        printf(g(C.blub("::")..C.bright(" Starting full system upgrade...\n")))
        alpm.logaction("starting full system upgrade\n")
        local op_s_upgrade = config.op_s_upgrade >= 2 and 1 or 0

        if (alpm.sync_sysupgrade(op_s_upgrade) == -1) then
            eprintf("LOG_ERROR", "%s\n", alpm.strerrorlast())
            retval = 1
            return transcleanup()
        end

        if (config.op_s_upgrade_aur) then
            config.op_s_upgrade = 0
            trans_aurupgrade(aurpkgs)
            targets = aurpkgs
        end
    else
        for i, targ in ipairs(targets) do
            repeat
                if (alpm.sync_target(targ) == -1) then
                    found = false
                    if (alpm.pm_errno() == "P_E_TRANS_DUP_TARGET" or
                        alpm.pm_errno() == "P_E_PKG_IGNORED") then
                        lprintf("LOG_WARNING", g("skipping target: %s\n"), targ)
                        break
                    end
                    if (alpm.pm_errno() ~= "P_E_PKG_NOT_FOUND") then
                        eprintf("LOG_ERROR", "'%s': %s\n", targ, alpm.strerrorlast())
                        retval = 1
                        return transcleanup()
                    end
                    printf(g(C.blub("::")..C.bright(" %s package not found, searching for group...\n")), targ)
                    for i, db in ipairs(sync_dbs) do
                        local grp = db:db_readgrp(targ)
                        if (grp) then
                            found = true
                            printf(g(C.blub("::")..C.bright(" group %s (including ignored packages):\n")), targ)
                            local grppkgs = grp:grp_get_pkgs()
                            local pkgs = tblremovedupes(grppkgs)
                            local pkgnames = {}
                            for i, pkgname in ipairs(pkgs) do
                                tblinsert(pkgnames, pkgname:pkg_get_name())
                            end
                            list_display("   ", pkgnames)
                            if (yesno(g(C.yelb("::")..C.bright(" Install whole content?")))) then
                                for i, pkgname in ipairs(pkgnames) do
                                    tblinsert(targets, pkgname)
                                end
                            else
                                for i, pkgname in ipairs(pkgnames) do
                                    if (yesno(g(C.yelb("::")..C.bright(" Install %s from group %s?")), pkgname, targ)) then
                                        tblinsert(targets, pkgname)
                                    end
                                end
                            end
                            pkgnames = nil
                            pkgs = nil
                        end
                    end

                    if (not found) then
                        printf(C.blub("::")..C.bright(" %s group not found, searching AUR...\n"), targ)
                        local infourl = aururl..aurmethod.info.."arg="..url.escape(targ)
                        local inforesults = aur.getgzip(infourl)
                        if (not inforesults) then
                            return 1
                        end
                        local jsonresults = yajl.to_value(inforesults) or {}

                        if (type(jsonresults.results) ~= "table") then
                            jsonresults.results = {}
                        end

                        if (jsonresults.results.Name) then
                            found = true
                            tblinsert(aurpkgs, targ)
                        end

                    end
--                    local pkgs = db_local:db_get_pkgcache()

                    if (not found) then
                        eprintf("LOG_ERROR", g("'%s': not found in sync db\n"), targ)
                        retval = 1
                        return transcleanup()
                    end
                end
            until 1
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
            end
        elseif (alpm.pm_errno() == "P_E_CONFLICTING_DEPS") then
            for i, conflict in ipairs(data) do
                printf(g(C.blub("::")..
                      C.bright(" %s: conflicts with %s (%s)\n")),
                    conflict:conflict_get_package1(),
                    conflict:conflict_get_package2(),
                    conflict:conflict_get_reason())
            end
        end
        retval = 1
        return transcleanup()
    end

    local packages = alpm.trans_get_add()
    if (not next(packages) and not found and not next(aurpkgs)) then
        printf(g(" local database is up to date\n"));
        return transcleanup()
    end

    if (config.op_s_printuris) then
        for i, pkg in ipairs(packages) do
            local db = pkg:pkg_get_db()
            local dburl = db:db_get_url()
            if (dburl) then
                printf("%s/%s\n", dburl, pkg:pkg_get_filename())
            else
                eprintf("LOG_ERROR", g("no URL for package: %s\n"), pkg:pkg_get_name())
            end
        end
        return transcleanup()
    end

    local needs, provided, needsdeps, caninstall, possibleaur =
        {}, {}, {}, {}, {}

    for i, pkg in ipairs(targets) do
        if (not pacmaninstallable(pkg)) then
            tblinsert(possibleaur, pkg)
        end
    end

    updateprovided(provided)
    getalldeps(possibleaur, needs, needsdeps, caninstall, provided)

    local needsdupe = tblstrdup(needs)

    for i, pkg in ipairs(needsdupe) do
        if (pacmaninstallable(pkg)) then
            for i, db in ipairs(sync_dbs) do
                local loaded = db:db_get_pkg(pkg)
                if (loaded and not tblisin(targets, loaded:pkg_get_name())) then
                    local inpackages = false
                    for i, package in ipairs(packages) do
                        if (package:pkg_get_name() == loaded:pkg_get_name()) then
                            inpackages = true
                            break
                        end
                    end
                    if (not inpackages) then
                        tblinsert(packages, loaded)
                        removetblentry(needs, pkg)
                    end
                end
            end
        end
    end

    if (next(aurpkgs)) then
        if (next(packages)) then
            printf(C.greb("\n==>")..C.bright(" Installing the following packages from repos\n"))
            util.display_targets(packages, true)
            --  what about packages that are to be removed?
        end

        printf(C.greb("\n==>")..C.bright(" Installing the following packages from AUR\n"))
        local str = string.format(g("Targets (%d):"), #needs)
        list_display(str, needs)
    else
        display_alpm_targets(packages)
    end

    printf("\n")

    local confirm
    if (config.op_s_downloadonly) then
        confirm = yesno(C.yelb("==>")..C.bright(" Proceed with download?"))
    else
        confirm = yesno(C.yelb("==>")..C.bright(" Proceed with installation?"))
    end
    if (not confirm) then
        return transcleanup()
    end
    data = {}
    transret, data = alpm.trans_commit(data)
    if (transret == -1) then
        eprintf("LOG_ERROR", g("failed to commit transaction (%s)\n"), alpm.strerrorlast())
        if (alpm.pm_errno() == "P_E_FILE_CONFLICTS") then
            for i, conflict in ipairs(data) do
                if (conflict:fileconflict_get_type() == "FILECONFLICT_TARGET") then
                    printf(g("%s exists in both '%s' and '%s'\n"),
                    conflict:fileconflict_get_file(),
                    conflict:fileconflict_get_target(),
                    conflict:fileconflict_get_ctarget())
                elseif (conflict:fileconflict_get_type() == "FILECONFLICT_FILESYSTEM") then
                    printf(g("%s: %s exists in filesystem\n"),
                        conflict:fileconflict_get_target(),
                        conflict:fileconflict_get_file())
                end
            end
        elseif (alpm.pm_errno() == "P_E_PKG_INVALID" or alpm.pm_errno() == "P_E_DLT_INVALID") then
            for i, filename in ipairs(data) do
                printf(g("%s is invalid or corrupted\n"), filename)
            end
        end

        printf(g(C.redb("==>")..C.bright("Errors occured, no packages were upgraded.\n")))
        retval = 1
        return transcleanup()
    end

    if (next(aurpkgs)) then
        transcleanup()
        return aur_install(aurpkgs)
    else
        return transcleanup()
    end
end

local function clyde_sync(targets)
    if (config.flags["downloadonly"] or config.op_s_printuris) then
        local isin, int = tblisin(config.logmask, "LOG_WARNING")
        if (isin) then
            fastremove(config.logmask, int)
        end
    end

    if (config.op_s_clean > 0) then
        local ret = 0
        local transinitret = trans_init({})

        if (transinitret == -1) then
            return 1
        end
        ret = ret + sync_cleancache(config.op_s_clean)
        printf("\n")
        ret = ret + sync_cleandb_all()

        if (trans_release() == -1) then
            ret = ret + 1
        end

        return ret
    end

    local sync_dbs = alpm.option_get_syncdbs()
    if (sync_dbs == nil or #sync_dbs == 0) then
        lprintf("LOG_ERROR", g("no usable package repositories configured.\n"))
        return 1
    end

    if (config.op_s_sync > 0) then
        printf(g(C.blub("::")..C.bright(" Synchronizing package databases...\n")))
        alpm.logaction("synchronizing package lists\n")
        if (sync_synctree(config.op_s_sync, sync_dbs) ~= 1) then
          return 1
        end
    end

    if (config.op_s_search) then
        return sync_search(sync_dbs, targets)
    end

    if (config.group > 0) then
        return sync_group(config.group, sync_dbs, targets)
    end

    if (config.op_s_info) then
        return sync_info(sync_dbs, targets)
    end

    if (config.op_q_list) then
        return sync_list(sync_dbs, targets)
    end

    if (not next(targets)) then
        if (config.op_s_upgrade > 0) then
            --proceed--
        elseif (config.op_s_sync > 0) then
            return 0
        else
            lprintf("LOG_ERROR", g("no targets specified (use -h for help)\n"))
            return 1
        end
    end

    local targs = tblstrdup(targets)

    if (not config.flags["downloadonly"] and (not config.op_s_printuris)) then
        local packages = syncfirst()
        if (next(packages)) then
            local tmp = tbldiff(targets, packages)
            if (config.op_s_upgrade > 0 or next(tmp)) then
                tmp = nil
                printf(g(C.blub("::")..C.bright(" The following packages should be upgraded first :\n")))
                list_display("   ", packages)
                if (yesno(g(C.yelb("::")..C.bright(" Do you want to cancel the current operation\n::and upgrade these packages now?")))) then
                    targs = packages
                    config.flags = {}
                    config.op_s_upgrade = 0
                else
                    packages = nil
                end
                printf("\n")
            else
                lprintf("LOG_DEBUG", "skipping SyncFirst dialog\n")
                packages = nil
            end
        end
    end

    local ret = sync_trans(targs)
    targs = nil
    return ret
end

function main(targets)
    local result = clyde_sync(targets)
    if (not result) then
        result = 1
    end

    return result
end
