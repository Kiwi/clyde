#ifndef _LUALPM_CALLBACK_H
#define _LUALPM_CALLBACK_H

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

void cb_register ( lua_State *L, callback_key_t *key );
int cb_lookup ( callback_key_t *key );
void cb_log_error ( const char *context, const char *message );
void cb_error_handler ( const char *cbname, int err );

/* GENERIC ALPM CALLBACKS */

extern callback_key_t cb_key_log;
extern callback_key_t cb_key_dl;
extern callback_key_t cb_key_totaldl;
extern callback_key_t cb_key_fetch;

void cb_cfunc_log     ( pmloglevel_t level, char *fmt, va_list list );
void cb_cfunc_dl      ( const char *filename, off_t xfered, off_t total );
void cb_cfunc_totaldl ( off_t total );
int  cb_cfunc_fetch   ( const char *url, const char *localpath, int force );

/* TRANSACTION CALLBACKS */

extern callback_key_t transcb_key_event;
extern callback_key_t transcb_key_conv;
extern callback_key_t transcb_key_progress;

void transcb_cfunc_event    ( pmtransevt_t event, void *arg_one,
                              void *arg_two );
void transcb_cfunc_conv     ( pmtransconv_t type, void *arg_one,
                              void *arg_two, void *arg_three, int *response );
void transcb_cfunc_progress ( pmtransprog_t type, const char * desc,
                              int item_progress, int total_count,
                              int total_pos );

#endif
