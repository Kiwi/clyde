#include <stdio.h>
#include <signal.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

static lua_State* last_L = NULL;

// static int l_signal (lua_State *L);

static void sighandler(int s) {
    printf("s=%d\n", s);
    lua_getfield(last_L, LUA_GLOBALSINDEX, "signal_handler");
    lua_pushnumber(last_L, s);
    lua_call(last_L, 1, 0);
}

static int sigtest_getchar(lua_State *L) {
    last_L = L;
    // l_signal(L);

    signal(SIGINT, SIG_DFL);

    int c = getchar();
    int result;

    if (c==EOF) {
        result = 0;
    } else {
        char string[2] = "\0\0";
        string[0] = c;
        lua_pushstring(L, string);
        result = 1;
    }

    return result;
}

static luaL_Reg const pkg_funcs[] = {
    // { "set_signal_handler", sigtest_set_signal_handler },
    { "getchar", sigtest_getchar },
    { NULL, NULL }
};


int luaopen_sigtestlib(lua_State *L) {
    lua_newtable(L);
    luaL_register(L, NULL, pkg_funcs);

    return 1;
}


