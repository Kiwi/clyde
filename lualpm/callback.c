#include <stdio.h>
#include <assert.h>
#include <lua.h>
#include "callback.h"

/* A global is required for alpm C -> Lua gateways to get their Lua
 * goodness from.  Unfortunately alpm callbacks have no userdata or
 * other pointer which the client could use to send their own
 * parameters in. */

/* This can probably be fixed by using lua's c-closures but maybe later...
   -JD */

lua_State *GlobalState;

void
cb_log_error ( const char *cbname, const char *message )
{
    /* TODO: send the error message to the libalpm logger here? */
    fprintf(stderr, "lualpm (%s callback): %s\n", cbname, message);
}

void
cb_register ( lua_State *L, callback_key_t *key )
{
    lua_pushlightuserdata( L, key );
    lua_pushvalue( L, -2 );
    lua_settable( L, LUA_REGISTRYINDEX );
}

int
cb_lookup ( callback_key_t *key )
{
    lua_pushlightuserdata( GlobalState, key );
    lua_gettable( GlobalState, LUA_REGISTRYINDEX );

    if ( lua_isnil( GlobalState, -1 )) {
        cb_log_error( key->name, "Value for callback is nil!" );
        return 0;
    }
    else if ( lua_type( GlobalState, -1 ) != LUA_TFUNCTION ) {
        cb_log_error( key->name, "Value for callback is not a function!" );
        return 0;
    }

    return 1;
}

void
cb_error_handler ( const char *cbname, int err )
{
    switch (err) {
    case 0:                     /* success */
        break;
    case LUA_ERRMEM:
        cb_log_error( cbname, "ran out of memory running Lua" );
        break;
    case LUA_ERRERR:
    case LUA_ERRRUN:
        if (lua_type(GlobalState, -1) == LUA_TSTRING) {
            const char *msg = lua_tostring(GlobalState, -1);
            cb_log_error( cbname, msg );
        }
        else {
            cb_log_error( cbname,
                          "error running Lua "
                          "(received a non-string error object)" );
        }
        break;
    default:
        cb_log_error( cbname, "unknown error while running Lua" );
    }
}

/****************************************************************************/

/* Use these macros to define a callback.
   The macros create a key variable called "cb_key_$NAME"
   A C function that can be passed as a function pointer
   is created, named "cb_cfunc_$NAME".
*/ 

#define BEGIN_CALLBACK( NAME, ... ) \
    callback_key_t cb_key_ ## NAME = { #NAME " callback" }; \
                                                            \
    void cb_cfunc_ ## NAME ( __VA_ARGS__ )                  \
    {                                                       \
    lua_State *L = GlobalState;                             \
    if ( ! cb_lookup( &cb_key_ ## NAME )) { return; }

#define END_CALLBACK( NAME, ARGCOUNT )                      \
    int lua_err = lua_pcall( L, ARGCOUNT, 0, 0 );           \
    if ( lua_err != 0 ) {                                   \
        cb_error_handler( #NAME, lua_err );                 \
    }                                                       \
    return;                                                 \
    } /* end of cb_cfunc */

BEGIN_CALLBACK( log, pmloglevel_t level, char *fmt, va_list vargs )
{
    char * fmted = NULL;
    int err      = vasprintf( &fmted, fmt, vargs );
    assert( err != -1 && "[BUG] vasprintf ran out of memory" );
    
    lua_pushstring( L, fmted );
    push_loglevel( L, level );
}
END_CALLBACK( log, 2 )

BEGIN_CALLBACK( dl, const char *filename, off_t xfered, off_t total )
{
    lua_pushstring( L, filename );
    lua_pushnumber( L, xfered );
    lua_pushnumber( L, total );
}
END_CALLBACK( dl, 3 )    

BEGIN_CALLBACK( totaldl, off_t total )
{
    lua_pushnumber( L, total );
}
END_CALLBACK( totaldl, 1 )

