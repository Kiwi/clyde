#include <alpm.h>
#include <alpm_list.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "types.h"

/* DEPENDENCY CLASS */

/* char *alpm_dep_compute_string(const pmdepend_t *dep); */
static int lalpm_dep_compute_string(lua_State *L)
{
    const pmdepend_t *dep = check_pmdepend(L, 1);
    char *result = alpm_dep_compute_string(dep);
    push_string(L, result);
    free((void*)result);

    return 1;
}


pmdepend_t **push_pmdepend_box(lua_State *L)
{
    pmdepend_t **box = lua_newuserdata(L, sizeof(pmdepend_t*));
    *box = NULL;

    if (luaL_newmetatable(L, "pmdepend_t")) {
        static luaL_Reg const methods[] = {
            { "dep_compute_string",     lalpm_dep_compute_string },
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

/* MISC DEP FUNCTIONS********************************************************/

/* int alpm_depcmp(pmpkg_t *pkg, pmdepend_t *dep); */
int lalpm_depcmp(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    pmdepend_t *dep = check_pmdepend(L, 2);
    const int result = alpm_depcmp(pkg, dep);
    lua_pushnumber(L, result);

    return 1;
}

/* alpm_list_t *alpm_checkdeps(alpm_list_t *pkglist, int reversedeps,
		alpm_list_t *remove, alpm_list_t *upgrade); */
int lalpm_checkdeps(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    alpm_list_t *pkglist = lstring_table_to_alpm_list(L, 1);
    const int reversedeps = luaL_checknumber(L, 2);
    luaL_checktype(L, 2, LUA_TTABLE);
    alpm_list_t *remove = lstring_table_to_alpm_list(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    alpm_list_t *upgrade = lstring_table_to_alpm_list(L, 3);
    alpm_list_t *result = alpm_checkdeps(pkglist, reversedeps, remove, upgrade);
    alpm_list_to_any_table(L, result, PMPKG_T);

    return 1;
}
/* alpm_list_t *alpm_deptest(pmdb_t *db, alpm_list_t *targets); */
int lalpm_deptest(lua_State *L)
{
    pmdb_t *db = check_pmdb(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    alpm_list_t *targets = lstring_table_to_alpm_list(L, 2);
    alpm_list_t *result = alpm_deptest(db, targets);
    alpm_list_to_any_table(L, result, STRING);

    return 1;
}

