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

struct log_cb_args {
    pmloglevel_t level;
    const char  *msg;
};

struct dl_cb_args {
    const char  *filename;
    off_t       xfered;
    off_t       total;
};

struct totaldl_cb_args {
    off_t       total;
};

extern lua_State *GlobalState;

void register_callback(lua_State *L, callback_key_t *key, int narg);
void get_callback(lua_State *L, callback_key_t *key);
void handle_pcall_error_unprotected( lua_State *L, int err,
                                     const char *context);

extern callback_key_t log_cb_key[1];
extern callback_key_t dl_cb_key[1];
extern callback_key_t totaldl_cb_key[1];

void
log_cb_gateway_unprotected(pmloglevel_t level, char *fmt, va_list list);
void
dl_cb_gateway_unprotected(const char *f, off_t x, off_t t);
void
totaldl_cb_gateway_unprotected(off_t t);

/* DEPENDENCY FUNCTIONS ******************************************************/
/* See dep.c */

int lalpm_depcmp(lua_State *L);
int lalpm_checkdeps(lua_State *L);
int lalpm_deptest(lua_State *L);

#endif

