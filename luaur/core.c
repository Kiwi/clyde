#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

static int luaur_setenv ( lua_State *L )
{
    const char * env_name  = luaL_checkstring( L, 1 );
    const char * env_value = luaL_checkstring( L, 2 );
    int retval, luaret;
    
    retval = setenv( env_name, env_value, 1 );

    if ( retval == -1 ) {
        lua_pushnil( L );
        lua_pushstring( L, strerror( errno ));
        luaret = 2;
    }
    else {
        lua_pushboolean( L, 1 );
        luaret = 1;
    }

    return luaret;
}

static luaL_Reg const luaur_core_funcs[] = {
    { "setenv", luaur_setenv },
    { NULL,     NULL         }
};


int luaopen_luaur_core ( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, luaur_core_funcs );
    return 1;
}
