module(..., package.seeall)
package.cpath = "/usr/lib/lua/5.1/?.so;"..package.cpath
package.path ="/usr/share/lua/5.1/?.lua;"..package.path
local lfs = require "lfs"
local alpm = require "lualpm"
local util = require "clydelib.util"
local utilcore = require "clydelib.utilcore"
local packages = require "clydelib.packages"
local aur = require "clydelib.aur"
local upgrade = require "clydelib.upgrade"
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
local strsplit = util.strsplit
local pm_targets = pm_targets
local community = community
local dump_pkg_changelog = packages.dump_pkg_changelog
local dump_pkg_files = packages.dump_pkg_files
local dump_pkg_sync = packages.dump_pkg_sync

local tblsort = table.sort
local tblconcat = table.concat
local Json = require "clydelib.Json"
require "socket"
local http = require "socket.http"
local url = require "socket.url"
local ltn12 = require "ltn12"
local aurresults = {}
local aururl = "http://aur.archlinux.org/rpc.php?"
local aurmethod = {
    ['search'] = "type=search&";
    ['info'] = "type=info&";
    ['msearch'] = "type=msearch&";
}

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
--local function sync_cleandb(dbpath, keep_used)
--local function sync_cleandb_all()
--local function sync_cleancache(level)
local function sync_synctree(level, syncs)
    local success = 0
    local ret

    if (trans_init("T_T_SYNC", config.flags) == -1) then
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
    return success > 0
end







local function sync_search(syncs, targets)
    local found = false
    local ret
    local localdb = alpm.option_get_localdb()
--    local pkgcache = localdb:db_get_pkgcache()
    for i, db in ipairs(syncs) do
        repeat
            if (next(targets)) then
                ret = db:db_search(targets)
            else
                ret = pkgcache
                ret = db:db_get_pkgcache()
            end

            if (not next(ret)) then
                break
            else
                found = true
            end

            for i, pkg in ipairs(ret) do
                if (not config.quiet) then
                    if (localdb:db_get_pkg(pkg:pkg_get_name())) then
                        printf("%s/%s %s [installed]", db:db_get_name(), pkg:pkg_get_name(), pkg:pkg_get_version())
                    else
                        printf("%s/%s %s", db:db_get_name(), pkg:pkg_get_name(), pkg:pkg_get_version())
                    end
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
                        printf(" (")
                        for i, group in ipairs(grp) do
                            printf("%s", group)
                            if (i < #grp) then
                                printf(" ")
                            end
                        end
                        printf(")")
                    end

                    printf("\n    ")
                    indentprint(pkg:pkg_get_desc(), 3)
                end
                printf("\n")
            end

        until 1
    end

    if (next(targets)) then
        local searchurl = aururl..aurmethod.search.."arg="..url.escape(targets[1])
        if (#targets[1] < 2) then
            lprintf("LOG_WARNING", "First query arg too small to search AUR\n")
            return (not found)
        end
        local r, e = http.request {
            url = searchurl;
            sink = ltn12.sink.table(aurresults);
        }
        aurresults = tblconcat(aurresults)
        local jsonresults = Json.Decode(aurresults)
        local aurpkgs = {}
        if (type(jsonresults.results) ~= "table") then
            jsonresults.results = {}
        end
        for k, v in pairs(jsonresults.results) do
            aurpkgs[v.Name] = {['name'] = v.Name; ['version'] = v.Version; ['description'] = v.Description, ['votes'] = v.NumVotes}

        end
        prune(aurpkgs, targets)
        local sortedpkgs = sortaur(aurpkgs)
        for i, pkg in ipairs(sortedpkgs) do
            if (not config.quiet) then
                if (localdb:db_get_pkg(pkg.name)) then
                    printf("aur/%s %s [installed] (%s)\n    ", pkg.name, pkg.version, pkg.votes)
                else
                    printf("aur/%s %s (%s)\n    ", pkg.name, pkg.version, pkg.votes)
                end
                indentprint(pkg.description, 3)
                printf("\n")
            else
                printf("%s\n", pkg.name)
            end
        end
    end
    return (not found)
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
        local foundpkg = false
        for i, target in ipairs(targets) do
            local slash = target:find("/")
            if (slash) then
                local pkgstr = target:sub(1, slash)
                local repo = target:sub(slash)
                local dbmatch = false
                local db
                for i, database in ipairs(syncs) do
                    if (repo == database:db_get_name()) then
                        dbmatch = true
                        db = database
                        break
                    end
                end

                if (not dbmatch) then
                    eprintf("LOG_ERROR", g("repository '%s' does not exist\n"), repo)
                    --printf("error: repository '%s' does not exist\n", repo)
                    return 1
                end

                local pkgcache = db:db_get_pkgache()
                for i, pkg in ipairs(pkgcache) do
                    if (pkgstr == pkg:pkg_get_name()) then
                        dump_pkg_sync(pkg, db:db_get_name())
                        foundpkg = false
                        break
                    end
                end

                if (not foundpkg) then
                    eprintf("LOG_ERROR", g("package '%s' was not found in repository '%s'\n"), pkgstr, repo)
                    --printf("error: package '%s' was not found in repository '%s'\n", pkgstr, repo)
                    ret = ret + 1
                end
            else
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
                if (not foundpkg) then
                    eprintf("LOG_ERROR", g("package '%s' was not found\n"), pkgstr)
                    --printf("error: package '%s' was not found\n", pkgstr)
                    ret = ret + 1
                end
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
--                    print(type(d))
                    db = d
                    break
                end
            end

            if (db == nil) then
                eprintf("LOG_ERROR", g("repository \"%s\" was not found.\n"), repo)
                --printf("error: repository \"%s\" was not found.\n", repo)
                return 1
            end
            --local ls = tbljoin(ls, db)
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
            if (package --[[and next(package)]]) then
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
    local inforesults = {}
    local aurpkgs = {}
    local sync_dbs = alpm.option_get_syncdbs()
    local function transcleanup()
        --if (data) then
        --    data = nil
        --end
        if (trans_release() == -1) then
            retval = 1
        end
        return retval
        --cleanup(retval)
    end
    --local data = {}
    --local retval

    if (trans_init("T_T_SYNC", config.flags) == -1) then
        --print("trans init fail")
        --print(alpm.strerrorlast())
        return 1
    end

    if (config.op_s_upgrade > 0) then
        printf(g(":: Starting full system upgrade..\n"))
        --TODO implement this in lualpm
--        alpm.logaction("starting full system upgrade\n")
        local op_s_upgrade
        if (config.op_s_upgrade >= 2) then
            op_s_upgrade = 1
        else
            op_s_upgrade = 0
        end

        if (alpm.trans_sysupgrade(op_s_upgrade) == -1) then
            eprintf("LOG_ERROR", "%s\n", alpm.strerrorlast())
            retval = 1
            return transcleanup()
        end
    else
        for i, targ in ipairs(targets) do
            repeat
                if (alpm.trans_addtarget(targ) == -1) then
                    found = false
                    if (alpm.strerrorlast() == g("duplicate target") or
                        alpm.strerrorlast() == g("operation cancelled due to ignorepkg")) then
                        lprintf("LOG_WARNING", g("skipping target: %s\n"), targ)
                        break
                    end
                    if (alpm.strerrorlast == g("could not find or read package")) then
                        eprintf("LOG_ERROR", "'%s': %s\n", targ, alpm.strerrorlast())
                        retval = 1
                        return transcleanup()
                    end
                    printf(g("%s package not found, searching for group...\n"), targ)
                    for i, db in ipairs(sync_dbs) do
                        local grp = db:db_readgrp(targ)
                        if (grp) then
                            found = true
                            printf(g(":: group %s (including ignored packages):\n"), targ)
                            local grppkgs = grp:grp_get_pkgs()
                            local pkgs = tblremovedupes(grppkgs)
                            local pkgnames = {}
                            for i, pkgname in ipairs(pkgs) do
                                tblinsert(pkgnames, pkgname:pkg_get_name())
                            end
                            list_display("   ", pkgnames)
                            if (yesno(g(":: Install whole content?"))) then
                                for i, pkgname in ipairs(pkgnames) do
                                    tblinsert(targets, pkgname)
                                end
                            else
                                for i, pkgname in ipairs(pkgnames) do
                                    if (yesno(g(":: Install %s from group %s?"), pkgname, targ)) then
                                        tblinsert(targets, pkgname)
                                    end
                                end
                            end
                            pkgnames = nil
                            pkgs = nil
                        end
                    end

                    if (not found) then
                        printf("%s group not found, searching AUR...\n", targ)
                        local infourl = aururl..aurmethod.info.."arg="..url.escape(targ)
                        inforesults = {}
                        local r, e = http.request {
                            url = infourl;
                            sink = ltn12.sink.table(inforesults);
                        }
                        inforesults = tblconcat(inforesults)
                        local jsonresults = Json.Decode(inforesults)
                        if (type(jsonresults.results) ~= "table") then
                            jsonresults.results = {}
                        end

                        if (jsonresults.results.Name) then
                            found = true
                            tblinsert(aurpkgs, targ)
                            print("adding pkg")
                        end

                    end
                    local pkgs = db_local:db_get_pkgcache()
--                    print(unpack(pkgs))
--                    for k, v in ipairs(pkgs) do print(v:pkg_get_name()) end

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
        local err = alpm.strerrorlast()
        eprintf("LOG_ERROR", g("failed to prepare transaction (%s)\n"), err)
        if (err == g("could not satisfy dependencies")) then
            for i, miss in ipairs(data) do
                local dep = miss:miss_get_dep()
                local depstring = dep:dep_compute_string()
                printf(g(":: %s: requires %s\n"), miss:miss_get_target(), depstring)
            end

        elseif (err == g("conflicting dependencies")) then
            --local conflict = data
            --print("HI")
            for i, conflict in ipairs(data) do
                --print(k, v)
                printf(g(":: %s: conflicts with %s\n"), conflict:conflict_get_package1(), conflict:conflict_get_package2())
            end
        end
        retval = 1
        return transcleanup()
    end

    local packages = alpm.trans_get_pkgs()
    if (not next(packages) and not found) then
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

    display_synctargets(packages)
    --TODO: write this function to pretty up install list, or something
--    display_aurtargets(aurpkgs)
    if (next(aurpkgs)) then
        print("\nInstalling the following packages from AUR\n")
        list_display("", aurpkgs)
--        print(unpack(aurpkgs))
        print(" (Plus dependencies)")
    end
    printf("\n")

    local confirm
    if (config.op_s_downloadonly) then
        confirm = yesno("Proceed with download?")
    else
        confirm = yesno("Proceed with installation?")
    end
    if (not confirm) then
        return transcleanup()
    end
    data = {}
    transret, data = alpm.trans_commit(data)
    if (transret == -1) then
        eprintf("LOG_ERROR", g("failed to commit transaction (%s)\n"), alpm.strerrorlast())
        local err = alpm.strerrorlast()
        --TODO: bunch of bindings needed for here
        if (err == g("conflicting files")) then
        end



        printf(g("Errors occured, no packages were upgraded.\n"))
        retval = 1
        return transcleanup()
    end
--    print(transret, data, alpm.strerrorlast())



--       print("     conflicting dependencies")
    if (next(aurpkgs)) then
        transcleanup()
        return aur_install(aurpkgs)
    else
        return transcleanup()
    end
end


local function aur_install(targets)
    local mkpkgopts = table.concat(config.mkpkgopts)
    local provided = {}
    local needs = {}
    local caninstall = {}
    local needsdeps = {}
    local function updateprovided(tbl)
        local db_local = alpm.option_get_localdb()
        local pkgcache = db_local:db_get_pkgcache()
        for i, pkg in ipairs(pkgcache) do
            tbl[pkg:pkg_get_name()] = pkg:pkg_get_version()
--            print(tbl[pkg:pkg_get_name()])
            local provides = pkg:pkg_get_provides()
            if (next(provides)) then
                for i, prov in ipairs(provides) do
                    --print(prov)
                    local p = prov:match("(.+)=") or prov
                    local v = prov:match("=(.+)") or nil
                    --print(p, v, prov)
                    tbl[p] = v or true
                    --print(prov)
                end
            end
        end
    end
    updateprovided(provided)

    local function download_extract(target)
        local user = os.getenv("SUDO_USER") or "root"
        local host = "aur.archlinux.org"
        print(target)
        aur.get(host, string.format("/packages/%s/%s.tar.gz", target, target))
        aur.dispatcher()
--        print(string.format("/packages/%s/%s.tar.gz", target, target))
        lfs.chdir("/tmp/clyde/"..target)
        os.execute("bsdtar -xf " .. target .. ".tar.gz")
        os.execute("chmod 777 -R *")
        os.execute("chown "..user..":users -R /tmp/clyde/*")
        --print(lfs.chdir(lfs.currentdir().. "/"..target))
        --print(lfs.currentdir())
        --os.execute("ls")
    end
    --download_extract("clamz")

    local function makepkg(target)
        lfs.chdir("/tmp/clyde/"..target.."/"..target)
        local user = os.getenv("SUDO_USER")
        local result
        --print(user)
        if (user) then
            result = os.execute("su "..user.." -c 'makepkg -f' "..mkpkgopts)
        else
            local response = noyes("Running makepkg as root is a bad idea! Continue anyway?")
            if (response)  then
                result = os.execute("makepkg -f --asroot "..mkpkgopts)
            else
                cleanup(1)
            end
        end
        if (result ~= 0) then
            print("Build failed")
            cleanup(1)
        end

    end
--    makepkg("clamz")
    local function installpkgs(targets)
        if (type(targets) ~= "table") then
            targets = {targets}
        end
        local pkgs = {}
        for i, target in ipairs(targets) do
            lfs.chdir("/tmp/clyde/"..target.."/"..target)
            for file in lfs.dir(lfs.currentdir()) do
                local pkg = file:match(".-%.pkg%.tar%.gz")
--                print(pkg)
                if (pkg) then
                    tblinsert(pkgs, pkg)
                end
            end
        end
        local ret = upgrade.main(pkgs)
        --print(ret)
        if (ret ~= 0) then
            cleanup(ret)
        end
    end
--installpkgs({"clamz"})
    local function getdepends(target)
        local ret = {}
        local ret2 = {}
        if provided[target] then return {}, {} end
        local sync_dbs = alpm.option_get_syncdbs()
        for i, db in ipairs(sync_dbs) do
            local package = db:db_get_pkg(target)
            if (package) then
                local depends = package:pkg_get_depends()
                for i, dep in ipairs(depends) do
                    --print(dep:dep_compute_string())
                    tblinsert(ret, dep:dep_compute_string())
                end
                --print("HI")
                return ret, {}
            end
        end

        local host = "aur.archlinux.org"
        local pkgbuild = {}
        http.request {
            url = "http://"..host.."/packages/"..target.."/"..target.."/PKGBUILD";
            sink = ltn12.sink.table(pkgbuild)
        }
        pkgbuild = table.concat(pkgbuild)
--        print(pkgbuild)

        pkgbuild = pkgbuild:gsub("#.-\n", "\n")
        local makedepends = pkgbuild:match("makedepends=%((.-)%)") or ""
        pkgbuild = pkgbuild:gsub("makedepends=%((.-)%)", "")
        local depends = pkgbuild:match("depends=%((.-)%)") or ""
--        print(target, depends, "target, depends")
        depends = depends:gsub("[\"'\\\t%s]", " ")
        makedepends = makedepends:gsub("[\"'\\\t%s]", " ")

--        print(depends, makedepends, "depends", "makedepends")
--        print(depends)
        ret = strsplit(depends, " ") or {}
        ret2 = strsplit(makedepends, " ") or {}
--        print("RET", unpack(ret))
--        print("RET2",unpack(ret2))
        return ret, ret2
    end

--    updateprovided(provided)
    local memodepends = {}
    local memomakedepends = {}

    local function getalldeps(targs)
        local done = true
        local dupe = tblstrdup(needsdeps)
        for i, targ in ipairs(targs) do
            if (not tblisin(needs, targ)) then
                tblinsert(needs, targ)
            end
            --local _
            local depends, makedepends = getdepends(targ)
--            print(depends, _)
--            print("DEPENDS",unpack(depends))
--            local _, makedepends = true, memomakedepends[targ] or getdepends(targ)
--            print(_, makedepends)
--            print("MAKE",unpack(makedepends))
--            if (depends and next(depends) and not memodepends[targ]) then memodepends[targ] = depends end
--            if (makedepends and next(makedepends) and not memomakedepends[targ]) then memomakedepends[targ] = makedepends end
--            local depends, makedepends = getdepends(targ)
            --print(targ, "TARG")
            --print(unpack(depends))
            --print(unpack(makedepends))
            --depends = depends or {}
            --makedepends = makedepends or {}
            --local depends = memodepends[targ] or getdepends(targ)
            --local _, makedepends = nil, memomakedepends[targ] or getdepends(targ)
            --print(unpack(depends))
            --print(unpack(makedepends))
            --if (not memodepends[targ] or not memomakedepends[targ]) then memodepends[targ], memomakedepends[targ] = depends, makedepends end
            local bcaninstall = true
            depends = tbljoin(depends, makedepends)
--            for k, v in ipairs(depends) do print(targ, v, "k/v") end
            --print(unpack(depends))
            for i, dep in ipairs(depends) do
            repeat
--                print(dep)
                if dep == "" then break end
                --print(dep, "DEP")
                local dep = dep:match("(.+)<") or dep:match("(.+)>") or dep:match("(.+)=") or dep
                --print(dep)
                --if (not provided[dep] and not tblisin(dupe, dep)) then

                --print(dep)
--                if(provided[dep]) then print("FAIL") end
                if (not provided[dep])then
                    --print(dep, "NOT PROVIDED")
                    if (not tblisin(needs, dep)) then
                        tblinsert(needs, dep)
                    end
                    bcaninstall = false
                    --print(dep, targ)
                    if (not tblisin(needsdeps, targ) and not tblisin(caninstall, targ)) then
--                        tblinsert(needs, targ)
                        --needsdeps = tbldiff(needs, caninstall)
                        done = false
                        --print(unpack(needs))
                        --print(unpack(dupe))
                        --needs = tbldiff(needs, dupe)
                        --print(unpack(needs))
                    end
                --    done = false
                --    if (not tblisin(dupe, dep)) then
                --        tblinsert(dupe, dep)
                --    end
                end
            until 1
            end
            if (bcaninstall) then
                if (not tblisin(caninstall, targ)) then
                    tblinsert(caninstall, targ)
                end
            end
        end
        needsdeps = tbldiff(needs, caninstall)
        --print(unpack(needs), "needs")
        --print(unpack(caninstall), "caninstall")
        --print(unpack(needsdeps), "needsdeps")
--        for k, v in pairs(memodepends) do
--            for k, v in pairs(v) do print(k, v) end
--        end
        return done
    end

    local donewithdeps
    donewithdeps = getalldeps(targets)
--    donewithdeps, needsdeps = getalldeps({"haskell-hashed-storage"})
--    print(donewithdeps)

--    print(unpack(caninstall))
--    print(unpack(needsdeps))
    while (not donewithdeps) do
        donewithdeps = getalldeps(needsdeps)
--        print("HI")
    end
    --print(unpack(tbldiff(caninstall, needsdeps)))
    --print(unpack(tbldiff(needsdeps, caninstall)))
--    print(unpack(caninstall))
--    print(unpack(needsdeps))
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
---[[
    config.flagsdupe = tblstrdup(config.flags)
--    socket.sleep(10)
--    repeat
--    local caninstallbool = true
--    updateprovided(provided)
--    getalldeps(needsdeps)
--    local installable = #caninstall
--    while (installable > 0) do
        --[[
        if (not caninstallbool) then
            return
        end
        repeat
            if (not caninstallbool) then
                return
            end
            --]]
        --repeat
        --updateprovided(provided)
        --getalldeps(needsdeps)

        --updateprovided(provided)
--        local package
--        print(unpack(caninstall))
--        repeat
--            updateprovided(provided)
--            getalldeps(needsdeps)
--            print(unpack(caninstall))
--        for i, pkg in ipairs(caninstall) do
            --repeat

--            print(unpack(caninstall))
            local installed = 0
            while (next(caninstall) and (installed < #needs)) do
--            while (next(caninstall) and next(needsdeps)) do
--                print(unpack(caninstall))
                updateprovided(provided)
                getalldeps(needsdeps)
--                local pkg = nil
--                print(#caninstall)
--                print(unpack(caninstall))
--            pkg = caninstall[1]
            for i, pkg in ipairs(caninstall) do
                repeat
--            print(pkg, "PKG")
            if (pacmaninstallable(pkg)) then
                if (tblisin(targets, pkg) and not tblisin(config.flagsdupe, "T_F_ALLDEPS")
                        or (tblisin(config.flagsdupe, "T_F_ALLEXPLICIT")
                                and not (tblisin(config.flagsdupe, "T_F_ALLDEPS")))) then
                    --install as explicit
                    sync_aur_trans({pkg})
                    updateprovided(provided)
                    getalldeps(needsdeps)
--                    print(unpack(needsdeps))
                    --fastremove(caninstall, pkg)
                    --local found, index = tblisin(caninstall, pkg)
                    --table.remove(caninstall, index)
                    print("installed " .. pkg .. " explicitly")
                    installed = installed + 1
                    break
                    --...
                else
                    tblinsert(config.flags, "T_F_ALLDEPS")
                    --install as deps
                    --...
                    sync_aur_trans({pkg})
                    updateprovided(provided)
                    getalldeps(needsdeps)
--                    print(unpack(needsdeps))
                    removeflags("T_F_ALLDEPS")
                    --local found, index = tblisin(caninstall, pkg)
--                    fastremove(caninstall, pkg)
                    print("installed " .. pkg .. " as dependency")
                    installed = installed + 1
                    break
                end
            else
                --install aur pkgs
                if (tblisin(targets, pkg) and not tblisin(config.flagsdupe, "T_F_ALLDEPS")
                        or (tblisin(config.flagsdupe, "T_F_ALLEXPLICIT")
                                and not (tblisin(config.flagsdupe, "T_F_ALLDEPS"))))  then
                    --install as explicit
                    download_extract(pkg)
                    --print(unpack(caninstall))
                    makepkg(pkg)
                    installpkgs(pkg)
                    updateprovided(provided)
                    getalldeps(needsdeps)
--                    print(unpack(needsdeps))
                    --local found, index = tblisin(caninstall, pkg)
                    print("installed " .. pkg .. " explicitly")
                    installed = installed + 1
                    break
                else
                    tblinsert(config.flags, "T_F_ALLDEPS")
                    --install as deps
                    download_extract(pkg)
                    --print(pkg)
                    makepkg(pkg)
                    installpkgs(pkg)
                    removeflags("T_F_ALLDEPS")
                    updateprovided(provided)
                    getalldeps(needsdeps)
--                    print(unpack(needsdeps))
                    --local found, index = tblisin(caninstall, pkg)
                    --table.remove(caninstall, index)
                    print("installed " .. pkg .. " as dependency")
                    installed = installed + 1
                    break
                end
                --...
            end

        --    ...
    until 1
        --fastremove(caninstall, #caninstall)
        end
--    until 1

       -- caninstall[#caninstall] = nil
    end
    --        until 1
        --updateprovided(provided)
        --getalldeps(needsdeps)
--        break
--    until 1
--    end
    --]]
    --print(unpack(installed))

--until 1


end



local function sync_trans(targets)
    local retval = 0
    local found
    local transret
    local data = {}
    local inforesults = {}
    local aurpkgs = {}
    local sync_dbs = alpm.option_get_syncdbs()
    local function transcleanup()
        --if (data) then
        --    data = nil
        --end
        if (trans_release() == -1) then
            retval = 1
        end
        return retval
        --cleanup(retval)
    end
    --local data = {}
    --local retval

    if (trans_init("T_T_SYNC", config.flags) == -1) then
        --print("trans init fail")
        --print(alpm.strerrorlast())
        return 1
    end

    if (config.op_s_upgrade > 0) then
        printf(g(":: Starting full system upgrade..\n"))
        --TODO implement this in lualpm
--        alpm.logaction("starting full system upgrade\n")
        local op_s_upgrade
        if (config.op_s_upgrade >= 2) then
            op_s_upgrade = 1
        else
            op_s_upgrade = 0
        end

        if (alpm.trans_sysupgrade(op_s_upgrade) == -1) then
            eprintf("LOG_ERROR", "%s\n", alpm.strerrorlast())
            retval = 1
            return transcleanup()
        end
    else
        for i, targ in ipairs(targets) do
            repeat
                if (alpm.trans_addtarget(targ) == -1) then
                    found = false
                    if (alpm.strerrorlast() == g("duplicate target") or
                        alpm.strerrorlast() == g("operation cancelled due to ignorepkg")) then
                        lprintf("LOG_WARNING", g("skipping target: %s\n"), targ)
                        break
                    end
                    if (alpm.strerrorlast == g("could not find or read package")) then
                        eprintf("LOG_ERROR", "'%s': %s\n", targ, alpm.strerrorlast())
                        retval = 1
                        return transcleanup()
                    end
                    printf(g("%s package not found, searching for group...\n"), targ)
                    for i, db in ipairs(sync_dbs) do
                        local grp = db:db_readgrp(targ)
                        if (grp) then
                            found = true
                            printf(g(":: group %s (including ignored packages):\n"), targ)
                            local grppkgs = grp:grp_get_pkgs()
                            local pkgs = tblremovedupes(grppkgs)
                            local pkgnames = {}
                            for i, pkgname in ipairs(pkgs) do
                                tblinsert(pkgnames, pkgname:pkg_get_name())
                            end
                            list_display("   ", pkgnames)
                            if (yesno(g(":: Install whole content?"))) then
                                for i, pkgname in ipairs(pkgnames) do
                                    tblinsert(targets, pkgname)
                                end
                            else
                                for i, pkgname in ipairs(pkgnames) do
                                    if (yesno(g(":: Install %s from group %s?"), pkgname, targ)) then
                                        tblinsert(targets, pkgname)
                                    end
                                end
                            end
                            pkgnames = nil
                            pkgs = nil
                        end
                    end

                    if (not found) then
                        printf("%s group not found, searching AUR...\n", targ)
                        local infourl = aururl..aurmethod.info.."arg="..url.escape(targ)
                        inforesults = {}
                        local r, e = http.request {
                            url = infourl;
                            sink = ltn12.sink.table(inforesults);
                        }
                        inforesults = tblconcat(inforesults)
                        local jsonresults = Json.Decode(inforesults)
                        if (type(jsonresults.results) ~= "table") then
                            jsonresults.results = {}
                        end

                        if (jsonresults.results.Name) then
                            found = true
                            tblinsert(aurpkgs, targ)
--                            print("adding pkg")
                        end

                    end
                    local pkgs = db_local:db_get_pkgcache()
--                    print(unpack(pkgs))
--                    for k, v in ipairs(pkgs) do print(v:pkg_get_name()) end

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
        local err = alpm.strerrorlast()
        eprintf("LOG_ERROR", g("failed to prepare transaction (%s)\n"), err)
        if (err == g("could not satisfy dependencies")) then
            for i, miss in ipairs(data) do
                local dep = miss:miss_get_dep()
                local depstring = dep:dep_compute_string()
                printf(g(":: %s: requires %s\n"), miss:miss_get_target(), depstring)
            end

        elseif (err == g("conflicting dependencies")) then
            --local conflict = data
            --print("HI")
            for i, conflict in ipairs(data) do
                --print(k, v)
                printf(g(":: %s: conflicts with %s\n"), conflict:conflict_get_package1(), conflict:conflict_get_package2())
            end
        end
        retval = 1
        return transcleanup()
    end

    local packages = alpm.trans_get_pkgs()
    if (not next(packages) and not found) then
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

    display_synctargets(packages)
    --TODO: write this function to pretty up install list, or something
--    display_aurtargets(aurpkgs)
    if (next(aurpkgs)) then
        print("\nInstalling the following packages from AUR\n")
        list_display("", aurpkgs)
--        print(unpack(aurpkgs))
        print(" (Plus dependencies)")
    end
    printf("\n")

    local confirm
    if (config.op_s_downloadonly) then
        confirm = yesno("Proceed with download?")
    else
        confirm = yesno("Proceed with installation?")
    end
    if (not confirm) then
        return transcleanup()
    end
    data = {}
    transret, data = alpm.trans_commit(data)
    if (transret == -1) then
        eprintf("LOG_ERROR", g("failed to commit transaction (%s)\n"), alpm.strerrorlast())
        local err = alpm.strerrorlast()
        --TODO: bunch of bindings needed for here
        if (err == g("conflicting files")) then
        end



        printf(g("Errors occured, no packages were upgraded.\n"))
        retval = 1
        return transcleanup()
    end
--    print(transret, data, alpm.strerrorlast())



--       print("     conflicting dependencies")
    if (next(aurpkgs)) then
        transcleanup()
        return aur_install(aurpkgs)
    else
        return transcleanup()
    end
end



















local function clyde_sync(targets)
    if (tblisin(config.flags, "T_F_DOWNLOADONLY") or config.op_s_printuris) then
        local isin, int = tblisin(config.logmask, "LOG_WARNING")
        if (isin) then
            fastremove(config.logmask, int)
        end
    end

    if (config.op_s_clean > 0) then
        local ret = 0
        local transinit = trans_init("T_T_SYNC", {})

        if (transinit == -1) then
            return 1
        end
        --TODO: write sync_cleancache and sync_cleandb_all
        --ret = ret + sync_cleancache(config.op_s_clean)
        printf("\n")
        --ret = ret + sync_cleandb_all()

        if (trans_release() == -1) then
            ret = ret + 1
        end

        return ret
    end

    local sync_dbs = alpm.option_get_syncdbs()
    if (sync_dbs == nil or #sync_dbs == 0) then
        lprintf("LOG_ERROR", g("no usable package repositories configured.\n"))
        --printf("error: no usable package repositories configured.\n")
        return 1
    end

    if (config.op_s_sync > 0) then
        printf(g(":: Synchronizing package databases...\n"))
        --TODO: alpm_logaction binding
        --alpm.logaction("synchronizing package lists\n")
        if (not sync_synctree(config.op_s_sync, sync_dbs)) then
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

    if ((not tblisin(config.flags, "T_F_DOWNLOADONLY")) and (not config.op_s_printuris)) then
        local packages = syncfirst()
        if (next(packages)) then
            local tmp = tbldiff(targets, packages)
            if (config.op_s_upgrade > 0 or next(tmp)) then
                tmp = nil
                printf(g(":: The following packages should be upgraded first :\n"))
                list_display("   ", packages)
                if (yesno(g(":: Do you want to cancel the current operation\n::and upgrade these packages now?"))) then
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


--local result, err = pcall(clyde_sync, pm_targets)
function main(targets)
    --local result, err = pcall(clyde_sync, targets)
    local result = clyde_sync(targets)
--local err = ""
    if (not result) then
        --if (err:match("interrupted!")) then
        --    printf("\nInterrupt signal received\n\n")
        --    trans_release()
            --cleanup(result)
        --else
            --printf("%s\n", err)
        --end
        result = 1
    end
    return result
end


--profiler.stop()
