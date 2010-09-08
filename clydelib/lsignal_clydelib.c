#include <lauxlib.h>

int luaopen_signal (lua_State *L);

int luaopen_clydelib_signal (lua_State *L) {
    return luaopen_signal(L);
}
