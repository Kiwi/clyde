#include <stdio.h>
#include <assert.h>
#include <lua.h>
#include "callback.h"

/* A global is required for alpm C -> Lua gateways to get their Lua
 * goodness from.  Unfortunately alpm callbacks have no userdata or
 * other pointer which the client could use to send their own
 * parameters in. */
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
    
    push_loglevel( L, level );
    push_string( L, fmted );
}
END_CALLBACK( log, 2 )

/****************************************************************************/

/* alpm_cb_download alpm_option_get_dlcb(); */
/* void alpm_option_set_dlcb(alpm_cb_download cb); */
callback_key_t dl_cb_key[1] = {{ "download progress" }};

static int
dl_cb_gateway_protected(lua_State *L)
{
    struct dl_cb_args *args = lua_touserdata(L, 1);
    cb_lookup(dl_cb_key);
    push_string(L, args->filename);
    lua_pushnumber(L, args->xfered);
    lua_pushnumber(L, args->total);
    lua_call(L, 3, 0);

    return 0;
}

void
dl_cb_gateway_unprotected(const char *f, off_t x, off_t t)
{
    lua_State *L = GlobalState;
    struct dl_cb_args args[1];
    int err;
    assert(L && "[BUG] no global Lua state in download progress callback");
    args->filename = f;
    args->xfered = x;
    args->total = t;
    err = lua_cpcall(L, dl_cb_gateway_protected, args);
    if (err) {
        cb_error_handler("download", err);
    }
}

/****************************************************************************/

/* alpm_cb_totaldl alpm_option_get_totaldlcb(); */
/* void alpm_option_set_totaldlcb(alpm_cb_totaldl cb); */
callback_key_t totaldl_cb_key[1] = {{ "total download progress" }};

static int
totaldl_cb_gateway_protected(lua_State *L)
{
    struct totaldl_cb_args *args = lua_touserdata(L, 1);
    cb_lookup(totaldl_cb_key);
    lua_pushnumber(L, args->total);
    lua_call(L, 1, 0);

    return 0;
}

void
totaldl_cb_gateway_unprotected(off_t t)
{
    lua_State *L = GlobalState;
    struct totaldl_cb_args args[1];
    int err;
    assert(L && "[BUG] no global Lua state in total download progress callback");
    args->total = t;
    err = lua_cpcall(L, totaldl_cb_gateway_protected, args);
    if (err) {
        cb_error_handler("total download", err);
    }
}
