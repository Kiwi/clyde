module(..., package.seeall)
local alpm = require "lualpm"
local util = require "clydelib.util"
local callback = require "clydelib.callback"
local printf = util.printf

function trans_init(ttype, flags)
    local ret  = alpm.trans_init(ttype, flags, callback.cb_trans_evt, callback.cb_trans_conv, callback.cb_trans_progress)
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
