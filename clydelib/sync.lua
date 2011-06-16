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
local ui = require "clydelib.ui"
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
                local localdb = alpm.option_get_localdb()
                local pkg     = localdb:db_get_pkg(localpkg:pkg_get_name())
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

        local success, err = pcall( makepath, cachedir )
        if not success then
            eprintf("LOG_ERROR", "%smkdir: %s\n",
                    g("could not create new cache directory\n"),
                    err )
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

-- SYNC SEARCH ---------------------------------------------------------------

-- These little helper functions are used with mk_match_printer from
-- ui.lua. Inside mk_match_printer, their results are gathered in a
-- table, so if they don't want to be printed they return nil.

-- Helper function for installed_tag().
local function get_installed_version ( pkg_name )
    local pkg = alpm.option_get_localdb():db_get_pkg( pkg_name )
    if pkg then return pkg:pkg_get_version()
    else return nil
    end
end

-- Returns a tag (string) to print showing if a pkg is installed and
-- if a different version is installed. If the pkg is not installed,
-- returns nil.
local function installed_tag ( pkg )
    local pkg_name, pkg_version = pkg.name, pkg.version
    local installed_version = get_installed_version( pkg_name )
    if not installed_version then return nil end

    -- If the found version is different from installed version,
    -- print the installed version.
    local tagtext = ""
    if alpm.pkg_vercmp( installed_version, pkg_version ) ~= 0 then
        tagtext = ui.colorize_verstr( installed_version, C.yel ) .. " "
    end

    tagtext = tagtext .. C.yel( "installed" )

    -- Put a space in-between ourself and previous text on the line.
    return C.yelb( "[" ) .. tagtext .. C.yelb( "]" )
end

-- Returns the size of the download or nil if N/A.
local function download_size_tag ( pkg )
    local size = pkg.size
    if size and config.showsize then
        if size > (1024 * 1024) then
            return string.format( "[%.2f MB]", size / (1024 * 1024))
        elseif size > 1024 then
            return string.format( "[%.2f KB]", size / 1024 )
        else
            return string.format( "[%d B]", size )
        end
    else
        return nil
    end
end

-- Obviously, this only works for AUR packages...
local function votes_tag ( pkg )
    if pkg.votes then
        return C.yelb( "(" ) .. C.yel( pkg.votes ) .. C.yelb( ")" )
    else
        return nil
    end
end

-- Returns a closure that prints a fancy colorized entry for a search match.
local function mk_match_printer ( shownumbers )
    -- We use the package printer provided in the clydelib.ui module
    -- and we provide our own custom tag generators...
    local print_counter = 1
    local pkg_colorizer = ui.mk_pkg_colorizer( installed_tag,
                                               download_size_tag,
                                               ui.groups_tag, votes_tag )
    local function match_printer ( match )
        if shownumbers then
            io.write( print_counter .. " " )
            print_counter = print_counter + 1
        end

        print( pkg_colorizer( match ))

        -- pkg_printer only prints one line so we print the desc ourselves
        io.write( "    " )
        indentprint( match.desc, 3 )
        print()
    end

    return match_printer
end

-- Returns an array of packages which match the search queries.
local function sync_search_alpm ( targets, printcb )
    local found_pkgs = {}
    local syncsdbs = alpm.option_get_syncdbs()

    for i, syncdb in ipairs( syncsdbs ) do
        local matching_pkgs
        if next( targets ) then
            -- Let libalpm search for target strings
            matching_pkgs = syncdb:db_search( targets )
        else
            -- If no query strings are given then print everything out
            matching_pkgs = syncdb:db_get_pkgcache()
        end
        
        for i, pkg in ipairs( matching_pkgs ) do
            table.insert( found_pkgs, pkg )
            printcb { name    = pkg:pkg_get_name();
                      version = pkg:pkg_get_version();
                      desc    = pkg:pkg_get_desc();
                      dbname  = syncdb:db_get_name();
                      size    = pkg:pkg_get_size();
                      groups  = pkg:pkg_get_groups() }
        end
    end

    return found_pkgs
end

-- Returns an array of tables with info on packages who match the queries.
-- An AND search is performed so packages must match ALL the strings given.
local function sync_search_aur ( targets, printcb )
    if not targets or #targets == 0 then return {} end

    -- Check all of our target strings first and remove invalid ones...
    local targets = targets -- copy it first
    local i = 1
    while i <= #targets do
        local target = targets[i]
        if #target < 2 then
            lprintf("LOG_WARNING",
                    "Query arg '%s' is too small to search AUR\n",
                    target)
            table.remove( targets, i )
        else
            i = i + 1
        end
    end

    -- In an AND query, matches are an intersection of all the result sets.
    local matches_int = {}
    for i, query in ipairs( targets ) do
        local query_matches = aur.rpc_search( query )
        if i == 1 then
            matches_int = query_matches
        else
            -- Remove matches from the match intersection which are not also
            -- matches in our current query.
            for pkgname, unused in pairs( matches_int ) do
                if not query_matches[ pkgname ] then
                    matches_int[ pkgname ] = nil
                end
            end
        end
    end

    local found = {}
    for unused, pkginfo in pairs( matches_int ) do
        table.insert( found, pkginfo )
    end

    local function by_name ( left, right ) return left.name < right.name end

    table.sort( found, by_name )

    for i, info in ipairs( found ) do
        -- XXX: Should we make a new clean copy of the table?
        info.dbname = "aur"
        printcb( info )
    end


    return found
end

-- Searches for the given "targets" in ALPM and AUR. Returns a list of
-- package names that were found.
function sync_search ( targets, shownumbers )
    local function print_dumb ( match )
        print( C.bright( match.name ))
    end

    local match_printcb = config.quiet and print_dumb
        or mk_match_printer( shownumbers )
    local found_names = {}

    -- First we search the ALPM repos...
    if not config.op_s_search_aur_only then
        local found_pkg_objs = sync_search_alpm( targets, match_printcb )
        for i, pkgobj in ipairs( found_pkg_objs ) do
            table.insert( found_names, pkgobj:pkg_get_name())
        end
    end

    -- Next we search the AUR...
    if not config.op_s_search_repos_only then
        local found_aur_pkgs = sync_search_aur( targets, match_printcb )
        for i, pkginfo in ipairs( found_aur_pkgs ) do
            table.insert( found_names, pkginfo.name )
        end
    end

    return found_names
end

------------------------------------------------------------------------------

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

-- SYNC INFO -----------------------------------------------------------------

-- Find a database by its name.
local function find_repodb ( reponame )
    for i, dbobj in ipairs( alpm.option_get_syncdbs()) do
        if dbobj:db_get_name() == reponame then
            return dbobj
        end
    end

    local errfmt = g( "repository '%s' does not exist\n" )
    return nil, string.format( errfmt, reponame )
end

local function search_repo_for_pkg ( reponame, pkgname )
    local db, err = find_repodb( reponame )
    if not db then return nil, err end

    local pkgobj = db:db_get_pkg( pkgname )
    if pkgobj then return pkgobj end

    local errfmt = g("package '%s' was not found in repository '%s'\n")
    return nil, string.format( errfmt, pkgname, reponame )
end

local function search_for_pkg ( pkgname )
    for i, syncdb in ipairs( alpm.option_get_syncdbs()) do
        local pkgobj   = syncdb:db_get_pkg( pkgname )
        local reponame = syncdb:db_get_name()

        if pkgobj then return pkgobj, reponame end
    end

    local err = string.format( g("package '%s' was not found\n"), pkgname )
    return nil, nil, err
end

local function sync_info_alpm ( pkgname, reponame )
    local pkgobj, err

    if reponame then
        -- A repository name was explicitly specified.
        pkgobj, err = search_repo_for_pkg( reponame, pkgname )
    else
        pkgobj, reponame, err = search_for_pkg( pkgname )
    end

    if err then return nil, err end

    dump_pkg_sync( pkgobj, reponame )
    return true
end

local function sync_info_aur ( pkgname )
    if not aur.package_exists( pkgname ) then
        local err = string.format( "package '%s' was not found in the AUR\n",
                                   pkgname )
        return nil, err
    end

    packages.dump_pkg_sync_aur( pkgname )
    return true
end

local function sync_info_target ( target )
    -- If a repo name was given by using "reponame/pkgname" then
    -- this limits our search to one DB or the AUR.
    local reponame, pkgname = target:match( "^([^/]+)/(%S+)$" )
    if reponame and pkgname then
        local success, err
        if reponame == "aur" then
            success, err = sync_info_aur( pkgname )
        else
            success, err = sync_info_alpm( pkgname, reponame )
        end
        return success, err
    end

    if sync_info_alpm( target ) or sync_info_aur( target ) then return true end
    return nil, string.format( g("package '%s' was not found\n"), target )
end

local function sync_info ( targets )
    local error_occurred = false

    for i, target in ipairs( targets ) do
        local success, err = sync_info_target( target )
        if not success then
            error_occurred = true -- note error but keep looping
            eprintf( "LOG_ERROR", err )
        end
    end

    return error_occurred and 1 or 0
end

------------------------------------------------------------------------------

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

-- Awesome hack to emulate old behavior.
-- Returns trans_add_pkg's return value if found or nil if not found.
local function sync_target ( pkg_name ) 
    local syncdbs = alpm.option_get_syncdbs()
    local pkg     = alpm.find_dbs_satisfier( syncdbs, pkg_name )

    if pkg then return alpm.trans_add_pkg( pkg )
    else return nil end
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
                local addret = sync_target(targ)
                if addret then
                    if addret == -1 then
                        if (alpm.pm_errno() == "P_E_TRANS_DUP_TARGET" or
                            alpm.pm_errno() == "P_E_PKG_IGNORED") then
                            lprintf("LOG_WARNING", g("skipping target: %s\n"),
                                    targ)
                            found = false
                            break
                        end
                        eprintf("LOG_ERROR", "'%s': %s\n", targ,
                                alpm.strerrorlast())
                        retval = 1
                        return transcleanup()
                    end

                    found = true
                    break
                end

                print( ui.color_fmt( ":: %s package not found, "
                                     .. "searching for group...", targ))
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
                    if aur.package_exists( targ ) then
                        found = true
                        tblinsert( aurpkgs, targ )
                    end

                end

                if (not found) then
                    eprintf("LOG_ERROR", g("'%s': not found in sync db\n"), targ)
                    retval = 1
                    return transcleanup()
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
    local localdb  = alpm.option_get_localdb()
    local pkgcache = localdb:db_get_pkgcache()
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
    local pkgbuild = aur.pkgbuild_text( target )
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

    local names
    if (config.op_g_get_deps) then
        names = needs
    else
        names = targets
    end
    
    for i, pkgname in ipairs(names) do
        if (not pacmaninstallable(pkgname)) then
            aur.download_extract(pkgname, ".")
        end
    end
end

local function aur_install(targets)
    local provided = {}
    local needs = {}
    local caninstall = {}
    local needsdeps = {}
    updateprovided(provided)

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

    local origdir = lfs.currentdir()

    local installedtbl = {}
    local needscount = #needs - #pacmanpkgs
    while (#installedtbl < needscount ) do
        repeat
        updateprovided(provided)
        caninstall, needs, needsdeps = {}, {}, {}
        getalldeps(targets, needs, needsdeps, caninstall, provided)

        for i, pkg in ipairs(aurpkgs) do
            if (tblisin(caninstall, pkg)
                and not tblisin(installedtbl, pkg)) then

                local oldflag = config.flags.alldeps
                local newflag = not tflags.alldeps
                    and (tblisin(targets, pkg) or tflags.allexplicit)

                config.flags.alldeps = newflag

                local dldir  = aur.make_builddir(pkg)
                local pkgpath, pkgdir = aur.download_extract(pkg, dldir)

                -- Don't let root hog our new package files...
                if utilcore.geteuid() == 0 then
                    aur.chown_builduser( pkgpath )
                    aur.chown_builduser( pkgdir, '-R' )
                end

                if not config.noconfirm then
                    aur.customizepkg(pkg, pkgdir)
                end
                aur.makepkg(pkgdir)
                aur.installpkg(pkg)

                installed = installed + 1
                tblinsert(installedtbl, pkg)

                config.flags.alldeps = oldflag
            end
        end
        until 1
    end
end

-- Convert the table from alpm.option_get_ignorepkgs() to an easily
-- queryable table indexed by the pkg name
local function get_ignore_pkgs()
    local alpm_ignore_pkgs = alpm.option_get_ignorepkgs()
    local ignore_pkgs = {}

    for i, pkg in ipairs(alpm_ignore_pkgs) do
        ignore_pkgs[pkg] = true
    end

    return ignore_pkgs
end

local function find_installed_aur ()
    -- Gather a list of packages which aren't available from our repos...
    local foreign_count, foreign_pkgs = 0, {}
    local is_ignorepkg = get_ignore_pkgs()

    local localdb = alpm.option_get_localdb()
    for i, pkg in ipairs( localdb:db_get_pkgcache()) do
        local name = pkg:pkg_get_name()

        if not is_ignorepkg[name] and not pacmaninstallable(name) then
            local foreigner = { name = name; version = pkg:pkg_get_version() }
            table.insert( foreign_pkgs, foreigner )
            foreign_count = foreign_count + 1
        end
    end

    -- If the version on AUR is > our installed version get ready
    -- to update the package from AUR...

    local function aur_version ( pkgname )
        local success, result = pcall( aur.rpc_info, pkgname )

        if not success then
            print() -- Print newline, skip the progress bar.
            eprintf( "LOG_ERROR", result .. "\n" )
            return nil
        end

        if result then return result.version
        else return nil
        end
    end

    -- Loop through the packages in alphabetical order...
    table.sort( foreign_pkgs,
                function ( left, right )
                    return left.name < right.name
                end )

    print( C.blub("::") .. C.bright(" Identifying AUR packages..."))

    local aurpkgs = {}
    for i, foreigner in ipairs( foreign_pkgs ) do
        local name, version = foreigner.name, foreigner.version

        -- Display the progress bar with package name...
        local dispname = name
        if #dispname > 22 then
            dispname = string.sub( dispname, 1, 19 )
            dispname = dispname .. "..."
        end

        local message = string.format( " %-23s%3.0f/%3.0f",
                                       dispname, i, foreign_count )

        io.write( message )
        callback.fill_progress( math.floor( i*100/foreign_count ),
                                math.ceil( i*100/foreign_count ),
                                util.getcols() - #message )

        -- If a newer version is on the AUR, then add it to our list
        local aurver = aur_version( name )
        if aurver and alpm.pkg_vercmp( aurver, version ) > 0 then
            table.insert( aurpkgs, name )
        end
    end

    print( C.blub("  -> ") .. C.bright
       .. "Identified " .. #aurpkgs .. " AUR packages." .. C.reset )

    return aurpkgs
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
            aurpkgs = find_installed_aur()
            targets = aurpkgs
        end
    else
        for i, targ in ipairs(targets) do
            repeat
                local addret = sync_target(targ)
                if addret then
                    if addret == -1 then
                        if (alpm.pm_errno() == "P_E_TRANS_DUP_TARGET" or
                            alpm.pm_errno() == "P_E_PKG_IGNORED") then
                            lprintf("LOG_WARNING", g("skipping target: %s\n"),
                                    targ)
                            found = false
                            break
                        end
                        eprintf("LOG_ERROR", "'%s': %s\n", targ,
                                alpm.strerrorlast())
                        retval = 1
                        return transcleanup()
                    end

                    found = true
                    break
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
                    if aur.package_exists( targ ) then
                        found = true
                        tblinsert( aurpkgs, targ )
                    end

                end

                if (not found) then
                    if #targets == 1 then 
						eprintf("LOG_ERROR", g("'%s': not found in sync db\n"), targ)
					else
						eprintf("LOG_ERROR", g("'%s': not found in sync db, skipping\n"), targ)
					end
					targets[i] = 0
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
        if (not pacmaninstallable(pkg) and not (pkg == 0)) then
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
        local found = sync_search( targets )
        if #found > 0 then return 0 else return 1 end
    end

    if (config.group > 0) then
        return sync_group(config.group, sync_dbs, targets)
    end

    if config.op_s_info then
        if next( targets ) then
            return sync_info( targets )
        else
            -- If no targets were given... dump everything in alpm repos!!
            for i, db in ipairs( sync_dbs ) do
                local dbname = db:db_get_name()
                for i, pkg in ipairs( db:db_get_pkgcache()) do
                    dump_pkg_sync( pkg, dbname )
                end
            end
            return 0
        end
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
