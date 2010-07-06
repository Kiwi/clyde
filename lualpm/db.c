#include <string.h>
#include <alpm.h>
#include <alpm_list.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "types.h"

/* DATABASE CLASS */

/* int alpm_db_unregister(pmdb_t *db); */
static int lalpm_db_unregister(lua_State *L)
{
    pmdb_t *db = check_pmdb(L, 1);
    const int result = alpm_db_unregister(db);
    lua_pushnumber(L, result);

    return 1;
}

/* const char *alpm_db_get_name(const pmdb_t *db); */
static int lalpm_db_get_name(lua_State *L)
{
    const pmdb_t *db = check_pmdb(L, 1);
    const char *result = alpm_db_get_name(db);
    push_string(L, result);

    return 1;
}

/* const char *alpm_db_get_url(const pmdb_t *db); */
static int lalpm_db_get_url(lua_State *L)
{
    const pmdb_t *db = check_pmdb(L, 1);
    const char *result = alpm_db_get_url(db);
    push_string(L, result);

    return 1;
}

/* int alpm_db_setserver(pmdb_t *db, const char *url); */
static int lalpm_db_setserver(lua_State *L)
{
    pmdb_t *db = check_pmdb(L, 1);
    const char *url = luaL_checkstring(L, 2);
    const int result = alpm_db_setserver(db, url);
    lua_pushnumber(L, result);

    return 1;
}

/* int alpm_db_update(int level, pmdb_t *db); */
/* lua prototype is db:db_update(true/false) (true to force) */
static int lalpm_db_update(lua_State *L)
{
    pmdb_t *db = check_pmdb(L, 1);
    const int level = lua_toboolean(L, 2);
    const int result = alpm_db_update(level, db);
    lua_pushnumber(L, result);

    return 1;
}

/* pmpkg_t *alpm_db_get_pkg(pmdb_t *db, const char *name); */
static int lalpm_db_get_pkg(lua_State *L)
{
    pmdb_t *db = check_pmdb(L, 1);
    const char *name = luaL_checkstring(L, 2);
    pmpkg_t **box = push_pmpkg_box(L);
    *box = alpm_db_get_pkg(db, name);
    if (*box == NULL) {
        lua_pushnil(L);
    }

    return 1;
}

/* alpm_list_t *alpm_db_get_pkgcache(pmdb_t *db); */
static int lalpm_db_get_pkgcache(lua_State *L)
{
    pmdb_t *db = check_pmdb(L, 1);
    alpm_list_t *list = alpm_db_get_pkgcache(db);
    alpm_list_to_any_table(L, list, PMPKG_T);

    return 1;
}

/* pmgrp_t *alpm_db_readgrp(pmdb_t *db, const char *name); */
static int lalpm_db_readgrp(lua_State *L)
{
    pmdb_t *db = check_pmdb(L, 1);
    const char *name = luaL_checkstring(L, 2);
    pmgrp_t **box = push_pmgrp_box(L);
    *box = alpm_db_readgrp(db, name);
    if (*box == NULL) {
        lua_pushnil(L);
    }

    return 1;
}

/* alpm_list_t *alpm_db_get_grpcache(pmdb_t *db); */
static int lalpm_db_get_grpcache(lua_State *L)
{
    pmdb_t *db = check_pmdb(L, 1);
    alpm_list_t *list = alpm_db_get_grpcache(db);
    alpm_list_to_any_table(L, list, PMGRP_T);

    return 1;
}

/* alpm_list_t *alpm_db_search(pmdb_t *db, const alpm_list_t* needles); */
static int lalpm_db_search(lua_State *L)
{
    pmdb_t *db = check_pmdb(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    alpm_list_t *needles = lstring_table_to_alpm_list(L, 2);
    alpm_list_t *list = alpm_db_search(db, needles);
    alpm_list_to_any_table(L, list, PMPKG_T);
    FREELIST(needles);
    alpm_list_free(list);

    return 1;
}

/* int alpm_db_set_pkgreason
   (pmdb_t *db, const char *name, pmpkgreason_t reason); */
static int lalpm_db_set_pkgreason ( lua_State *L )
{
    const char *pkg_name, *pkg_reason;
    pmpkgreason_t alpm_reason;
    pmdb_t *db;
    int result;

    db         = check_pmdb( L, 1 );
    pkg_name   = luaL_checkstring( L, 2 );
    pkg_reason = luaL_checkstring( L, 3 );

    alpm_reason = PKGREASON_COUNT;
    for ( int i=0; i < PKGREASON_COUNT ; ++i ) {
        if ( strcmp( pkg_reason, PKGREASON_TOSTR[ i ] ) == 0 ) {
            alpm_reason = i;
        }
    }

    result = ( alpm_reason < PKGREASON_COUNT
               ? alpm_db_set_pkgreason( db, pkg_name, alpm_reason )
               : -1 );
    lua_pushnumber( L, result );
    return 1;
}

/* methods have pmdb_t as first arg */
pmdb_t **push_pmdb_box(lua_State *L)
{
    pmdb_t **box = lua_newuserdata(L, sizeof(pmdb_t*));
    *box = NULL;

    if (luaL_newmetatable(L, "pmdb_t")) {
        static luaL_Reg const methods[] = {
            { "db_unregister",          lalpm_db_unregister },
            { "db_get_name",            lalpm_db_get_name }, /* works */
            { "db_get_url",             lalpm_db_get_url },
            { "db_setserver",           lalpm_db_setserver },
            { "db_update",              lalpm_db_update }, /* returning -1 in tests */
            { "db_get_pkg",             lalpm_db_get_pkg }, /* works */
            { "db_get_pkgcache",        lalpm_db_get_pkgcache },
            { "db_readgrp",             lalpm_db_readgrp },
            { "db_get_grpcache",        lalpm_db_get_grpcache },
            { "db_search",              lalpm_db_search },
            { "db_set_pkgreason",       lalpm_db_set_pkgreason },
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
