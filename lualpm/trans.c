#include <assert.h>
#include <alpm.h>
#include <alpm_list.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "types.h"
#include "lualpm.h"
#include "callback.h"

static const char * trans_flag_names[] =
    { "nodeps",
      "force",
      "nosave",
      "",
      "cascade",
      "recurse",
      "dbonly",
      "",
      "alldeps",
      "downloadonly",
      "noscriptlet",
      "noconflicts",
      "",
      "needed",
      "allexplicit",
      "unneeded",
      "recurseall",
      "nolock",
      NULL };

pmtransflag_t lualpm_totransflags( lua_State *L )
{
    pmtransflag_t result;
    int opt_shift;

    if ( ! lua_istable( L, -1 )) {
        /* XXX: Report this error in trans_init instead? Meh. */
        lua_pushstring( L, "Transaction flags must be provided as a table "
                           " with flag names as keys" );
        lua_error( L );
                     
    }

    /* Each flag has a key in the table. If the key's value is true
       the flag is enabled for the transaction. */
    result = 0;
    lua_pushnil( L );
    while ( lua_next( L, -2 ) != 0 ) {
        if ( lua_toboolean( L, -1 )) {
            opt_shift = luaL_checkoption( L, -2, NULL, trans_flag_names );
            result |= ( 1 << opt_shift );
        }
        lua_pop( L, 1 );
    }

    return result;
}

int lalpm_trans_init ( lua_State *L )
{
    pmtransflag_t flags;
    int result;

    lua_getfield( L, -1, "flags" );
    if ( ! lua_istable( L, -1 )) {
        lua_pushstring( L, "No 'flags' field was provided to trans_init!" );
        lua_error( L );
    }
    flags = lualpm_totransflags( L );
    lua_pop( L, 1 ); /* Pop the flags table off the stack. */

    /* Registering a nil value with these is alright. In fact it is
       better than running old callbacks. */

    lua_getfield( L, -1, "eventscb" );
    cb_register( L, &transcb_key_event );
    lua_getfield( L, -1, "convcb" );
    cb_register( L, &transcb_key_conv );
    lua_getfield( L, -1, "progresscb" );
    cb_register( L, &transcb_key_progress );

    result = alpm_trans_init( flags,
                              transcb_cfunc_event,
                              transcb_cfunc_conv,
                              transcb_cfunc_progress );
    lua_pushnumber(L, result);
    return 1;
}

int lalpm_trans_prepare(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    alpm_list_t *list = lstring_table_to_alpm_list(L, 1);
    const int result = alpm_trans_prepare(&list);
    lua_pushnumber(L, result);
    if (result == -1) {
        switch(pm_errno) {
            case PM_ERR_UNSATISFIED_DEPS:
                alpm_list_to_any_table(L, list, PMDEPMISSING_T);
                break;
            case PM_ERR_CONFLICTING_DEPS:
                alpm_list_to_any_table(L, list, PMCONFLICT_T);
                break;
            case PM_ERR_FILE_CONFLICTS:
                alpm_list_to_any_table(L, list, PMFILECONFLICT_T);
                break;
            default:
                break;
        }
        return 2;
    }

    return 1;
}

/* int alpm_trans_commit(alpm_list_t **data); */
int lalpm_trans_commit(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    alpm_list_t *list = lstring_table_to_alpm_list(L, 1);
    const int result = alpm_trans_commit(&list);
    lua_pushnumber(L, result);
    if (result == -1) {
        switch(pm_errno) {
            case PM_ERR_FILE_CONFLICTS:
                alpm_list_to_any_table(L, list, PMFILECONFLICT_T);
                break;
            case PM_ERR_PKG_INVALID:
            case PM_ERR_DLT_INVALID:
                alpm_list_to_any_table(L, list, STRING);
                break;
            default:
                break;
        }
        return 2;
    }

    return 1;
}

/* int alpm_trans_interrupt(void); */
int lalpm_trans_interrupt(lua_State *L)
{
    const int result = alpm_trans_interrupt();
    lua_pushnumber(L, result);

    return 1;
}

/* int alpm_trans_release(void); */
int lalpm_trans_release(lua_State *L)
{
    const int result = alpm_trans_release();
    lua_pushnumber(L, result);

    return 1;
}

/* unsigned int alpm_trans_get_flags(); */
int lalpm_trans_get_flags(lua_State *L)
{
    pmtransflag_t x = alpm_trans_get_flags();
    if (x == (pmtransflag_t)-1) {
        raise_last_pm_error(L);
    }

    return push_transflags_table(L, x);
}

/* alpm_list_t * alpm_trans_get_add(); */
int lalpm_trans_get_add(lua_State *L)
{
    alpm_list_t *list = alpm_trans_get_add();
    alpm_list_to_any_table(L, list, PMPKG_T);

    return 1;
}

/* alpm_list_t * alpm_trans_get_remove(); */
int lalpm_trans_get_remove(lua_State *L)
{
    alpm_list_t *list = alpm_trans_get_remove();
    alpm_list_to_any_table(L, list, PMPKG_T);

    return 1;
}

/* TRANSACTION ERRORS *******************************************************/

/* const char *alpm_miss_get_target(const pmdepmissing_t *miss); */
static int lalpm_miss_get_target(lua_State *L)
{
    const pmdepmissing_t *miss = check_pmdepmissing(L, 1);
    const char *result = alpm_miss_get_target(miss);
    push_string(L, result);

    return 1;
}

/* pmdepend_t *alpm_miss_get_dep(pmdepmissing_t *miss); */
static int lalpm_miss_get_dep(lua_State *L)
{
    pmdepmissing_t *miss = check_pmdepmissing(L, 1);
    pmdepend_t **box = push_pmdepend_box(L);
    *box = alpm_miss_get_dep(miss);
    if (*box == NULL) {
        lua_pushnil(L);
    }

    return 1;
}

/* const char *alpm_miss_get_causingpkg(const pmdepmissing_t *miss); */
static int lalpm_miss_get_causingpkg(lua_State *L)
{
    const pmdepmissing_t *miss = check_pmdepmissing(L, 1);
    const char *result = alpm_miss_get_causingpkg(miss);
    push_string(L, result);

    return 1;
}

pmdepmissing_t **push_pmdepmissing_box(lua_State *L)
{
    pmdepmissing_t **box = lua_newuserdata(L, sizeof(pmdepmissing_t*));
    *box = NULL;

    if (luaL_newmetatable(L, "pmdepmissing_t")) {
        static luaL_Reg const methods[] = {
            { "miss_get_target",        lalpm_miss_get_target },
            { "miss_get_dep",           lalpm_miss_get_dep },
            { "miss_get_causingpkg",    lalpm_miss_get_causingpkg },
            { NULL,                     NULL }
        };
        lua_newtable(L);
        luaL_register(L, NULL, methods);
        lua_setfield(L, -2, "__index");
        /*TODO DESTRUCTOR IF NEEDED*/
    }
    lua_setmetatable(L, -2);

    return box;
}

/* const char *alpm_conflict_get_package1(pmconflict_t *conflict); */
static int lalpm_conflict_get_package1(lua_State *L)
{
    pmconflict_t *conflict = check_pmconflict(L, 1);
    const char *result = alpm_conflict_get_package1(conflict);
    push_string(L, result);

    return 1;
}

/* const char *alpm_conflict_get_package2(pmconflict_t *conflict); */
static int lalpm_conflict_get_package2(lua_State *L)
{
    pmconflict_t *conflict = check_pmconflict(L, 1);
    const char *result = alpm_conflict_get_package2(conflict);
    push_string(L, result);

    return 1;
}

/* const char *alpm_conflict_get_reason(pmconflict_t *conflict); */
static int lalpm_conflict_get_reason(lua_State *L)
{
    pmconflict_t *conflict = check_pmconflict(L, 1);
    const char *result = alpm_conflict_get_reason(conflict);
    push_string(L, result);

    return 1;
}

pmconflict_t **push_pmconflict_box(lua_State *L)
{
    pmconflict_t **box = lua_newuserdata(L, sizeof(pmconflict_t*));
    *box = NULL;

    if (luaL_newmetatable(L, "pmconflict_t")) {
        static luaL_Reg const methods[] = {
            { "conflict_get_package1",  lalpm_conflict_get_package1 },
            { "conflict_get_package2",  lalpm_conflict_get_package2 },
            { "conflict_get_reason",    lalpm_conflict_get_reason    },
            { NULL,                     NULL }
        };
        lua_newtable(L);
        luaL_register(L, NULL, methods);
        lua_setfield(L, -2, "__index");
        /*TODO DESTRUCTOR IF NEEDED*/
    }
    lua_setmetatable(L, -2);

    return box;
}

/* const char *alpm_fileconflict_get_target(pmfileconflict_t *conflict); */
static int lalpm_fileconflict_get_target(lua_State *L)
{
    pmfileconflict_t *conflict = check_pmfileconflict(L, 1);
    const char *result = alpm_fileconflict_get_target(conflict);
    push_string(L, result);

    return 1;
}

/* pmfileconflicttype_t alpm_fileconflict_get_type(pmfileconflict_t *conflict); */
static int lalpm_fileconflict_get_type(lua_State *L)
{
    pmfileconflict_t *conflict = check_pmfileconflict(L, 1);
    pmfileconflicttype_t type = alpm_fileconflict_get_type(conflict);
    switch(type) {
        case PM_FILECONFLICT_TARGET:
            return push_string(L, "FILECONFLICT_TARGET");
        case PM_FILECONFLICT_FILESYSTEM:
            return push_string(L, "FILECONFLICT_FILESYSTEM");
    }
    return 1;
}

/* const char *alpm_fileconflict_get_file(pmfileconflict_t *conflict); */
static int lalpm_fileconflict_get_file(lua_State *L)
{
    pmfileconflict_t *conflict = check_pmfileconflict(L, 1);
    const char *result = alpm_fileconflict_get_file(conflict);
    push_string(L, result);

    return 1;
}

/* const char *alpm_fileconflict_get_ctarget(pmfileconflict_t *conflict); */
static int lalpm_fileconflict_get_ctarget(lua_State *L)
{
    pmfileconflict_t *conflict = check_pmfileconflict(L, 1);
    const char *result = alpm_fileconflict_get_ctarget(conflict);
    push_string(L, result);

    return 1;
}

pmfileconflict_t **push_pmfileconflict_box(lua_State *L)
{
    pmfileconflict_t **box = lua_newuserdata(L, sizeof(pmfileconflict_t*));
    *box = NULL;

    if (luaL_newmetatable(L, "pmfileconflict_t")) {
        static luaL_Reg const methods[] = {
            { "fileconflict_get_target",    lalpm_fileconflict_get_target },
            { "fileconflict_get_type",      lalpm_fileconflict_get_type },
            { "fileconflict_get_file",      lalpm_fileconflict_get_file },
            { "fileconflict_get_ctarget",   lalpm_fileconflict_get_ctarget },
            { NULL,                     NULL }
        };
        lua_newtable(L);
        luaL_register(L, NULL, methods);
        lua_setfield(L, -2, "__index");
        /*TODO DESTRUCTOR IF NEEDED*/
    }
    lua_setmetatable(L, -2);

    return box;
}

