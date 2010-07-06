#include <alpm.h>
#include <alpm_list.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "types.h"

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
