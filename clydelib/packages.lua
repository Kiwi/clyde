local alpm = require "lualpm"
local lfs = require "lfs"
local util = require "clydelib.util"
local utilcore = require "clydelib.utilcore"
local aur = require "clydelib.aur"
local printf = util.printf
local basename = util.basename
local cleanup = util.cleanup
local tblinsert = util.tblinsert
local realpath = util.realpath
local strsplit = util.strsplit
local string_display = util.string_display
local list_display = util.list_display
local list_display_linebreak = util.list_display_linebreak
--local indentprint = utilcore.indentprint
local pm_targets = pm_targets

module(..., package.seeall)

--dump pkg backups
function dump_pkg_backups(pkg)
    local root = alpm.option_get_root()
    local backups = pkg:pkg_get_backup()
    printf("Backup Files:\n")
    if (next(backups)) then
        for i, str in ipairs(backups) do
--            local cont
            repeat
                local path, msum = str:match("(%S+)%s+(%S+)")
                if not path then
                --cont = true
                    break
                end
                path = root .. path
                if (utilcore.access(path, "R_OK") == 0) then
                    local md5sum = alpm.compute_md5sum(path)
                    if (not md5sum) then
                        printf("error: could not calculate checksums for %s\n", path)
--                        cont = true
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
       -- if not cont then break end
        end
    else
        printf("(none)\n")
    end
end



--dump pkg full
function dump_pkg_full(pkg, level)
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
        --depstrings[#depstrings + 1] = dep:pkg_get_name()
        depstrings[#depstrings + 1] = dep:dep_compute_string()
        --print(dep:pkg_get_name())

--        TODO: add compute_string to lualpm so this can work >.>
    end

    if (level > 0) then
        requiredby = pkg:pkg_compute_requiredby()
    end
    string_display("Name           :", pkg:pkg_get_name())
    string_display("Version        :", pkg:pkg_get_version())
    string_display("URL            :", pkg:pkg_get_url())
    list_display("Licenses       :", pkg:pkg_get_licenses())
    list_display("Groups         :", pkg:pkg_get_groups())
    list_display("Provides       :", pkg:pkg_get_provides())
    list_display("Depends On     :", depstrings)
    list_display_linebreak("Optional Deps  :", pkg:pkg_get_optdepends())
    if (level > 0) then
        list_display("Required By    :", requiredby)
        requiredby = nil
    end
    list_display("Conflicts With :", pkg:pkg_get_conflicts())
    list_display("Replaces       :", pkg:pkg_get_replaces())
    if (level < 0) then
        printf("Download Size  : %6.2f K\n", pkg:pkg_get_size() / 1024)
    end
    if (level == 0) then
        printf("Compressed Size: %6.2f K\n", pkg:pkg_get_size() / 1024)
    end
    printf("Installed Size : %6.2f K\n", pkg:pkg_get_isize() / 1024)
    string_display("Packager       :", pkg:pkg_get_packager())
    string_display("Architecture   :", pkg:pkg_get_arch())
    string_display("Build Date     :", bdatestr)
    if (level > 0) then
        string_display("Install Date   :", idatestr)
        string_display("Install Reason :", reason)
    end
    if (level >= 0) then
        local scriptlet = pkg:pkg_has_scriptlet()
        if scriptlet == 1 then scriptlet = "Yes" else scriptlet = "No" end
        string_display("Install Script :", scriptlet)
    end
    if (level < 0) then
        string_display("MD5 Sum        :", pkg:pkg_get_md5sum())
    end
    string_display("Description    :", pkg:pkg_get_desc())
    if (level > 1) then
        dump_pkg_backups(pkg)
    end
    --TODO add dump_pkg_backups
    printf("\n")
    depstrings = nil
end


--dump pkg sync
function dump_pkg_sync(pkg, treename)
    if (pkg == nil) then
        return
    end
    string_display("Repository     :", treename)
    dump_pkg_full(pkg, -1)
end


--dump pkg full
function dump_pkg_full_aur(pkg, level)
    if (not pkg or not level) then
        return
    end
    local reason, bdatestr, idatestr, bdate, idate, requiredby, depstrings
    local pkgbuildurl = string.format("http://aur.archlinux.org/packages/%s/%s/PKGBUILD", pkg,
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

    local function gpat(field)
        return strsplit(gpa(field), " ") or {}
    end

--    bdate = pkg:pkg_get_builddate()
--    if (bdate) then
--        bdatestr = os.date("%a %d %b %Y %r %Z", bdate)
--    end
--    idate = pkg:pkg_get_installdate()
--    if (idate) then
--        idatestr = os.date("%a %d %b %Y %r %Z", idate)
--    end

--    local ireason = pkg:pkg_get_reason()

--    if (ireason == "P_R_EXPLICIT") then
--        reason = "Explicitly installed"
--    elseif (ireason == "P_R_DEPEND") then
--        reason = "Installed as a dependency for another package"
--    else
--        reason = "Unknown"
--    end

--    local depends = pkg:pkg_get_depends()
--    depstrings = {}
--    for i, dep in ipairs(depends) do
        --depstrings[#depstrings + 1] = dep:pkg_get_name()
--        depstrings[#depstrings + 1] = dep:dep_compute_string()
        --print(dep:pkg_get_name())

--        TODO: add compute_string to lualpm so this can work >.>
--    end

--    if (level > 0) then
--        requiredby = pkg:pkg_compute_requiredby()
--    end
    string_display("Name           :", pkg)
    string_display("Version        :", gpa("pkgver").."-"..gpa("pkgrel"))
    string_display("URL            :", gpa("url"))
    list_display("Licenses       :", gpat("license"))
    list_display("Groups         :", gpat("groups"))
    list_display("Provides       :", gpat("provides"))
    list_display("Depends On     :", gpat("depends"))
    list_display("Make Depends   :", gpat("makedepends"))
    list_display_linebreak("Optional Deps  :", gpat("optdepends"))
--    if (level > 0) then
--        list_display("Required By    :", requiredby)
--        requiredby = nil
--    end
    list_display("Conflicts With :", gpat("conflicts"))
    list_display("Replaces       :", gpat("replaces"))
--    if (level < 0) then
--        printf("Download Size  : %6.2f K\n", pkg:pkg_get_size() / 1024)
--    end
--    if (level == 0) then
--        printf("Compressed Size: %6.2f K\n", pkg:pkg_get_size() / 1024)
--    end
--    printf("Installed Size : %6.2f K\n", pkg:pkg_get_isize() / 1024)
--    string_display("Packager       :", pkg:pkg_get_packager())
    string_display("Architecture   :", gpa("arch"))
--    string_display("Build Date     :", bdatestr)
--    if (level > 0) then
--        string_display("Install Date   :", idatestr)
--        string_display("Install Reason :", reason)
--    end
--    if (level >= 0) then
--        local scriptlet = pkg:pkg_has_scriptlet()
--        if scriptlet == 1 then scriptlet = "Yes" else scriptlet = "No" end
--        string_display("Install Script :", scriptlet)
--    end
--    if (level < 0) then
--        string_display("MD5 Sum        :", pkg:pkg_get_md5sum())
--    end
    string_display("Description    :", gpa("pkgdesc"))
--    if (level > 1) then
--        dump_pkg_backups(pkg)
--    end
    --TODO add dump_pkg_backups
    printf("\n")
--    depstrings = nil
end


function dump_pkg_sync_aur(pkg)
    if (pkg == nil) then
        return
    end
    string_display("Repository     :", "aur")
    dump_pkg_full_aur(pkg, -1)
end

--dump pkg files
function dump_pkg_files(pkg, quiet)
    local pkgname = pkg:pkg_get_name()
    local pkgfiles = pkg:pkg_get_files()
    local root = alpm.option_get_root()

    for i, file in ipairs(pkgfiles) do
        if (not quiet) then
            printf( "%s %s%s\n", pkgname, root, file)
        else
            printf("%s%s\n", root, file)
        end
    end
end


--dump pkg changelog
function dump_pkg_changelog(pkg)
    local changelog = pkg:pkg_changelog_open()

    if (changelog == nil) then
        printf("error: no changelog available for '%s'.\n", pkg:pkg_get_name())
        return
    else
        --ret = 0
        repeat
            local buff, ret = pkg:pkg_changelog_read(4096, changelog)
        --if (ret < 4096) then
        --    buff = buff:sub(1,ret)
        --end
            printf("%s", buff)
        until ret == 0
    end

    pkg:pkg_changelog_close(changelog)
    printf("\n")
end



