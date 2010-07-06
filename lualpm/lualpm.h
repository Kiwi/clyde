#ifndef _LUALPM_H
#define _LUALPM_H

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "types.h"

/* CALLBACKS ****************************************************************/

/* We use addresses of structs describing a callback as the key to a
 * callback in the Lua registry. */
typedef struct {
    const char *name;
} callback_key_t;

extern lua_State *GlobalState;

void register_callback(lua_State *L, callback_key_t *key, int narg);
void get_callback(lua_State *L, callback_key_t *key);
void handle_pcall_error_unprotected( lua_State *L, int err,
                                     const char *context);

/* DEPENDENCY FUNCTIONS ******************************************************/
/* See dep.c */

int lalpm_depcmp(lua_State *L);
int lalpm_checkdeps(lua_State *L);
int lalpm_deptest(lua_State *L);



#endif

