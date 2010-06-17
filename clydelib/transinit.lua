module(..., package.seeall)
local alpm = require "lualpm"
local util = require "clydelib.util"
local callback = require "clydelib.callback"
local utilcore = require "clydelib.utilcore"
local printf = util.printf
local eprintf = util.eprintf
local g = utilcore.gettext

function trans_init(flags)
    local ret  = alpm.trans_init(flags, callback.cb_trans_evt, callback.cb_trans_conv, callback.cb_trans_progress)
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
