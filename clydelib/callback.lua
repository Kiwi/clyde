--local util = require "clydelib.util"
--local printf = util.printf
local utilcore = require "clydelib.utilcore"
local g = utilcore.gettext
colorize = require "clydelib.colorize"
local socket = require "socket"
module(...,package.seeall)

local function printf(...)
    io.write(string.format(...))
end

local function getcols()
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

local rate_last
local xfered_last
local list_xfered = 0
local list_total = 0
local initial_time


local prevpercent = 0

local on_progress = 0
local output

local last_time = 0
function get_update_timediff(first_call)
    local retval = 0.0
    return function(first_call)
        if(first_call) then
            last_time = socket.gettime()
        else
            local this_time = socket.gettime()
            local diff_time = this_time - last_time
            retval = diff_time

            if (retval < .2) then
                retval = 0.0
            else
                last_time = this_time
                --print("update retval: "..retval)
            end
        end

        return retval
    end
end


local lasthash, mouth = 0, 0
function fill_progress(bar_percent, disp_percent, proglen)
    local C = colorize

        local hashlen = proglen - 8
        local hash = math.floor(bar_percent * hashlen / 100)

        if (bar_percent == 0) then
            lasthash = 0
            mouth = 0
        end

        if (proglen > 8) then
            printf(" [")
            for i = hashlen, 1, -1 do
                if (config.chomp) then
                    if (i > hashlen - hash) then
                        printf("-")
                    elseif (i == hashlen - hash ) then
                        if (lasthash == hash) then
                            if (mouth ~= 0) then
                                printf(C.yel(string.char(0xe2, 0x88, 0xa9)).." "..C.yelb("C"))
--                                printf("\27[1;33m∩ C\27[m")
                            else
                                printf(C.yel(string.char(0xe2, 0x88, 0xa9)).." "..C.yelb("c"))
--                                printf("\27[1;33m∩ c\27[m")
                            end
                        else
                            lasthash = hash
                            if mouth == 1 then mouth = 0 else mouth = 1 end
                            if (mouth ~= 0) then
                                printf(C.yel(string.char(0xe2, 0x88, 0xa9)).." "..C.yelb("C"))
--                                printf("\27[1;33m∩ C\27[m")
                            else
                                printf(C.yel(string.char(0xe2, 0x88, 0xa9)).." "..C.yelb("c"))
--                                printf("\27[1;33m∩ c\27[m")
                            end
                        end
                    elseif (i%3 == 0) then
                        printf(C.whi("o"))
--                        printf("\27[0;37mo\27[m")
                    else
                        printf(C.whi(" "))
--                        printf("\27[0;37m \27[m")
                    end
                elseif (i > hashlen - hash) then
                    printf("#")
                else
                    printf("-")
                end
            end
            printf("]\27[K")
        end
        if (proglen > 5) then
            if (disp_percent ~= 100 and config.chomp) then
                printf("\27[3D] %3d%% ", disp_percent)
            else
                printf(" %3d%%", disp_percent)
            end
        end
        if (bar_percent == 100) then
            printf("\n")
        else
            if (config.chomp) then
                printf("\27[1A\r")
            else
                printf("\r")
            end
        end
        io.stdout:flush()
end

function cb_trans_progress(event, pkgname, percent, howmany, remain)
    local timediff
    local infolen = 50
    local tmp, digits, textlen, opr
    local len, wclen, wcwid, padwid
    local wcstr

    if (config.noprogressar) then
        return
    end

    if (percent == 0) then
        timediff = get_update_timediff(true)()
    else
        timediff = get_update_timediff(false)()
    end

    if (percent > 0 and percent < 100 and not timediff) then
        return
    end

    if (not pkgname or percent == prevpercent) then
        return
    end

    prevpercent = percent

    local lookuptbl = {
        ["T_P_ADD_START"] = function() opr = g("installing") end;
        ["T_P_UPGRADE_START"] = function() opr = g("upgrading") end;
        ["T_P_REMOVE_START"] = function() opr = g("removing") end;
        ["T_P_CONFLICTS_START"] = function() opr = g("checking for file conflicts") end;
    }
    if (lookuptbl[event]) then
        lookuptbl[event]()
    else
        printf("error: unknown event type")
    end

    digits = #tostring(howmany)
    textlen = infolen -3 - (2 * digits)
    len = #opr + (#pkgname or 0)
    wcstr = string.format("%s %s", opr, pkgname)

    padwid = textlen - len - 3
    if (padwid <  0) then
        local mpadwid = padwid * -1
        local i = textlen - 5
        wcstr = string.sub(wcstr, 1, i)
        wcstr = wcstr.."..."
        padwid = 0
    end
    printf("(%d/%d) %s %s", remain, howmany, wcstr, string.rep(" ", padwid ))
    fill_progress(percent, percent, getcols() - infolen)

    if (percent == 100) then
        on_progress = 0
        io.stdout:flush()
    else
        on_progfress = 1
    end
end


function cb_dl_total(total)
    list_total = total
    if (total == 0) then
        list_xfered = 0
    end
end


function cb_dl_progress(filename, file_xfered, file_total)
    local infolen = 50
    local filenamelen = infolen - 27
    local fname, len, wclen, padwid, wcfname

    local totaldownload = false
    local xfered, total
    local file_percent, total_percent = 0, 0
    local rate, timediff, f_xfered = 0.0, 0.0, 0.0
    local eta_h, eta_m, eta_s = 0, 0, 0
    local rate_size, xfered_size = "K", "K"

    if (config.noprogressbar or file_total == -1) then
        if (file_xfered == 0) then
            printf(g("downloading %s...\n"), filename)
            io.stdout:flush()
        end
        return
    end

    if (config.totaldownload and list_total ~= 0) then
        if (list_xfered + file_total <= list_total) then
            totaldownload = true
        else
            list_xfered = 0
            list_total = 0
        end
    end

    if (totaldownload) then
        xfered = list_xfered + file_xfered
        total = list_total
    else
        xfered = file_xfered
        total = file_total
    end

    if (xfered > total) then
        return
    end

    if (file_xfered == 0) then
        if (not totaldownload or (totaldownload and list_xfered == 0)) then
            initial_time = socket.gettime()
            xfered_last = 0
            rate_last = 0
            timediff = get_update_timediff(true)()
        end
    elseif (file_xfered == file_total) then
        local current_time = socket.gettime()
        timediff = current_time - initial_time
        rate = xfered / (timediff * 1024)

        eta_s = math.floor(timediff + .5)
    else
        timediff = get_update_timediff(false)()

        if (timediff < .02) then
            return
        end
        rate = (xfered - xfered_last) / (timediff * 1024)
        rate = (rate + 2 * rate_last) / 3
        eta_s = math.floor((total - xfered) / (rate * 1024))
        rate_last = rate
        xfered_last = xfered
    end

    file_percent = math.floor(file_xfered / file_total * 100)

    if (totaldownload) then
        total_percent = math.floor((list_xfered + file_xfered) / list_total * 100)

        if (file_xfered == file_total) then
            list_xfered = list_xfered + file_total
        end
    end

    eta_h = math.floor(eta_s / 3600)
    eta_s = eta_s - (eta_h * 3600)
    eta_m = math.floor(eta_s / 60)
    eta_s = eta_s - (eta_m * 60)
    fname = filename
    if (fname:match("%.db%.tar%.gz$")) then
        fname = fname:match("(.+)%.db%.tar%.gz$")
    elseif (fname:match("%.pkg%.tar%.gz$")) then
        fname = fname:match("(.+)%.pkg%.tar%.gz$")
    end

    len = #filename
    wcfname = fname:sub(1, len)
    padwid = filenamelen - #wcfname
    if (padwid < 0) then
        local i = filenamelen - 3
        wcfname = wcfname:sub(1, i)
        wcfname = wcfname.."..."
        padwid = 0
    end

    if (rate > 2048) then
        rate = rate / 1024
        rate_size = "M"
        if (rate > 2048) then
            rate = rate / 1024
            rate_size = "G"
        end
    end

    f_xfered = xfered / 1024
    if (f_xfered > 2048) then
        f_xfered = f_xfered / 1024
        xfered_size = "M"
        if (f_xfered > 2048) then
            f_xfered = f_xfered / 1024
            xfered_size = "G"
        end
    end
    printf("%s%s %6.1f%s %6.1f%s/s %02d:%02d:%02d", wcfname, string.rep(" ", padwid),
        f_xfered, xfered_size, rate, rate_size,
        tonumber(eta_h), tonumber(eta_m), tonumber(eta_s))

    if (totaldownload) then
        fill_progress(file_percent, total_percent, getcols() - infolen)
    else
        fill_progress(file_percent, file_percent, getcols() - infolen)
    end
end
