local alpm = require "lualpm"
local lfs = require "lfs"
local util = require "clydelib.util"
local utilcore = require "clydelib.utilcore"
local aur = require "clydelib.aur"
local printf = util.printf
local eprintf = util.eprintf
local basename = util.basename
local cleanup = util.cleanup
local tblinsert = util.tblinsert
local realpath = util.realpath
local strsplit = util.strsplit
local string_display = util.string_display
local list_display = util.list_display
local list_display_linebreak = util.list_display_linebreak
local g = utilcore.gettext
local pm_targets = pm_targets
local db_local = db_local
local C = colorize

module(..., package.seeall)

function dump_pkg_backups(pkg)
    local root = alpm.option_get_root()
    local backups = pkg:pkg_get_backup()
    printf("Backup Files:\n")
    if (next(backups)) then
        for i, str in ipairs(backups) do
            repeat
                local path, msum = str:match("(%S+)%s+(%S+)")
                if not path then
                    break
                end
                path = root .. path
                if (utilcore.access(path, "R_OK") == 0) then
                    local md5sum = alpm.compute_md5sum(path)
                    if (not md5sum) then
                        printf("error: could not calculate checksums for %s\n", path)
                        break
                    end
                if (md5sum ~= msum) then
                    printf("MODIFIED\t%s\n", path)
                else
                    printf("Not Modified\t%s\n", path)
                end
            else
                printf("MISSING\t\t%s\n", path)
            end
            until 1
        end
    else
        printf("(none)\n")
    end
end

function dump_pkg_full(pkg, level)
    local bl = #C.bright("")
    local reason, bdatestr, idatestr, bdate, idate, requiredby, depstrings
    if (not pkg or not level) then
        return
    end
    bdate = pkg:pkg_get_builddate()
    if (bdate) then
        bdatestr = os.date("%a %d %b %Y %r %Z", bdate)
    end
    idate = pkg:pkg_get_installdate()
    if (idate) then
        idatestr = os.date("%a %d %b %Y %r %Z", idate)
    end

    local ireason = pkg:pkg_get_reason()

    if (ireason == "P_R_EXPLICIT") then
        reason = "Explicitly installed"
    elseif (ireason == "P_R_DEPEND") then
        reason = "Installed as a dependency for another package"
    else
        reason = "Unknown"
    end

    local depends = pkg:pkg_get_depends()
    depstrings = {}
    for i, dep in ipairs(depends) do
        depstrings[#depstrings + 1] = dep:dep_compute_string()
    end

    if (level > 0) then
        requiredby = pkg:pkg_compute_requiredby()
    end
    string_display(C.bright("Name           :"), C.bright(pkg:pkg_get_name()), bl)
    string_display(C.bright("Version        :"), C.greb(pkg:pkg_get_version()), bl)
    string_display(C.bright("URL            :"), C.cyab(pkg:pkg_get_url()), bl)
    list_display(C.bright("Licenses       :"), pkg:pkg_get_licenses(), false, 0, bl - 1)
    list_display(C.bright("Groups         :"), pkg:pkg_get_groups(), false, 0, bl - 1)
    list_display(C.bright("Provides       :"), pkg:pkg_get_provides(), false, 0, bl - 1)
    list_display(C.bright("Depends On     :"), depstrings, false, 0, bl - 1)
    list_display_linebreak(C.bright("Optional Deps  :"), pkg:pkg_get_optdepends(), bl)
    if (level > 0) then
        list_display(C.bright("Required By    :"), requiredby, false, 0, bl - 1)
        requiredby = nil
    end
    list_display(C.bright("Conflicts With :"), pkg:pkg_get_conflicts(), false, 0, bl - 1)
    list_display(C.bright("Replaces       :"), pkg:pkg_get_replaces(), false, 0, bl - 1)
    if (level < 0) then
        printf(C.bright("Download Size").."  : %6.2f K\n", pkg:pkg_get_size() / 1024)
    end
    if (level == 0) then
        printf(C.bright("Compressed Size:").." %6.2f K\n", pkg:pkg_get_size() / 1024)
    end
    printf(C.bright("Installed Size :").." %6.2f K\n", pkg:pkg_get_isize() / 1024)
    string_display(C.bright("Packager       :"), pkg:pkg_get_packager(), bl)
    string_display(C.bright("Architecture   :"), pkg:pkg_get_arch(), bl)
    string_display(C.bright("Build Date     :"), bdatestr, bl)
    if (level > 0) then
        string_display(C.bright("Install Date   :"), idatestr, bl)
        string_display(C.bright("Install Reason :"), reason, bl)
    end
    if (level >= 0) then
        local scriptlet = pkg:pkg_has_scriptlet()
        if scriptlet == 1 then scriptlet = "Yes" else scriptlet = "No" end
        string_display(C.bright("Install Script :"), scriptlet, bl)
    end
    if (level < 0) then
        string_display(C.bright("MD5 Sum        :"), pkg:pkg_get_md5sum(), bl)
    end
    string_display(C.bright("Description    :"), pkg:pkg_get_desc(), bl)
    if (level > 1) then
        dump_pkg_backups(pkg)
    end
    printf("\n")
    depstrings = nil
end

function dump_pkg_sync(pkg, treename)
    if (pkg == nil) then
        return
    end

    local dbcolors = {
        extra = C.greb;
        core = C.redb;
        community = C.magb;
        testing = C.yelb;
    }

    local treecolor = dbcolors[treename] or C.magb

    string_display(C.bright("Repository     :"), treecolor(treename))
    dump_pkg_full(pkg, -1)
end

function dump_pkg_full_aur(pkg, level)
    local bl = #C.bright("")
    if (not pkg or not level) then
        return
    end
    local reason, bdatestr, idatestr, bdate, idate, requiredby, depstrings
    local pkgbuildurl = string.format("https://aur.archlinux.org:443/packages/%s/%s/PKGBUILD", pkg,
pkg)
    local pkgbuild = aur.getgzip(pkgbuildurl)
    local tmp = os.tmpname()
    local tmpfile = io.open(tmp, "w")
    tmpfile:write(pkgbuild)
    tmpfile:close()
    local carch = util.getbasharray("/etc/makepkg.conf", "CARCH")

    local function gpa(field)
        return util.getpkgbuildarray(carch, tmp, field)
    end

    local function gpaopt(field)
        return util.getpkgbuildarraylinebreak(carch, tmp, field)
    end

    local function gpat(field)
        return strsplit(gpa(field), " ") or {}
    end

    string_display(C.bright("Name           :"), C.bright(pkg), bl)
    string_display(C.bright("Version        :"), C.greb(gpa("pkgver").."-"..gpa("pkgrel")), bl)
    string_display(C.bright("URL            :"), C.cyab(gpa("url")))
    list_display(C.bright("Licenses       :"), gpat("license"), false, 0, bl - 1)
    list_display(C.bright("Groups         :"), gpat("groups"), false, 0, bl - 1)
    list_display(C.bright("Provides       :"), gpat("provides"), false, 0, bl - 1)
    list_display(C.bright("Depends On     :"), gpat("depends"), false, 0, bl - 1)
    list_display(C.bright("Make Depends   :"), gpat("makedepends"), false, 0, bl - 1)
    list_display_linebreak(C.bright("Optional Deps  :"), gpaopt("optdepends"), bl - 1)
    list_display(C.bright("Conflicts With :"), gpat("conflicts"), false, 0, bl - 1)
    list_display(C.bright("Replaces       :"), gpat("replaces"), false, 0, bl - 1)
    string_display(C.bright("Architecture   :"), gpa("arch"), bl)
    string_display(C.bright("Description    :"), gpa("pkgdesc"), bl)
    printf("\n")
    os.remove(tmp)
end

function dump_pkg_sync_aur(pkg)
    if (pkg == nil) then
        return
    end
    string_display(C.bright("Repository     :"), C.magb("aur"))
    dump_pkg_full_aur(pkg, -1)
end

function dump_pkg_files(pkg, quiet)
    local pkgname = pkg:pkg_get_name()
    local pkgfiles = pkg:pkg_get_files()
    local root = alpm.option_get_root()

    for i, file in ipairs(pkgfiles) do
        if (not quiet) then
            printf( "%s %s%s\n", C.bright(pkgname), root, file)
        else
            printf("%s%s\n", root, file)
        end
    end
end

function dump_pkg_changelog(pkg)
    local changelog = pkg:pkg_changelog_open()

    if (changelog == nil) then
        eprintf("LOG_ERROR", g("no changelog available for '%s'.\n"), pkg:pkg_get_name())
        return
    else
        repeat
            local buff, ret = pkg:pkg_changelog_read(4096, changelog)
            printf("%s", buff)
        until ret == 0
    end

    pkg:pkg_changelog_close(changelog)
    printf("\n")
end

local function get_real_size(paccache)
    local totalsize = 0
    for i, pkg in ipairs(paccache) do
        local pkgfiles = pkg:pkg_get_files()
        for i, file in ipairs(pkgfiles) do
            local attr = lfs.symlinkattributes("/"..file)

            if (attr and (attr.mode == "file" or attr.mode == "link")) then
                totalsize = totalsize + attr.size
            end
        end
        printf(C.greb("\rReal space used by installed packages: ")) printf(C.yelb("%d M"), totalsize / 1024^2) printf(C.greb(" progression ")) printf(C.yelb("%d/%d\r"), i, #paccache)
        io.stdout:flush()
    end

    printf(C.greb("Real space used by installed packages: ")) printf(C.yelb("%d M \27[K"), totalsize / 1024^2)
end

local function get_theoretical_size(paccache)
    local totalsize = 0
    for i, pkg in ipairs(paccache) do
        totalsize = totalsize + pkg:pkg_get_isize()
        printf(C.greb("\rTheoretical space used by installed packages: ")) printf(C.yelb("%d M"), totalsize / 1024^2) printf(C.greb(" progression ")) printf(C.yelb("%d/%d\r"), i, #paccache)

        io.stdout:flush()
    end

    printf(C.greb("Theoretical space used by installed packages: ")) printf(C.yelb("%d M \27[K"), totalsize / 1024^2)
end

local function get_size_cachedirs()
    local cachedirs = alpm.option_get_cachedirs()
    local totalsize = 0
    for i, cachedir in ipairs(cachedirs) do
        for file in lfs.dir(cachedir) do
            if ((file ~= ".") and (file ~= "..")) then
                local filepath = cachedir.."/"..file
                local attr = lfs.symlinkattributes(filepath)
                if (attr and (attr.mode == "file" or attr.mode == "link")) then
                    totalsize = totalsize + attr.size
                end
            end
        end
    end

    printf(C.yelb("%d M"), totalsize / 1024^2)
end

local function list_package_numbers(syncdbs, paccache)
    local tblinsert = table.insert
    local reponames = {}
    local packagetbl = {}

    for i, repo in ipairs(syncdbs) do
        local pkgcache = repo:db_get_pkgcache()
        local dbname = repo:db_get_name()
        reponames[dbname] = 0
        for l, pkg in ipairs(pkgcache) do
            packagetbl[pkg:pkg_get_name()] = dbname
        end
    end

    for i, pkg in ipairs(paccache) do
        if (packagetbl[pkg:pkg_get_name()]) then
            reponames[packagetbl[pkg:pkg_get_name()]] = reponames[packagetbl[pkg:pkg_get_name()]] + 1
        end
    end

    local displaystring = ""
    local numinrepos = 0
    for i, db in ipairs(syncdbs) do
        local repos = db:db_get_name()
        if reponames[repos] then
            displaystring = displaystring..string.format("%s %s,@", repos, C.yelb("("..reponames[repos]..")"))
            numinrepos = numinrepos + reponames[repos]
        end

    end

    local others = #paccache - numinrepos
    displaystring = displaystring:sub(1, #displaystring - 1).."@others* ".. C.yelb("("..others..")")
    local displaytbl = strsplit(displaystring, "@")

    list_display("", displaytbl, true, 13)
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
    if (pkg:pkg_get_reason() ~= "P_R_DEPEND") then
        return false
    end

    if (not is_unrequired(pkg)) then
        return false
    end

    return true
end

local function get_orphans(paccache)
    local ret = {}
    for i, pkg in ipairs(paccache) do
        if (filter(pkg)) then
            tblinsert(ret, pkg:pkg_get_name())
        end
    end

    return ret
end

local function get_explicit(paccache)
    local ret = 0
    for i, pkg in ipairs(paccache) do
        if ("P_R_EXPLICIT" == pkg:pkg_get_reason()) then
            ret = ret + 1
        end
    end

    return ret
end

local function get_dependencies(paccache)
    local ret = 0
    for i, pkg in ipairs(paccache) do
        if ("P_R_DEPEND" == pkg:pkg_get_reason()) then
            ret = ret + 1
        end
    end

    return ret
end

function packagestats(db_local)
    local pkgcache = db_local:db_get_pkgcache()
    local orphans = get_orphans(pkgcache)
    local syncdbs = alpm.option_get_syncdbs()
    local ignorepkgs = alpm.option_get_ignorepkgs()
    local ignoregrps = alpm.option_get_ignoregrps()
    local cols = util.getcols()
    printf(C.blub(("-"):rep(cols)).."\n")
    --printf(C.blub(" ---------------------------------------------\n"))
    local header = "    Archlinux Core Dump        (clyde)"
    local colored = C.bright("    Archlinux Core Dump    ")..C.greb("    (clyde)")
    printf(C.blub("|")..(" "):rep(math.floor((cols / 2) - (#header / 2) - 1))..colored..(" "):rep(math.floor((cols / 2) - (#header / 2) - 1)))
    if (cols%2 ~= 0) then
        printf(C.blub(" |\n"))
    else
        printf(C.blub("|\n"))
    end
    --printf(C.blub("|")..C.bright("  Archlinux Core Dump               ")..C.greb("(clyde)")..C.blub("  |\n"))
    printf(C.blub(("-"):rep(cols)).."\n")
    --printf(C.blub(" ---------------------------------------------\n"))
    printf("\n")
    printf("\n")
    printf(C.blub(("-"):rep(cols)).."\n")
    --printf(C.blub("-----------------------------------------------\n"))
    printf(C.greb("Total installed packages: %s\n"), C.yelb(#pkgcache))
    printf(C.greb("Explicitly installed packages: %s\n"), C.yelb(get_explicit(pkgcache)))
    printf(C.greb("Packages installed as dependencies: %s\n"), C.yelb(get_dependencies(pkgcache)))

    printf(C.redb("There are %s"..C.redb(" packages no longer used by any other package:\n")), C.yelb(#orphans))
    list_display("", orphans, true)
    printf("\n")
    printf(C.blub(("-"):rep(cols)).."\n")
    printf(C.greb("HoldPkgs: %s\n"), C.yelb(#config.holdpkg))
    list_display("", config.holdpkg, true)
    printf(C.greb("IgnorePkgs: %s\n"), C.yelb(#ignorepkgs))
    list_display("", ignorepkgs, true)
    printf(C.greb("IgnoreGroups: %s\n"), C.yelb(#ignoregrps))
    list_display("", ignoregrps, true)
    printf("\n")
    printf(C.blub(("-"):rep(cols)).."\n")
    --printf(C.blub("-----------------------------------------------\n"))

    printf(C.greb("Number of configured repositories: %s\n"), C.yelb(#syncdbs))
    printf(C.greb("Number of installed packages from each repository:\n"))
    list_package_numbers(syncdbs, pkgcache)
    printf("\n")
    printf("*others are packages installed from local builds or AUR Unsupported\n")
    printf("\n")
    printf(C.blub(("-"):rep(cols)).."\n")
    --printf(C.blub("-----------------------------------------------\n"))
    get_theoretical_size(pkgcache)
        printf("\n")
    get_real_size(pkgcache)
        printf("\n")
    printf(C.greb("Space used by pkg downloaded in cache (cachedirs): ")) get_size_cachedirs()
        printf("\n")
    printf(C.greb("Space used by src downloaded in cache: ")) printf(C.yelb("null\n"))
end
