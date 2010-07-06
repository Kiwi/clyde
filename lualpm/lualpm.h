#ifndef _LUALPM_H
#define _LUALPM_H

#include <lua.h>

#include "types.h"

/* CALLBACKS ****************************************************************/

extern lua_State *GlobalState;

void register_callback(lua_State *L, callback_key_t *key, int narg);
void get_callback(lua_State *L, callback_key_t *key);
void handle_pcall_error_unprotected( lua_State *L, int err,
                                     const char *context);

#endif

