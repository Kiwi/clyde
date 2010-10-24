#include <assert.h>
#include <alpm.h>
#include <alpm_list.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "types.h"
#include "lualpm.h"
#include "callback.h"

/* We shall keep these here, static for now.  I would prefer them
   to be in a header file, but they are only used in this file. */

struct event_cb_args {
    pmtransevt_t    event;
    void            *data1;
    void            *data2;
};

struct conv_cb_args {
    pmtransconv_t   type;
    void            *data1;
    void            *data2;
    void            *data3;
    int             *response;
};

struct progress_cb_args {
    pmtransprog_t  progress_type;
    const char    *pkg_name;
    int            percent;
    int            pkg_count;
    int            pkg_current;
};

callback_key_t trans_cb_progress_key[1] = {{ "transaction progress" }};

static void
event_cb_gateway_unprotected(pmtransevt_t event, void *data1, void *data2);

static void
conversation_cb_gateway_unprotected( pmtransconv_t type,
                                     void *data1, void *data2, void *data3,
                                     int *response );

static void
progress_cb_gateway_unprotected( pmtransprog_t t,
                                 const char *s, int a, int b, int c );


/* TODO: remove this if it is as unnused as it looks -JD */
pmtrans_t **push_pmtrans_box(lua_State *L)
{
    pmtrans_t **box = lua_newuserdata(L, sizeof(pmtrans_t*));
    *box = NULL;

    if (luaL_newmetatable(L, "pmtrans_t")) {
        static luaL_Reg const methods[] = {
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

/* int alpm_trans_init(pmtransflag_t flags,
                    alpm_trans_cb_event cb_event, alpm_trans_cb_conv conv,
                    alpm_trans_cb_progress cb_progress); */
int lalpm_trans_init(lua_State *L)
{
    pmtransflag_t flags = check_transflags_table(L, 1);
    lua_pushvalue(L, 2);
    lua_setfield(L, LUA_REGISTRYINDEX, "lualpm: events callback table");
    lua_pushvalue(L, 3);
    lua_setfield(L, LUA_REGISTRYINDEX, "lualpm: conversations callback table");
    lua_pushvalue(L, 4);
    cb_register( L, trans_cb_progress_key );
    const int result = alpm_trans_init(flags,
                                       event_cb_gateway_unprotected,
                                       conversation_cb_gateway_unprotected,
                                       progress_cb_gateway_unprotected);
    lua_pushnumber(L, result);
    return 1;
}

/* int alpm_trans_prepare(alpm_list_t **data); */
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

/* TRANSACTION CALLBACKS ****************************************************/

static int
event_cb_gateway_protected(lua_State *L)
{
    struct event_cb_args *args = lua_touserdata(L, 1);
    lua_getfield(L, LUA_REGISTRYINDEX, "lualpm: events callback table");
    switch (args->event) {
        pmpkg_t **box;
        pmpkg_t **box1;
#define f(x) case PM_TRANS_EVT_ ## x: lua_getfield(L, -1, "T_E_" #x)
#define bn(b) if (b == NULL) { \
                    lua_pushnil(L); }
#define g(x, y) lua_call(L, x, y); break
        f(CHECKDEPS_START);
            g(0, 0);
        f(FILECONFLICTS_START);
            g(0, 0);
        f(RESOLVEDEPS_START);
            g(0, 0);
        f(INTERCONFLICTS_START);
            g(0, 0);
        f(ADD_START);
            box = push_pmpkg_box(L);
            *box = args->data1;
            bn(*box);
            g(1, 0);
        f(ADD_DONE);
            box = push_pmpkg_box(L);
            *box = args->data1;
            bn(*box);
            g(1, 0);
        f(REMOVE_START);
            box = push_pmpkg_box(L);
            *box = args->data1;
            bn(*box);
            g(1, 0);
        f(REMOVE_DONE);
            box = push_pmpkg_box(L);
            *box  = args->data1;
            bn(*box);
            g(1, 0);
        f(UPGRADE_START);
            box = push_pmpkg_box(L);
            *box = args->data1;
            bn(*box);
            g(1, 0);
        f(UPGRADE_DONE);
            box = push_pmpkg_box(L);
            *box = args->data1;
            bn(*box);
            box1 = push_pmpkg_box(L);
            *box1 = args->data2;
            bn(*box1);
            g(2, 0);
        f(INTEGRITY_START);
            g(0, 0);
        f(DELTA_INTEGRITY_START);
            g(0, 0);
        f(DELTA_PATCHES_START);
            g(0, 0);
        f(DELTA_PATCH_START);
            push_string(L, (char *)args->data1);
            push_string(L, (char *)args->data2);
            g(2, 0);
        f(DELTA_PATCH_DONE);
            g(0, 0);
        f(DELTA_PATCH_FAILED);
            g(0, 0);
        f(SCRIPTLET_INFO);
            push_string(L, (char *)args->data1);
            g(1, 0);
        f(RETRIEVE_START);
            push_string(L, (char *)args->data1);
            g(1, 0);
#undef f
#undef bn
#undef g
#define f(x) case PM_TRANS_EVT_ ## x:
            f(FILECONFLICTS_DONE)
            f(CHECKDEPS_DONE)
            f(RESOLVEDEPS_DONE)
            f(INTERCONFLICTS_DONE)
            f(INTEGRITY_DONE)
            f(DELTA_INTEGRITY_DONE)
            f(DELTA_PATCHES_DONE)
            break;
#undef f
        default:
            assert(0 && "[BUG] unexpected pmtransevt_t");
    }

    return 0;
}

static void
event_cb_gateway_unprotected(pmtransevt_t event, void *data1, void *data2)
{
    lua_State *L = GlobalState;
    struct event_cb_args args[1];
    int err;
    assert(L && "[BUG] no global Lua state in transaction event callback");
    args->event = event;
    args->data1 = data1;
    args->data2 = data2;
    err = lua_cpcall(L, event_cb_gateway_protected, args);
    if (err) {
        cb_error_handler( "event", err );
    }
}

static int
conversation_cb_gateway_protected(lua_State *L)
{
    struct conv_cb_args *args = lua_touserdata(L, 1);
    lua_getfield(L, LUA_REGISTRYINDEX, "lualpm: conversations callback table");
    switch (args->type) {
        pmpkg_t **box;
        pmpkg_t **box1;
#define f(x) case PM_TRANS_CONV_  ## x: lua_getfield(L, -1, "T_C_" #x)
#define g(x, y) lua_call(L, x, y); \
                if (args->response) { \
                args->response[0] = lua_toboolean(L, -1); \
                } \
                lua_pop(L, 1); \
                break
#define bn(b) if (b == NULL) { \
                    lua_pushnil(L); }
        f(INSTALL_IGNOREPKG);
            box = push_pmpkg_box(L);
            *box = args->data1;
            bn(*box);
            g(1, 1);
        f(REPLACE_PKG);
            box = push_pmpkg_box(L);
            *box = args->data1;
            bn(*box);
            box1 = push_pmpkg_box(L);
            *box1 = args->data2;
            bn(*box1);
            push_string(L, args->data3);
            g(3, 1);
        f(CONFLICT_PKG);
            push_string(L, args->data1);
            push_string(L, args->data2);
            push_string(L, args->data3);
            g(3, 1);
        f(CORRUPTED_PKG);
            push_string(L, args->data1);
            g(1, 1);
        f(LOCAL_NEWER);
            box = push_pmpkg_box(L);
            *box = args->data1;
            bn(*box);
            g(1, 1);
        f(REMOVE_PKGS);
            alpm_list_to_any_table(L, args->data1, PMPKG_T);
            g(1, 1);
#undef f
#undef g
#undef bn
        default:
            assert(0 && "[BUG] unexpected pmtransconv_t");
    }

    return 0;
}

static void
conversation_cb_gateway_unprotected(pmtransconv_t type, void *data1, void *data2, void *data3, int *response)
{
    lua_State *L = GlobalState;
    struct conv_cb_args args[1];
    int err;
    assert(L && "[BUG] no global Lua state in conversation event callback");
    args->type = type;
    args->data1 = data1;
    args->data2 = data2;
    args->data3 = data3;
    args->response = response;
    err = lua_cpcall(L, conversation_cb_gateway_protected, args);
    if (err) {
        cb_error_handler( "conversation", err );
    }
}

static int
push_pmtransprog(lua_State *L, pmtransprog_t t)
{
    switch (t) {
#define f(x) case PM_TRANS_PROGRESS_ ## x: return push_string(L, "T_P_" #x)
        f(ADD_START);
        f(UPGRADE_START);
        f(REMOVE_START);
        f(CONFLICTS_START);
#undef f
    default:
        assert(0 && "[BUG] unexpected pmtransprog_t");
    }
    return 0;
}

/* This is a protected gateway for the progress callback.  It receives
 * the progress callback arguments on the stack as a lightuserdata
 * pointing to a struct progress_cb_args. */
static int
progress_cb_gateway_protected(lua_State *L)
{
    struct progress_cb_args *args = lua_touserdata(L, 1);

    /* We'll look for a callback to call in the registry. */
    cb_lookup(trans_cb_progress_key);
    push_pmtransprog(L, args->progress_type);
    push_string(L, args->pkg_name);
    lua_pushnumber(L, args->percent);
    lua_pushnumber(L, args->pkg_count);
    lua_pushnumber(L, args->pkg_current);
    lua_call(L, 5, 0);

    return 0;
}

/* This is called by libalpm whenever a transaction progress event
 * occurs.  The context of the call is such that no Lua API functions
 * which may throw errors are allowed since that would cause a setjmp
 * past libalpm's call frames.  Instead we wrap the arguments into a
 * struct and transfer control to a protected gateway for further
 * processing. */
static void
progress_cb_gateway_unprotected(pmtransprog_t t, const char *s, int a, int b, int c)
{
    lua_State *L = GlobalState;
    struct progress_cb_args args[1];
    int err;
    assert(L && "[BUG] no global Lua state in progress callback");
    args->progress_type = t;
    args->pkg_name = s;
    args->percent = a;
    args->pkg_count = b;
    args->pkg_current = c;
    err = lua_cpcall(L, progress_cb_gateway_protected, args);
    if (err) {
        cb_error_handler( "progress", err );
    }
}
