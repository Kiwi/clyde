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
    FREELIST(pkglist);
    FREELIST(remove);
    FREELIST(upgrade);
    return 1;
}

int lalpm_find_satisfier ( lua_State *L )
{
    const char  * depstring;
    alpm_list_t * pkglist;
    pmpkg_t     * found;

    pkglist   = lpackage_table_to_alpm_list( L, 1 );
    depstring = luaL_checkstring( L, 2 );
    found     = alpm_find_satisfier( pkglist, depstring );
    alpm_list_free( pkglist );

    if ( found == NULL ) {
        lua_pushnil( L );
    }
    else {
        push_pmpkg( L, found );
    }

    return 1;
}

int lalpm_find_dbs_satisfier ( lua_State * L )
{
    const char  * depstring;
    alpm_list_t * dblist;
    pmpkg_t     * found;

    dblist    = ldatabase_table_to_alpm_list( L, 1 );
    depstring = luaL_checkstring( L, 2 );
    found     = alpm_find_dbs_satisfier( dblist, depstring );
    alpm_list_free( dblist );

    if ( found == NULL ) {
        lua_pushnil( L );
    }
    else {
        push_pmpkg( L, found );
    }

    return 1;
}
