#include <lua.h>
#include "lualpm.h";

/* A global is required for alpm C -> Lua gateways to get their Lua
 * goodness from.  Unfortunately alpm callbacks have no userdata or
 * other pointer which the client could use to send their own
 * parameters in. */
lua_State *GlobalState;

/* We use addresses of structs describing a callback as the key to a
 * callback in the Lua registry. */
typedef struct {
    const char *name;
} callback_key_t;

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

