#include <stdio.h>
#include <assert.h>
#include <lua.h>
#include "lualpm.h"

/* A global is required for alpm C -> Lua gateways to get their Lua
 * goodness from.  Unfortunately alpm callbacks have no userdata or
 * other pointer which the client could use to send their own
 * parameters in. */
lua_State *GlobalState;

void
register_callback(lua_State *L, callback_key_t *key, int narg)
{
    lua_pushlightuserdata(L, key);
    lua_pushvalue(L, narg);
    lua_settable(L, LUA_REGISTRYINDEX);
}

void
get_callback(lua_State *L, callback_key_t *key)
{
    lua_pushlightuserdata(L, key);
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (lua_isnil(L, -1)) {
        luaL_error(L, "no %s callback set!", key->name);
    }
}

static void
log_internal_error(const char *message, const char *context)
{
    /* TODO: send the error message to the libalpm logger here? */
    fprintf(stderr, "lualpm %s: %s\n", context, message);
}

void
handle_pcall_error_unprotected(
    lua_State  *L,
    int         err,
    const char *context)
{
    switch (err) {
    case 0:                     /* success */
        break;
    case LUA_ERRMEM:
        log_internal_error("lualpm: ran out of memory calling Lua", context);
        break;
    case LUA_ERRERR:
    case LUA_ERRRUN:
        /* If a pcall fails it will push an error message or error
         * object on the stack.  The following usage of lua_type()
         * and lua_tostring() is safe by inspection of the Lua source
         * code -- this particular usage cannot throw Lua errors. */
        if (lua_type(L, -1) == LUA_TSTRING) {
            char const *msg = lua_tostring(L, -1);
            log_internal_error(msg, context);
        }
        else {
            log_internal_error("lualpm: error calling Lua "
                               "(received a non-string error object)",
                               context);
        }
        break;
    default:
        log_internal_error("lualpm: unknown error while calling Lua",
                           context);
    }
}

/****************************************************************************/

/* alpm_cb_log alpm_option_get_logcb(); */
/* void alpm_option_set_logcb(alpm_cb_log cb); */
callback_key_t log_cb_key[1] = {{ "log callback" }};

static int
log_cb_gateway_protected(lua_State *L)
{
    struct log_cb_args *args = lua_touserdata(L, 1);
    get_callback(L, log_cb_key);
    push_loglevel(L, args->level);
    push_string(L, args->msg);
    lua_call(L, 2, 0);

    return 0;
}

void
log_cb_gateway_unprotected(pmloglevel_t level, char *fmt, va_list list)
{
    lua_State *L = GlobalState;
    struct log_cb_args args[1];
    int err;
    assert(L && "[BUG] no global Lua state in log callback");
    args->level = level;
    char *s = NULL;
    err = vasprintf(&s, fmt, list);
    assert(err != -1 && "[BUG] out of memory");
    args->msg = s;
    err = lua_cpcall(L, log_cb_gateway_protected, args);
    free(s);
    if (err) {
        handle_pcall_error_unprotected(L, err, "log callback");
    }
}

/****************************************************************************/

/* alpm_cb_download alpm_option_get_dlcb(); */
/* void alpm_option_set_dlcb(alpm_cb_download cb); */
callback_key_t dl_cb_key[1] = {{ "download progress" }};

static int
dl_cb_gateway_protected(lua_State *L)
{
    struct dl_cb_args *args = lua_touserdata(L, 1);
    get_callback(L, dl_cb_key);
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
        handle_pcall_error_unprotected(L, err, "download callback");
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
    get_callback(L, totaldl_cb_key);
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
        handle_pcall_error_unprotected(L, err, "total download callback");
    }
}

