module(..., package.seeall)
local alpm = require "lualpm"
local lfs = require "lfs"
local utilcore = require "clydelib.utilcore"
local signal = require "clydelib.signal"
local C = colorize
local g = utilcore.gettext

function dump(o)
    if type(o) == 'table' then
        local s = '{ '
        for k,v in pairs(o) do
                if type(k) ~= 'number' then k = '"'..k..'"' end
                s = s .. '['..k..'] = ' .. dump(v) .. ','
        end
        return s .. '} '
    else
        return tostring(o)
    end
end

function printf(...)
    io.write(string.format(...))
end

function sanitize(str)
    return str:gsub("[%^%$%(%)%%%.%[%]%+%-%?%*]", "%%%1")
end

function strcasecmp(str1, str2)
    local string1, string2

    string1 = str1:lower()
    string2 = str2:lower()

    if (string1 < string2) then
        return -1
    elseif (string1 > string2) then
        return 1
    elseif (string1 == string2) then
        return false
    else
        eprintf("LOG_ERROR", "invalid useage of strcasecmp")
    end
end

-- Split text into a list consisting of the strings in text,
-- separated by strings matching delimiter (which may be a pattern).
-- example: strsplit("Anna, Bob, Charlie,Dolores", ",%s*")
function strsplit(text, delimiter)
    local strfind = string.find
    local strsub = string.sub
    local tinsert = table.insert
  local list = {}
  local pos = 1
  if strfind("", delimiter, 1) then -- this would result in endless loops
    error("delimiter matches empty string!")
  end
  while 1 do
    local first, last = strfind(text, delimiter, pos)
    if first then -- found?
      tinsert(list, strsub(text, pos, first-1))
      pos = last+1
    else
      tinsert(list, strsub(text, pos))
      break
    end
  end
  return list
end

function strtrim(str)
    if (str == nil) or (#str == 0) then
        return str
    end
    str = str:gsub("^%s+", "")
    str = str:gsub("%s+$", "")

    return str
end

function basename(name)
    return name:match("/(%w+)$")
end

function cleanup(ret)
    if (alpm.release() == -1) then
        lprintf("LOG_ERROR", alpm.strerrorlast() .. "\n")
    end
    if (type(config.configfile) == "userdata") then
        config.configfile:close()
    end
    os.exit(ret)
end

function realpath(file)
    return lfs.currentdir() .. "/" .. file
end

function getcols()
    if (0 == utilcore.isatty(1)) then
        return 80
    else
        local tbl, ret = utilcore.ioctl(1, 21523)

        if (ret == 0) then
            return tbl.ws_col
        end
        return 80
    end
end

function tblinsert(tbl, ins)
    tbl[#tbl + 1] = ins
end

function tbljoin(tbl1, tbl2)
    local n = #tbl1
    local t = { unpack (tbl1, 1, n) }
    for i = 1, #tbl2 do
        t [n+i] = tbl2[i]
    end

    return t
end

function getbuilduser()
    return config.op_s_build_user or os.getenv("SUDO_USER") or "root"
end

function tblisin(tbl, search)
    for i, val in ipairs(tbl) do
        if (val == search) then
            return true, i
        end
    end
    return false
end

function fastremove(tbl, i)
    local size = #tbl
    tbl[i] = tbl[size]
    tbl[size] = nil
end

function tblremovedupes(tbl)
    local t = {}
    for i, search in ipairs(tbl) do
        if (not tblisin(t, search)) then
            tblinsert(t, search)
        end
    end
    return t
end

function tbldiff(tbl1, tbl2)
    local t = {}
    for i, val in ipairs(tbl1) do
        local found, indx = tblisin(tbl2, val)
        if (not found) then
            tblinsert(t, val)
        end
    end
    return t
end

function tblstrdup(tbl)
    local t = {}
    for i, val in ipairs(tbl) do
        t[i] = val
    end
    return t
end

function vfprintf(stream, level, format, ...)
    local ret = 0
    local stream = stream
    if (not tblisin(config.logmask, level)) then
        return ret
    end

    if CLYDE_DEBUG then
        if (tblisin(config.logmask, "LOG_DEBUG")) then
            local time = os.time()
            printf("[%s] ", os.date("%H:%M:%S", time))
        end
    end

    if (stream == "stderr") then
        stream = io.stderr
    elseif (stream == "stdout") then
        stream = io.stdout
    end


    local lookuptbl = {
        ["LOG_DEBUG"] = function() stream:write(C.redb("debug: ")) end;
        ["LOG_ERROR"] = function() stream:write(C.redb(g("error: "))) end;
        ["LOG_WARNING"] = function() stream:write(C.redb(g("warning: "))) end;
        ["LOG_FUNCTION"] = function() stream:write(C.redb(g("function: "))) end;
    }
    if (lookuptbl[level]) then
        lookuptbl[level]()
    else
        printf("error: invalid log level")
        return 1
    end
    stream:write(string.format(format, ...))
    return ret

end

function fprintf(stream, format, ...)
    local stream = stream

    if (stream == "stderr") then
        stream = io.stderr
    elseif (stream == "stdout") then
        stream = io.stdout
    end

    stream:write(string.format(format, ...))
end

function lprintf(level, format, ...)
    local ret = vfprintf("stdout", level, format, ...)
    return ret
end

function eprintf(level, format, ...)
    local ret = vfprintf("stderr", level, format, ...)
    return ret
end

function trans_init(flags)
    local callback = require "clydelib.callback"

    local ret  = alpm.trans_init { flags      = flags,
                                   eventscb   = callback.cb_trans_event,
                                   convcb     = callback.cb_trans_conv,
                                   progresscb = callback.cb_trans_progress }
    if (ret == -1) then
        eprintf("LOG_ERROR", g("failed to init transaction (%s)\n"), alpm.strerrorlast())
        if (alpm.pm_errno() == "P_E_HANDLE_LOCK") then
            io.stderr:write(string.format(g("  if you're sure a package manager is not already\n"..
                  "  running, you can remove %s\n"), alpm.option_get_lockfile()))
        end
        return -1
    end
    return 0
end

function trans_release()
    local transrelease = alpm.trans_release()
    if (transrelease == -1) then
        eprintf("LOG_ERROR", g("failed to release transaction (%s)\n"), alpm.strerrorlast())
        return -1
    end
    return 0
end

function needs_root()
    if (config.op == "PM_OP_UPGRADE" or config.op == "PM_OP_REMOVE" or
        (config.op == "PM_OP_SYNC" and (config.op_s_clean > 0 or config.op_s_sync > 0 or
            (config.group == 0 and not config.op_s_info and not config.op_q_list
            and not config.op_s_search and not config.op_s_printuris)))) then
        return true
    else
        return false
    end
end

function rmrf(path)
    local ret, errstr, errnum = os.remove(path)
    if (ret or errnum == 2) then
        return false
    elseif (lfs.attributes(path, "mode") == "directory") then
        for file in lfs.dir(path) do
            if file ~= "." and file ~= ".." then
                local full = path .. "/" .. file
                local attr = lfs.attributes(full)
                assert(type(attr) == "table")
                if attr.mode == "directory" then
                    rmrf(full)
                else
                    os.remove(full)
                end
            end
        end
        return not os.remove(path)
    else
        return true
    end
end

function makepath(path)
    local oldmask = utilcore.umask(0000)
    local ret = false
    local parts = strsplit(path:sub(2, #path-1), "/")
    local incr = ""

    for i, part in ipairs(parts) do
        incr = incr .. "/" .. part
        if (utilcore.access(incr, "F_OK") ~= 0) then
            if (utilcore.mkdir(incr, tonumber("755", 8)) ~= 0) then
                ret = true
                break
            end
        end
    end

    utilcore.umask(oldmask)

    return ret
end

function indentprint(str, indent, space)
    if (not str or type(str) ~= "string") then
        return
    end
    local words = {}
    for w in str:gmatch("%S+") do
        words[#words + 1] = w
    end
    local space = space or 1
    local cols = getcols()
    local count = 0
    local len = #words
    local var = 1
    local totallen = 0
    for i = 1, len do
        totallen = #words[i] + totallen
    end
    for i = 1, len do
        count = count + #words[i] + space - var
        var = 0
        if count < cols - indent - space + 1 or len == 1 then

            printf("%s", words[i])
            if i ~= len and words[i+1] and #words[i+1] + count < cols - indent - space then
                printf("%s", (" "):rep(space))
            end
        else
            printf("\n%s", (" "):rep(indent))
            local remainlen = 0
            for j = i, len do
                remainlen = remainlen + #words[j] + space
            end
                printf(" ")
            printf("%s", words[i])
            if i ~= len then
                printf((" "):rep(space))
            end
            count = #words[i] + space - 1
        end
    end

end

function string_display(title, string, adjust)
    local len = 0
    if (title and type(title) == "string") then
        adjust = adjust or 0
        len = #title - adjust
        printf("%s ", title)
    end
    if (not string or #string == 0) then
        printf(g("None"))
    else
        indentprint(string, len)
    end
    printf("\n")
end

local function checkempty(tbl)
    for i, v in ipairs(tbl) do
        if v ~= string.rep(" ", #v) then
            return false
        end
    end
    return true
end

function list_display(title, list, nospace, extracols, adjust)
    local len, cols = 0, 0
    if (title and type(title) == "string") then
        adjust = adjust or 0
        len = #title - adjust
        if (not nospace) then
            printf("%s ", title)
        end
    end

    if (not next(list) or checkempty(list)) then
        printf("%s\n", g("None"))
    else
        cols = len
        for i, str in ipairs(list) do
            local s = #str + 2
            local maxcols = getcols()

            if (s + cols > maxcols) then
                cols = len
                printf("\n")
                for j = 1, len  do
                    printf(" ")
                end
            end

            printf("%s  ", str)
            cols = cols + s - (extracols or 0)
        end
        printf("\n")
    end
end

function list_display_linebreak(title, list, adjust)
    local len = 0
    if (title and type(title) == "string") then
        adjust = adjust or 0
        len = #title - adjust
        printf("%s ", title)
    end

    if (not next(list) or checkempty(list)) then
        printf("%s\n", g("None"))
    else
        indentprint(list[1], len)
        printf("\n")
        for i = 2, #list do
            for j = 1, len  + 1 do
                printf(" ")
            end
            indentprint(list[i], len)
            printf("\n")
        end
    end
end

function display_targets(pkgs, install)
    local isize, dlsize, mbisize, mbdlsize = 0, 0, 0, 0
    local str
    local targets = {}

    if (not next(pkgs)) then
        return
    end

    printf("\n")
    for i, pkg in ipairs(pkgs) do
        dlsize = pkg:pkg_download_size() + dlsize
        isize = pkg:pkg_get_isize() + isize

        if (config.showsize) then
            local mbsize = pkg:pkg_get_size() / (1024 * 1024)
            str = string.format("%s-%s [%.2f MB]", pkg:pkg_get_name(), pkg:pkg_get_version(), mbsize)
        else
            str = string.format("%s-%s", pkg:pkg_get_name(), pkg:pkg_get_version())
        end

        tblinsert(targets, str)
    end

    mbdlsize = dlsize / (1024 * 1024)
    mbisize = isize / (1024 * 1024)

    if (install) then
        str = string.format(g("Targets (%d):"), #targets)
        list_display(str, targets)
        printf("\n")

        printf(g("Total Download Size:    %.2f MB\n"), mbdlsize)
        printf(g("Total Installed Size:   %.2f MB\n"), mbisize)
    else
        str = string.format(g("Remove (%d):"), #targets)
        list_display(str, targets)
        printf("\n")

        printf(g("Total Removed Size:   %.2f MB\n"), mbisize)
    end
end

function display_alpm_targets()
    display_targets(alpm.trans_get_remove(), false)
    display_targets(alpm.trans_get_add(), true)
end

function display_new_optdepends(oldpkg, newpkg)
    local old = oldpkg:pkg_get_optdepends()
    local new = newpkg:pkg_get_optdepends()
    local optdeps = tbldiff(new, old)
    if (next(optdeps)) then
        printf(g("New optional dependencies for %s\n"), newpkg:pkg_get_name())
        list_display_linebreak("   ", optdeps)
    end
end

function display_optdepends(pkg)
    local optdeps = pkg:pkg_get_optdepends()
    if (optdeps and next(optdeps)) then
        printf(g("Optional dependencies for %s\n"), pkg:pkg_get_name())
        list_display_linebreak("   ", optdeps)
    end
end

local function question(preset, fmt, ...)
    -- get single-character input
    local stream

    local str = string.format(fmt, ...)
    if ( preset ) then
        str = str .. " [Y/n] "
    else
        str = str .. " [y/N] "
    end

    if (config.noconfirm) then
        io.stderr:write( str )
        if ( preset ) then io.stderr:write( "Y\n" )
        else io.stderr:write( "N\n" ) end
        return preset
    end

    while ( true ) do
        io.write( str )
        local answer = string.upper( utilcore.getchar())

        if (answer == "\n") then return preset end
        print() -- print newline if the user didn't

        if answer == "Y" then return true
        elseif answer == "N" then return false
        end

        print( "Please answer 'Y', 'N', or ENTER." )
    end

    return false
end

function yesno(fmt, ...)
    local ret = question(true, fmt, ...)
    return ret
end

function noyes(fmt, ...)
    local ret = question(false, fmt, ...)
    return ret
end

local mt_version = {}

function mt_version.__lt(a, b)
        for i = 1, math.min(#a, #b) do
                if a[i] < b[i] then
                        return true;
                elseif a[i] ~= b[i] then
                        return false;
                end
        end
end

function mt_version.__eq(a, b)
        for i = 1, math.min(#a, #b) do
                if a[i] ~= b[i] then return false; end
        end
        return true;
end

function mt_version.__tostring(v)
        return table.concat(v, ".");
end

-- Create a version (table/object) from a string "a.b.c.d"
function mkversion(s)
        local t = setmetatable({}, mt_version);
        s:gsub("%P+", function (c) table.insert(t, tonumber(c)); end);
        return t;
end

--a = mkversion "2.0.3"
--b = mkversion "2.1.0"

--print(a > b, a == b, a < b)

function getbasharray(file, str)
    local fd = io.popen(string.format([[
        /bin/bash -c '. %s &> /dev/null
        echo "${%s[@]}"'
        ]], file, str))
    local ret = fd:read("*l")
    fd:close()
    return ret
end

function getpkgbuildarray(carch, pkgbuild, str)
    local fd =  io.popen(string.format([[
        /bin/bash -c 'CARCH=%s
        . %s &> /dev/null
        echo "${%s[@]}"'
        ]], carch, pkgbuild, str))
    local ret = fd:read("*l")
    fd:close()
    return ret
end

function getpkgbuildarraylinebreak(carch, pkgbuild, str)
    local fd = io.popen(string.format([[
    /bin/bash -c 'CARCH=%s
    . %s &> /dev/null
    for i in "${%s[@]}"; do
        echo "$i";
    done'
    ]], carch, pkgbuild, str))
    local ret = fd:read("*a")
    fd:close()
    ret = strsplit(ret, "\n")
    ret[#ret] = nil
    return ret
end

function getbasharrayuser(file, str, user)
    local fd =  io.popen(string.format([[
        /bin/bash -c 'export USER=%s
        . %s &> /dev/null
        echo "${%s[@]}"'
        ]], user, file, str))
    local ret = fd:read("*l")
    fd:close()
    return ret
end
