#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/utsname.h>

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

/* Copied from clydelib */
static int luaur_arch ( lua_State *L )
{
    struct utsname un;
    uname( &un );
    lua_pushstring( L, un.machine );
    return 1;
}

static luaL_Reg const luaur_core_funcs[] = {
    { "setenv", luaur_setenv },
    { "arch",   luaur_arch   },
    { NULL,     NULL         }
};


int luaopen_luaur_core ( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, luaur_core_funcs );
    return 1;
}
