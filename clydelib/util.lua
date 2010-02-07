local alpm = require "lualpm"
local lfs = require "lfs"
local utilcore = require "clydelib.utilcore"
--local io = io
--local os = os
--local string = string
module(..., package.seeall)

--module("util")

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
    local release = alpm.release()
    --TODO  logging if fail
    if (type(config.configfile) == "userdata") then
        config.configfile:close()
    end
--    return true
    os.exit(release)
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
    local gettext = utilcore.gettext
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
        ["LOG_DEBUG"] = function() stream:write("debug: ") end;
        ["LOG_ERROR"] = function() stream:write(gettext("error: ")) end;
        ["LOG_WARNING"] = function() stream:write(gettext("warning: ")) end;
        ["LOG_FUNCTION"] = function() stream:write(gettext("function: ")) end;
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
    local gettext = utilcore.gettext
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

function trans_init(ttype, flags)
    local ret  = alpm.trans_init(ttype, flags, nil, nil, nil)
    if (ret == -1) then
        printf("error: failed to init transaction(%s)\n", alpm.strerrorlast())
        if (alpm.strerrorlast() == "unable to lock database") then
            printf("  if you're sure a package manager is not already\n"..
                  "  running, you can remove %s\n", alpm.option_get_lockfile())
        end
        return -1
    end
    return 0
end

function trans_release()
    local transrelease = alpm.trans_release()
    --print(transrelease)
    if (transrelease == -1) then
        printf("error: failed to release transaction (%s)\n", alpm.strerrorlast())
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
        --print(config.op)
        --print(config.op_s_clean, config.op_s_sync, config.group, config.op_s_info, config.op_q_list, config.op_s_search, config.op_s_printuris)
        return false
    end
end


---[[
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
                --printf(" ")
            end
            --printf(count + #words[i])
            --if remainlen + indent > cols or i == len then

                --print(count + indent)
                printf(" ")
            --end
            printf("%s", words[i])
            if i ~= len then
                printf((" "):rep(space))
            end
            count = #words[i] + space - 1
        end
    end

end

function string_display(title, string)
    local len
    if (title and type(title) == "string") then
        len = #title
        printf("%s ", title)
    end
    if (not string or #string == 0) then
        printf("None")
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

function list_display(title, list)
    local len, cols
    if (title and type(title) == "string") then
        len = #title
        printf("%s ", title)
    end
    if (not next(list) or checkempty(list)) then
        printf("%s\n", "None")
    else
        local str = table.concat(list, "  ")
        indentprint(str, len, 2)
        printf("  ")
        printf("\n")
    end
end

function list_display_linebreak(title, list)
    local len = 0
    if (title and type(title) == "string") then
        len = #title
        printf("%s ", title)
    end

    if (not next(list) or checkempty(list)) then
        printf("%s\n", "None")
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
    local g = utilcore.gettext
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

function display_synctargets(syncpkgs)
    local pkglist, rpkglist = {}, {}
    for i, pkg in ipairs(syncpkgs) do
        tblinsert(pkglist, pkg)
        local to_replace = pkg:pkg_get_removes()

        for i, rp in ipairs(to_replace) do
            tblinsert(rpkglist, rp)
        end
    end

    display_targets(rpkglist, false)
    display_targets(pkglist, true)

end






local function question(preset, fmt, ...)
    local gettext = utilcore.gettext
    local stream

    if (config.noconfirm) then
        stream = "stdout"
    else
        stream = "stderr"
    end

    local str = string.format(fmt, ...)
    fprintf(stream, fmt, ...)

    if (preset) then
        fprintf(stream, " %s ", gettext("[Y/n]"))
    else
        fprintf(stream, " %s ", gettext("[y/N]"))
    end

    if (config.noconfirm) then
        fprintf(stream, "\n")
        return preset
    end

    local answer = io.stdin:read()

    if (#answer == 0) then
        return preset
    end

    if ((not strcasecmp(answer, gettext("Y"))) or (not strcasecmp(answer, gettext("YES")))) then
        return true
    elseif ((not strcasecmp(answer, gettext("N"))) or (not strcasecmp(answer, gettext("NO")))) then
        return false
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

function getbasharray(file, str)
    return io.popen(string.format([[
        /bin/bash -c '. %s
        echo "${%s[@]}"'
        ]], file, str)):read("*l")
end

function getpkgbuildarray(carch, pkgbuild, str)
    return io.popen(string.format([[
        /bin/bash -c 'CARCH=%s
        . %s
        echo "${%s[@]}"'
        ]], carch, pkgbuild, str)):read("*l")
end
--a = mkversion "2.0.3"
--b = mkversion "2.1.0"

--print(a > b, a == b, a < b)
