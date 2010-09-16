#include <lauxlib.h>

/*
 * This is basically just to be able to do
 *  require "clydelib.signal"
 * and still keep the original lsignal.c
 */

int luaopen_signal (lua_State *L);

int luaopen_clydelib_signal (lua_State *L) {
    return luaopen_signal(L);
}
