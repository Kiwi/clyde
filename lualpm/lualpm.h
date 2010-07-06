#ifndef _LUALPM_H
#define _LUALPM_H

#include <lua.h>

/* DATA TYPES ***************************************************************/

typedef enum types {
    STRING,
    PMDB_T,
    PMPKG_T,
    PMDELTA_T,
    PMGRP_T,
    PMTRANS_T,
    PMDEPEND_T,
    PMDEPMISSING_T,
    PMCONFLICT_T,
    PMFILECONFLICT_T
} types;

typedef struct {
    char const *name;
    int value;
} constant_t;

typedef struct changelog {
    void *fp;
    pmpkg_t *pkg;
    char *buffer;
} changelog;

/* The pkgreason_t enum is either 0 or 1 ... map these to strings */
const char * PKGREASON_TOSTR[] = { "P_R_EXPLICIT", "P_R_DEPEND" };
#define PKGREASON_COUNT 2

/* DATA TYPE FUNCTIONS */

pmdb_t **push_pmdb_box(lua_State *L);
pmpkg_t **push_pmpkg_box(lua_State *L);
pmdelta_t **push_pmdelta_box(lua_State *L);
pmgrp_t **push_pmgrp_box(lua_State *L);
pmtrans_t **push_pmtrans_box(lua_State *L);
pmdepend_t **push_pmdepend_box(lua_State *L);
pmdepmissing_t **push_pmdepmissing_box(lua_State *L);
pmconflict_t **push_pmconflict_box(lua_State *L);
pmfileconflict_t **push_pmfileconflict_box(lua_State *L);
changelog * push_changelog_box(lua_State *L);

#define push_pmdb(L, v)           push_pmdb_box(L)[0] = (v)
#define push_pmpkg(L, v)          push_pmpkg_box(L)[0] = (v)
#define push_pmdelta(L, v)        push_pmdelta_box(L)[0] = (v)
#define push_pmgrp(L, v)          push_pmgrp_box(L)[0] = (v)
#define push_pmtrans(L, v)        push_pmtrans_box(L)[0] = (v)
#define push_pmdepend(L, v)       push_pmdepend_box(L)[0] = (v)
#define push_pmdepmissing(L, v)   push_pmdepmissing_box(L)[0] = (v)
#define push_pmconflict(L, v)     push_pmconflict_box(L)[0] = (v)
#define push_pmfileconflict(L, v) push_pmfileconflict_box(L)[0] = (v)

void * check_box(lua_State *L, int narg, char const *typename);
#define check_pmdb(L, narg) check_box((L), (narg), "pmdb_t")
#define check_pmpkg(L, narg) check_box((L), (narg), "pmpkg_t")
#define check_pmdelta(L, narg) check_box((L), (narg), "pmdelta_t")
#define check_pmgrp(L, narg) check_box((L), (narg), "pmgrp_t")
#define check_pmdepend(L, narg) check_box((L), (narg), "pmdepend_t")
#define check_pmdepmissing(L, narg) check_box((L), (narg), "pmdepmissing_t")
#define check_pmconflict(L, narg) check_box((L), (narg), "pmconflict_t")
#define check_pmfileconflict(L, narg) check_box((L), (narg), "pmfileconflict_t")

/* TRANSACTION FLAG CONSTANTS */

int push_constant( lua_State *L, int value, constant_t const constants[] );
int push_transflag( lua_State *L, pmtransflag_t f );
int push_transflags_table( lua_State *L, pmtransflag_t flags );

int check_constant( lua_State *L, int narg, constant_t const constants[],
                    char const *extramsg );
pmtransflag_t check_transflag( lua_State *L, int narg );
pmtransflag_t check_transflags_table( lua_State *L, int narg );

#define Constants(name) constant_t const name ##_constants[] = {
#define EndConstants { NULL, 0 } };

Constants(transflag)
#define Sym(x) { "T_F_" #x, PM_TRANS_FLAG_ ## x }
    Sym(NODEPS),
    Sym(FORCE),
    Sym(NOSAVE),
    Sym(CASCADE),
    Sym(RECURSE),
    Sym(DBONLY),
    Sym(ALLDEPS),
    Sym(DOWNLOADONLY),
    Sym(NOSCRIPTLET),
    Sym(NOCONFLICTS),
    Sym(NEEDED),
    Sym(ALLEXPLICIT),
    Sym(UNNEEDED),
    Sym(RECURSEALL),
    Sym(NOLOCK),
#undef Sym
EndConstants

int push_string(lua_State *L, char const *s);
int raise_last_pm_error(lua_State *L);
alpm_list_t *lstring_table_to_alpm_list(lua_State *L, int narg);
int push_typed_object(lua_State *L, types value_type, void *value);
int alpm_list_to_any_table(lua_State *L, alpm_list_t *list,
                           enum types value_type);

/* CONSTANTS ****************************************************************/

/* CALLBACKS ****************************************************************/

extern lua_State *GlobalState;

void register_callback(lua_State *L, callback_key_t *key, int narg);
void get_callback(lua_State *L, callback_key_t *key);
void handle_pcall_error_unprotected( lua_State *L, int err,
                                     const char *context);




#endif
