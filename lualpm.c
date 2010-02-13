/*
 * gcc -W -Wall -lalpm `pkg-config --cflags lua` -fPIC -shared -o lualpm.so lualpm.c
 * gcc -W -Wall -O2 -lalpm `pkg-config --cflags lua` -fPIC -shared -o lualpm.so lualpm.c -pedantic -std=c99  -D_GNU_SOURCE
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h> /* off_t */
#include <time.h> /* time_t */
#include <stdarg.h> /* va_list */

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <alpm.h>
#include <alpm_list.h>

/* A global is required for alpm C -> Lua gateways to get their Lua
 * goodness from.  Unfortunately alpm callbacks have no userdata or
 * other pointer which the client could use to send their own
 * parameters in. */
static lua_State *GlobalState;

static int
push_string(lua_State *L, char const *s)
{
    if (!s) lua_pushnil(L);
    else lua_pushstring(L, s);
    return 1;
}

typedef struct changelog {
    void *fp;
    pmpkg_t *pkg;
    char *buffer;
} changelog;

static pmdb_t **push_pmdb_box(lua_State *L);
static pmpkg_t **push_pmpkg_box(lua_State *L);
static pmdelta_t **push_pmdelta_box(lua_State *L);
static pmgrp_t **push_pmgrp_box(lua_State *L);
static pmtrans_t **push_pmtrans_box(lua_State *L);
static pmdepend_t **push_pmdepend_box(lua_State *L);
static pmdepmissing_t **push_pmdepmissing_box(lua_State *L);
static pmconflict_t **push_pmconflict_box(lua_State *L);
static pmfileconflict_t **push_pmfileconflict_box(lua_State *L);
static changelog * push_changelog_box(lua_State *L);
//static alpm_list_t **push_alpmlist_box(lua_State *L);
/*
static off_t **push_off_box(lua_State *L);
static time_t **push_time_box(lua_State *L);
*/
//16:24 < hoelzro> here's how you'd use it:
//16:25 < hoelzro> alpm_list_t t; memset(&t, 0, sizeof(alpm_list_t)); lua_tabletolist(&t, L, index);
/* do stuff with t ; alpm_list_free(&t); */
/*
*/

alpm_list_t *lstring_table_to_alpm_list(lua_State *L, int narg)
{
    alpm_list_t *newlist = NULL;
    size_t i, len = lua_objlen(L, narg);

    for (i = 1; i <= len; i++) {
        lua_rawgeti(L, narg, i);
        const char *data = lua_tostring(L, -1);
        newlist = alpm_list_add(newlist, strdup(data));
        lua_pop(L, 1);
    }

    return(newlist);
}
/*
static int alpm_list_to_lpkg_table(lua_State *L, alpm_list_t *list)
{
    lua_newtable(L);
    size_t j;
    alpm_list_t *i;
    for (i = list, j = 1; i; i = alpm_list_next(i), j++) {
        pmpkg_t **box = push_pmpkg_box(L);
        *box = alpm_list_getdata(i);
        lua_rawseti(L, -2, j);
    }

    return 1;
}
*/
/*
static int alpm_list_to_lstring_table(lua_State *L, alpm_list_t *list)
{
    lua_newtable(L);
    size_t j;
    alpm_list_t *i;
    for (i = list, j = 1; i; i = alpm_list_next(i), j++) {
        const char *str = alpm_list_getdata(i);
        lua_pushstring(L, str);
        lua_rawseti(L, -2, j);
    }

    return 1;
}
*/

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
static int alpm_list_to_any_table(lua_State *L, alpm_list_t *list, enum types type)
{
    lua_newtable(L);
    size_t j;
    alpm_list_t *i;
    switch(type) {
        case STRING:
            for (i = list, j = 1; i; i = alpm_list_next(i), j++) {
                const char *str = alpm_list_getdata(i);
                lua_pushstring(L, str);
                lua_rawseti(L, -2, j);
            }
            break;

        case PMDB_T:
            for (i = list, j = 1; i; i = alpm_list_next(i), j++) {
                pmdb_t **box = push_pmdb_box(L);
                *box = alpm_list_getdata(i);
/*                if (*box == NULL) {
                    lua_pop(L, 1);
                    break;
                }
*/
                lua_rawseti(L, -2, j);
            }
            break;

        case PMPKG_T:
            for (i = list, j = 1; i; i = alpm_list_next(i), j++) {
                pmpkg_t **box = push_pmpkg_box(L);
                *box = alpm_list_getdata(i);
                lua_rawseti(L, -2, j);
            }
            break;

        case PMDELTA_T:
            for (i = list, j = 1; i; i = alpm_list_next(i), j++) {
                pmdelta_t **box = push_pmdelta_box(L);
                *box = alpm_list_getdata(i);
                lua_rawseti(L, -2, j);
            }
            break;

        case PMGRP_T:
            for (i = list, j = 1; i; i = alpm_list_next(i), j++) {
                pmgrp_t **box = push_pmgrp_box(L);
                *box = alpm_list_getdata(i);
                lua_rawseti(L, -2, j);
            }
            break;

        case PMTRANS_T:
            for (i = list, j = 1; i; i = alpm_list_next(i), j++) {
                pmtrans_t **box = push_pmtrans_box(L);
                *box = alpm_list_getdata(i);
                lua_rawseti(L, -2, j);
            }
            break;

        case PMDEPEND_T:
            for (i = list, j = 1; i; i = alpm_list_next(i), j++) {
                pmdepend_t **box = push_pmdepend_box(L);
                *box = alpm_list_getdata(i);
                lua_rawseti(L, -2, j);
            }
            break;

        case PMDEPMISSING_T:
            for (i = list, j = 1; i; i = alpm_list_next(i), j++) {
                pmdepmissing_t **box = push_pmdepmissing_box(L);
                *box = alpm_list_getdata(i);
                lua_rawseti(L, -2, j);
            }
            break;

        case PMCONFLICT_T:
            for (i = list, j = 1; i; i = alpm_list_next(i), j++) {
                pmconflict_t **box = push_pmconflict_box(L);
                *box = alpm_list_getdata(i);
                lua_rawseti(L, -2, j);
            }
            break;

        case PMFILECONFLICT_T:
            for (i = list, j = 1; i; i = alpm_list_next(i), j++) {
                pmfileconflict_t **box = push_pmfileconflict_box(L);
                *box = alpm_list_getdata(i);
                lua_rawseti(L, -2, j);
            }
            break;

        default:
            lua_pushnil(L);
            break;
    }

    return 1;
}



pmtranstype_t lstring_to_transtype(lua_State *L, int narg)
{
    //pmtransflag_t flag;
    //int i; //, isoption = 0;

    const char *const slist[] = {
        "T_T_UPGRADE",
        "T_T_REMOVE",
        "T_T_REMOVEUPGRADE",
        "T_T_SYNC",
        NULL
    };

    const int ilist[] = {
        1, 2, 3, 4
    };
    const int i = luaL_checkoption(L, narg, NULL, slist);
    pmtransflag_t flag = ilist[i];
/*
    const int listlen = sizeof(slist)/sizeof(char*);

    const char *data = lua_tostring(L, narg);

    for (i = 0; i < listlen; i++) {
        if (strcmp(slist[i], data) == 0) {
            flag = ilist[i];
            isoption = 1;
            break;
        }
    }
    if (isoption == 0) {
        luaL_argerror(L, narg, "Invalid option given");
    }
*/
    return flag;
}

pmtransflag_t lstring_table_to_transflag(lua_State *L, int narg)
{
    pmtransflag_t flags = 0;
    int i, j, isoption, tbllen = lua_objlen(L, narg);

    const char *slist[] = {
        "T_F_NODEPS",
        "T_F_FORCE",
        "T_F_NOSAVE",
        "T_F_CASCADE",
        "T_F_RECURSE",
        "T_F_DBONLY",
        "T_F_ALLDEPS",
        "T_F_DOWNLOADONLY",
        "T_F_NOSCRIPTLET",
        "T_F_NOCONFLICTS",
        "T_F_NEEDED",
        "T_F_ALLEXPLICIT",
        "T_F_UNNEEDED",
        "T_F_RECURSEALL",
        "T_F_NOLOCK"
    };

    const int ilist[] = {
        0x01, 0x02, 0x04, 0x10, 0x20, 0x40,
        0x100, 0x200, 0x400, 0x800, 0x2000,
        0x4000, 0x8000, 0x10000, 0x20000
    };

    const int listlen = sizeof(slist)/sizeof(char*);

    for (j=1; j <= tbllen; j++) {
        lua_rawgeti(L, narg, j);
        const char *data = lua_tostring(L, -1);
        isoption = 0;
        for (i = 0; i < listlen; i++) {
            if (strcmp(slist[i], data) == 0) {
                flags |= ilist[i];
                isoption = 1;
                lua_pop(L, 1);
                break;
            }
        }
        lua_pop(L, 1);
        if (isoption == 0) {
            luaL_argerror(L, narg, "Invalid options given");
        }
    }

    return flags;
}


static pmdb_t *check_pmdb(lua_State *L, int narg)
{
    pmdb_t **box = luaL_checkudata(L, narg, "pmdb_t");
    if (*box == NULL) {
        luaL_argerror(L, narg, "Empty pmdb_t box");
    }

    return *box;
}

static pmpkg_t *check_pmpkg(lua_State *L, int narg)
{
    pmpkg_t **box = luaL_checkudata(L, narg, "pmpkg_t");
    if (*box == NULL) {
        luaL_argerror(L, narg, "Empty pmpkg_t box");
    }

    return *box;
}
static pmdelta_t *check_pmdelta(lua_State *L, int narg)
{
    pmdelta_t **box = luaL_checkudata(L, narg, "pmdelta_t");
    if (*box == NULL) {
        luaL_argerror(L, narg, "Empty pmdelta_t box");
    }

    return *box;
}
static pmgrp_t *check_pmgrp(lua_State *L, int narg)
{
    pmgrp_t **box = luaL_checkudata(L, narg, "pmgrp_t");
    if (*box == NULL) {
        luaL_argerror(L, narg, "Empty pmgrp_t box");
    }

    return *box;
}

static pmtrans_t *check_pmtrans(lua_State *L, int narg)
{
    pmtrans_t **box = luaL_checkudata(L, narg, "pmtrans_t");
    if (*box == NULL) {
        luaL_argerror(L, narg, "Empty pmtrans_t box");
    }

    return *box;
}

static pmdepend_t *check_pmdepend(lua_State *L, int narg)
{
    pmdepend_t **box = luaL_checkudata(L, narg, "pmdepend_t");
    if (*box == NULL) {
        luaL_argerror(L, narg, "Empty pmdepend_t box");
    }

    return *box;
}

static pmdepmissing_t *check_pmdepmissing(lua_State *L, int narg)
{
    pmdepmissing_t **box = luaL_checkudata(L, narg, "pmdepmissing_t");
    if (*box == NULL) {
        luaL_argerror(L, narg, "Empty pmdepmissing_t box");
    }

    return *box;
}

static pmconflict_t *check_pmconflict(lua_State *L, int narg)
{
    pmconflict_t **box = luaL_checkudata(L, narg, "pmconflict_t");
    if (*box == NULL) {
        luaL_argerror(L, narg, "Empty pmconflict_t box");
    }

    return *box;
}

static pmfileconflict_t *check_pmfileconflict(lua_State *L, int narg)
{
    pmfileconflict_t **box = luaL_checkudata(L, narg, "pmfileconflict_t");
    if (*box == NULL) {
        luaL_argerror(L, narg, "Empty pmfileconflict_t box");
    }

    return *box;
}

static changelog *check_changelog(lua_State *L, int narg)
{
    changelog **box = luaL_checkudata(L, narg, "changelog");
    if (*box == NULL) {
        luaL_argerror(L, narg, "Empty changelog box");
    }

    return *box;
}
/*
static off_t *check_off(lua_State *L, int narg)
{
    off_t **box = luaL_checkudata(L, narg, "off_t");
    if (*box == NULL) {
        luaL_argerror(L, narg, "Empty off_t box");
    }

    return *box;
}

static time_t *check_time(lua_State *L, int narg)
{
    time_t **box = luaL_checkudata(L, narg, "time_t");
    if (*box == NULL) {
        luaL_argerror(L, narg, "Empty time_t box");
    }

    return *box;
}
*/

/*
static alpm_list_t *check_alpmlist(lua_State *L, int narg)
{
    alpm_list_t **box = luaL_checkudata(L, narg, "alpm_list_t");
    if (*box == NULL) {
        luaL_argerror(L, narg, "Empty alpm_list_t box");
        }

    return *box;
}
*/

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
    lua_pushstring(L, result);

    return 1;
}

/* const char *alpm_db_get_url(const pmdb_t *db); */
static int lalpm_db_get_url(lua_State *L)
{
    const pmdb_t *db = check_pmdb(L, 1);
    const char *result = alpm_db_get_url(db);
    lua_pushstring(L, result);

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
    /* const int level = luaL_checkint(L, 2); */
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
/*        luaL_error(L, "got a NULL pmpkg_t from alpm_db_get_pkg()");*/
        lua_pushnil(L);
    }

    return 1;
}

/* alpm_list_t *alpm_db_get_pkgcache(pmdb_t *db); */
static int lalpm_db_get_pkgcache(lua_State *L)
{
    pmdb_t *db = check_pmdb(L, 1);
    alpm_list_t *list = alpm_db_get_pkgcache(db);
//    alpm_list_to_lpkg_table(L, list);
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
//    alpm_list_to_lstring_table(L, list);
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
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, PMPKG_T);
    FREELIST(needles);
    alpm_list_free(list);

    return 1;
}

/*methods have pmdb_t as first arg */
static pmdb_t **push_pmdb_box(lua_State *L)
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
/* int alpm_pkg_load(const char *filename, unsigned short full, pmpkg_t **pkg); */
/* lua prototype is pkg, ret = alpm.pkg_load(filename, true/false) */
static int lalpm_pkg_load(lua_State *L)
{
//    pmpkg_t *pkg = check_pmpkg(L, 3);
    pmpkg_t *pkg = NULL;
    //luaL_checkany(L, 3);
    //pkg = (pmpkg_t *)pkg;
    const char *filename = luaL_checkstring(L, 1);
    const int full = lua_toboolean(L, 2);
    const int result = alpm_pkg_load(filename, full, &pkg);
    pmpkg_t **box = push_pmpkg_box(L);
    *box = pkg;
    if (*box == NULL) {
        lua_pushnil(L);
    }

    lua_pushnumber(L, result);

    return 2;
}

/* int alpm_pkg_free(pmpkg_t *pkg); */
static int lalpm_pkg_free(lua_State *L)
{
     pmpkg_t *pkg = check_pmpkg(L, 1);
     const int result = alpm_pkg_free(pkg);
     lua_pushnumber(L, result);

     return 1;
}

/* int alpm_pkg_checkmd5sum(pmpkg_t *pkg); */
static int lalpm_pkg_checkmd5sum(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const int result = alpm_pkg_checkmd5sum(pkg);
    lua_pushnumber(L, result);

    return 1;
}
/* alpm_list_t *alpm_pkg_compute_requiredby(pmpkg_t *pkg); */
static int lalpm_pkg_compute_requiredby(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_compute_requiredby(pkg);
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);
    FREELIST(list);

    return 1;
}
/* const char *alpm_pkg_get_filename(pmpkg_t *pkg); */
static int lalpm_pkg_get_filename(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_filename(pkg);
    lua_pushstring(L, result);

    return 1;
}
/* const char *alpm_pkg_get_name(pmpkg_t *pkg); */
static int lalpm_pkg_get_name(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_name(pkg);
    lua_pushstring(L, result);

    return 1;
}

/* const char *alpm_pkg_get_version(pmpkg_t *pkg); */
static int lalpm_pkg_get_version(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_version(pkg);
    lua_pushstring(L, result);

    return 1;
}

/* const char *alpm_pkg_get_desc(pmpkg_t *pkg); */
static int lalpm_pkg_get_desc(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_desc(pkg);
    lua_pushstring(L, result);

    return 1;
}

/* const char *alpm_pkg_get_url(pmpkg_t *pkg); */
static int lalpm_pkg_get_url(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_url(pkg);
    lua_pushstring(L, result);

    return 1;
}

/* time_t alpm_pkg_get_builddate(pmpkg_t *pkg); */
static int lalpm_pkg_get_builddate(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const time_t result = alpm_pkg_get_builddate(pkg);
    lua_pushnumber(L, result);

    return 1;
}

/* time_t alpm_pkg_get_installdate(pmpkg_t *pkg); */
static int lalpm_pkg_get_installdate(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const time_t result = alpm_pkg_get_installdate(pkg);
    lua_pushnumber(L, result);

    return 1;
}

/* const char *alpm_pkg_get_packager(pmpkg_t *pkg); */
static int lalpm_pkg_get_packager(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_packager(pkg);
    lua_pushstring(L, result);

    return 1;
}
/* const char *alpm_pkg_get_md5sum(pmpkg_t *pkg); */
static int lalpm_pkg_get_md5sum(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_md5sum(pkg);
    lua_pushstring(L, result);

    return 1;
}


/* const char *alpm_pkg_get_arch(pmpkg_t *pkg); */
static int lalpm_pkg_get_arch(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_arch(pkg);
    lua_pushstring(L, result);

    return 1;
}

/* off_t alpm_pkg_get_size(pmpkg_t *pkg); */
static int lalpm_pkg_get_size(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const off_t result = alpm_pkg_get_size(pkg);
    lua_pushnumber(L, result);

    return 1;
}

/* off_t alpm_pkg_get_isize(pmpkg_t *pkg); */
static int lalpm_pkg_get_isize(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const off_t result = alpm_pkg_get_isize(pkg);
    lua_pushnumber(L, result);

    return 1;
}



/* pmpkgreason_t alpm_pkg_get_reason(pmpkg_t *pkg); */
static int lalpm_pkg_get_reason(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    pmpkgreason_t reason = alpm_pkg_get_reason(pkg);
    const char *list[] = {"P_R_EXPLICIT", "P_R_DEPEND"};
    lua_pushstring(L, list[reason]);

    return 1;
}



/* alpm_list_t *alpm_pkg_get_licenses(pmpkg_t *pkg); */
static int lalpm_pkg_get_licenses(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_licenses(pkg);
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_groups(pmpkg_t *pkg); */
static int lalpm_pkg_get_groups(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_groups(pkg);
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_depends(pmpkg_t *pkg); */
static int lalpm_pkg_get_depends(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_depends(pkg);
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, PMDEPEND_T);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_optdepends(pmpkg_t *pkg); */
static int lalpm_pkg_get_optdepends(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_optdepends(pkg);
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_conflicts(pmpkg_t *pkg); */
static int lalpm_pkg_get_conflicts(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_conflicts(pkg);
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_provides(pmpkg_t *pkg); */
static int lalpm_pkg_get_provides(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_provides(pkg);
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_deltas(pmpkg_t *pkg); */
static int lalpm_pkg_get_deltas(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_deltas(pkg);
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_replaces(pmpkg_t *pkg); */
static int lalpm_pkg_get_replaces(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_replaces(pkg);
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_files(pmpkg_t *pkg); */
static int lalpm_pkg_get_files(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_files(pkg);
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_backup(pmpkg_t *pkg); */
static int lalpm_pkg_get_backup(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_backup(pkg);
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_removes(pmpkg_t *pkg); */
static int lalpm_pkg_get_removes(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_removes(pkg);
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* pmdb_t *alpm_pkg_get_db(pmpkg_t *pkg); */
static int lalpm_pkg_get_db(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    pmdb_t **box = push_pmdb_box(L);
    *box = alpm_pkg_get_db(pkg);
    if (*box == NULL) {
        lua_pushnil(L);
    }

    return 1;
}

/* void *alpm_pkg_changelog_open(pmpkg_t *pkg); */
static int lalpm_pkg_changelog_open(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    changelog *box = push_changelog_box(L);
    box->pkg = pkg;
    box->fp = alpm_pkg_changelog_open(pkg);
    if (box->fp == NULL) {
        lua_pushnil(L);
    }

    return 1;
}

/* size_t alpm_pkg_changelog_read(void *ptr, size_t size,
		const pmpkg_t *pkg, const void *fp); */
/* lua prototype is: buff, num = pkg:pkg_changelog_read(size_t size, void *fp) *fp is a changelog */

static int lalpm_pkg_changelog_read(lua_State *L)
{
    const pmpkg_t *pkg = check_pmpkg(L, 1);
    const size_t size = luaL_checknumber(L, 2);
    changelog *box = (changelog*)luaL_checkudata(L, 3, "alpm_changelog");
    const void *fp = box->fp;
    if (fp == NULL) {
        lua_pushnil(L);
        lua_pushnil(L);

        return 2;
    }
    char buff[size];
//    box->buffer = buff;
//    const char *ptr = box->buffer;
    const size_t result = alpm_pkg_changelog_read(buff, size, pkg, fp);
    if (result < size) {
        *(buff + result) = '\0';
    }

    lua_pushlstring(L, buff, result);
    lua_pushnumber(L, result);

    return 2;
}

/* int alpm_pkg_changelog_close(const pmpkg_t *pkg, void *fp); */
static int lalpm_pkg_changelog_close(lua_State *L)
{
    const pmpkg_t *pkg = check_pmpkg(L, 1);
    changelog *box = (changelog*)luaL_checkudata(L, 2, "alpm_changelog");
    if (box->fp == NULL) {
        return 0;
    }
    void *fp = box->fp;
    const int result = alpm_pkg_changelog_close(pkg, fp);
    box->fp = NULL;
    lua_pushnumber(L, result);

    return 1;
}

/*
static Image checkImage (lua_State *L, int index)
{
  Image *pi, im;
  luaL_checktype(L, index, LUA_TUSERDATA);
  pi = (Image*)luaL_checkudata(L, index, IMAGE);
  if (pi == NULL) luaL_typerror(L, index, IMAGE);
  im = *pi;
  if (!im)
    luaL_error(L, "null Image");
  return im;
}
*/

/* unsigned short alpm_pkg_has_scriptlet(pmpkg_t *pkg); */
static int lalpm_pkg_has_scriptlet(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    unsigned short result = alpm_pkg_has_scriptlet(pkg);
    lua_pushnumber(L, result);

    return 1;
}

/* unsigned short alpm_pkg_has_force(pmpkg_t *pkg); */
static int lalpm_pkg_has_force(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    unsigned short result = alpm_pkg_has_force(pkg);
    lua_pushnumber(L, result);

    return 1;
}
/* off_t alpm_pkg_download_size(pmpkg_t *newpkg); */
static int lalpm_pkg_download_size(lua_State *L)
{
    pmpkg_t *newpkg = check_pmpkg(L, 1);
    const off_t result = alpm_pkg_download_size(newpkg);
    lua_pushnumber(L, result);

    return 1;
}



    static void stackDump (lua_State *L) {
      int i;
      int top = lua_gettop(L);
      for (i = 1; i <= top; i++) {  /* repeat for each level */
        int t = lua_type(L, i);
        switch (t) {
    
          case LUA_TSTRING:  /* strings */
            printf("`%s'", lua_tostring(L, i));
            break;
    
          case LUA_TBOOLEAN:  /* booleans */
            printf(lua_toboolean(L, i) ? "true" : "false");
            break;
    
          case LUA_TNUMBER:  /* numbers */
            printf("%g", lua_tonumber(L, i));
            break;
    
          default:  /* other values */
            printf("%s", lua_typename(L, t));
            break;
    
        }
        printf("  ");  /* put a separator */
      }
      printf("\n");  /* end the listing */
    }




alpm_list_t *ldatabase_table_to_alpm_list(lua_State *L, int narg)
{
    alpm_list_t *newlist = NULL;
    size_t i, len = lua_objlen(L, narg);
    for (i = 1; i <= len; i++) {
        lua_rawgeti(L, narg, i);
        const int index = -1;
        //const int typei = lua_type(L, index);
        //const char *type = lua_typename(L, typei);
        //printf("type: %s\n", type);
        pmdb_t *data = *(pmdb_t**)lua_touserdata(L, index);
        //const char *name = alpm_db_get_name(data);
        //printf("name: %s\n", name);
        newlist = alpm_list_add(newlist, (void *)data);
        lua_pop(L, 1);
    }
    return(newlist);
}

/* pmpkg_t *alpm_sync_newversion(pmpkg_t *pkg, alpm_list_t *dbs_sync); */
static int lalpm_sync_newversion(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    alpm_list_t *dbs_sync = ldatabase_table_to_alpm_list(L, 2);
    pmpkg_t **box = push_pmpkg_box(L);
    *box = alpm_sync_newversion(pkg, dbs_sync);
   if (*box == NULL) {
       //printf("IT's NULL");
        lua_pushnil(L);
    }
    alpm_list_free(dbs_sync);
    return 1;
}

/*char *alpm_dep_compute_string(const pmdepend_t *dep); */
static int lalpm_dep_compute_string(lua_State *L)
{
    const pmdepend_t *dep = check_pmdepend(L, 1);
    char *result = alpm_dep_compute_string(dep);
    lua_pushstring(L, result);
    free((void*)result);

    return 1;
}
static pmpkg_t **push_pmpkg_box(lua_State *L)
{
    pmpkg_t **box = lua_newuserdata(L, sizeof(pmpkg_t*));
    *box = NULL;

    if (luaL_newmetatable(L, "pmpkg_t")) {
        static luaL_Reg const methods[] = {
           // { "pkg_load",               lalpm_pkg_load },
            { "pkg_free",               lalpm_pkg_free },
            { "pkg_checkmd5sum",        lalpm_pkg_checkmd5sum }, /* returning -1 */
            { "pkg_compute_requiredby", lalpm_pkg_compute_requiredby },
            { "pkg_get_filename",       lalpm_pkg_get_filename }, /* returning nil in tests */
            { "pkg_get_name",           lalpm_pkg_get_name }, /* works */
            { "pkg_get_version",        lalpm_pkg_get_version }, /* works */
            { "pkg_get_desc",           lalpm_pkg_get_desc }, /* works */
            { "pkg_get_url",            lalpm_pkg_get_url }, /* works */
            { "pkg_get_builddate",      lalpm_pkg_get_builddate }, /* works */
            { "pkg_get_installdate",    lalpm_pkg_get_installdate }, /* works */
            { "pkg_get_packager",       lalpm_pkg_get_packager }, /* works */
            { "pkg_get_md5sum",         lalpm_pkg_get_md5sum }, /* returning nil in tests*/
            { "pkg_get_arch",           lalpm_pkg_get_arch }, /* works */
            { "pkg_get_size",           lalpm_pkg_get_size }, /* works */
/* pmpkgreason_t alpm_pkg_get_reason(pmpkg_t *pkg); */
            { "pkg_get_reason",         lalpm_pkg_get_reason },
            { "pkg_get_isize",          lalpm_pkg_get_isize }, /* works */
            { "pkg_get_licenses",       lalpm_pkg_get_licenses },
            { "pkg_get_groups",         lalpm_pkg_get_groups },
            { "pkg_get_depends",        lalpm_pkg_get_depends },
            { "pkg_get_optdepends",     lalpm_pkg_get_optdepends },
            { "pkg_get_conflicts",      lalpm_pkg_get_conflicts },
            { "pkg_get_provides",       lalpm_pkg_get_provides },
            { "pkg_get_deltas",         lalpm_pkg_get_deltas },
            { "pkg_get_replaces",       lalpm_pkg_get_replaces },
            { "pkg_get_files",          lalpm_pkg_get_files },
            { "pkg_get_backup",         lalpm_pkg_get_backup },
            { "pkg_get_removes",        lalpm_pkg_get_removes },
            { "pkg_get_db",             lalpm_pkg_get_db },
            { "pkg_changelog_open",     lalpm_pkg_changelog_open },
            { "pkg_changelog_read",     lalpm_pkg_changelog_read },
            { "pkg_changelog_close",    lalpm_pkg_changelog_close },
            { "pkg_has_scriptlet",      lalpm_pkg_has_scriptlet },
            { "pkg_has_force",          lalpm_pkg_has_force },
            { "pkg_download_size",      lalpm_pkg_download_size },
            //{ "dep_compute_string",     lalpm_dep_compute_string },
//            { "sync_newversion",        lalpm_sync_newversion },
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

/* const char *alpm_delta_get_from(pmdelta_t *delta); */
static int lalpm_delta_get_from(lua_State *L)
{
    pmdelta_t *delta = check_pmdelta(L, 1);
    const char *result = alpm_delta_get_from(delta);
    lua_pushstring(L, result);

    return 1;
}

/* const char *alpm_delta_get_to(pmdelta_t *delta); */
static int lalpm_delta_get_to(lua_State *L)
{
    pmdelta_t *delta = check_pmdelta(L, 1);
    const char *result = alpm_delta_get_to(delta);
    lua_pushstring(L, result);

    return 1;
}

/* const char *alpm_delta_get_filename(pmdelta_t *delta); */
static int lalpm_delta_get_filename(lua_State *L)
{
    pmdelta_t *delta = check_pmdelta(L, 1);
    const char *result = alpm_delta_get_filename(delta);
    lua_pushstring(L, result);

    return 1;
}

/* const char *alpm_delta_get_md5sum(pmdelta_t *delta); */
static int lalpm_delta_get_md5sum(lua_State *L)
{
    pmdelta_t *delta = check_pmdelta(L, 1);
    const char *result = alpm_delta_get_md5sum(delta);
    lua_pushstring(L, result);

    return 1;
}

/* off_t alpm_delta_get_size(pmdelta_t *delta); */
static int lalpm_delta_get_size(lua_State *L)
{
    pmdelta_t *delta = check_pmdelta(L, 1);
    const off_t result = alpm_delta_get_size(delta);
    lua_pushnumber(L, result);

    return 1;
}



static pmdelta_t **push_pmdelta_box(lua_State *L)
{
    pmdelta_t **box = lua_newuserdata(L, sizeof(pmdelta_t*));
    *box = NULL;

    if (luaL_newmetatable(L, "pmdelta_t")) {
        static luaL_Reg const methods[] = {
            { "delta_get_from",         lalpm_delta_get_from },
            { "delta_get_to",           lalpm_delta_get_to },
            { "delta_get_filename",     lalpm_delta_get_filename },
            { "delta_get_md5sum",       lalpm_delta_get_md5sum },
            { "delta_get_size",         lalpm_delta_get_size },
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

/* const char *alpm_grp_get_name(const pmgrp_t *grp); */
static int lalpm_grp_get_name(lua_State *L)
{
    const pmgrp_t *grp = check_pmgrp(L, 1);
    const char *result = alpm_grp_get_name(grp);
    lua_pushstring(L, result);

    return 1;
}

/* alpm_list_t *alpm_grp_get_pkgs(const pmgrp_t *grp); */
static int lalpm_grp_get_pkgs(lua_State *L)
{
    const pmgrp_t *grp = check_pmgrp(L, 1);
    alpm_list_t *list = alpm_grp_get_pkgs(grp);
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, PMPKG_T);

    return 1;
}

static pmgrp_t **push_pmgrp_box(lua_State *L)
{
    pmgrp_t **box = lua_newuserdata(L, sizeof(pmgrp_t*));
    *box = NULL;

    if (luaL_newmetatable(L, "pmgrp_t")) {
        static luaL_Reg const methods[] = {
            { "grp_get_name",           lalpm_grp_get_name },
            { "grp_get_pkgs",           lalpm_grp_get_pkgs },
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

static pmtrans_t **push_pmtrans_box(lua_State *L)
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



static pmdepend_t **push_pmdepend_box(lua_State *L)
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
/* const char *alpm_miss_get_target(const pmdepmissing_t *miss); */
static int lalpm_miss_get_target(lua_State *L)
{
    const pmdepmissing_t *miss = check_pmdepmissing(L, 1);
    const char *result = alpm_miss_get_target(miss);
    lua_pushstring(L, result);

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
    lua_pushstring(L, result);

    return 1;
}

static pmdepmissing_t **push_pmdepmissing_box(lua_State *L)
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
    lua_pushstring(L, result);

    return 1;
}

/* const char *alpm_conflict_get_package2(pmconflict_t *conflict); */
static int lalpm_conflict_get_package2(lua_State *L)
{
    pmconflict_t *conflict = check_pmconflict(L, 1);
    const char *result = alpm_conflict_get_package2(conflict);
    lua_pushstring(L, result);

    return 1;
}

static pmconflict_t **push_pmconflict_box(lua_State *L)
{
    pmconflict_t **box = lua_newuserdata(L, sizeof(pmconflict_t*));
    *box = NULL;

    if (luaL_newmetatable(L, "pmconflict_t")) {
        static luaL_Reg const methods[] = {
            { "conflict_get_package1",  lalpm_conflict_get_package1 },
            { "conflict_get_package2",  lalpm_conflict_get_package2 },
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

static pmfileconflict_t **push_pmfileconflict_box(lua_State *L)
{
    pmfileconflict_t **box = lua_newuserdata(L, sizeof(pmfileconflict_t*));
    *box = NULL;

    if (luaL_newmetatable(L, "pmfileconflict_t")) {
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




static changelog *push_changelog_box(lua_State *L)
{
    changelog *box = lua_newuserdata(L, sizeof(changelog));
    //box = NULL;

    if (luaL_newmetatable(L, "alpm_changelog")) {
        static luaL_Reg const methods[] = {
            { NULL,                     NULL }
        };
        lua_newtable(L);
        luaL_register(L, NULL, methods);
        lua_setfield(L, -2, "__index");
        /*TODO DESTRUCTOR IF NEEDED*/
        //luaL_getmetatable(L, "alpm_changelog");
    }
    lua_setmetatable(L, -2);

    return box;
}


















/*
static alpm_list_t **push_alpmlist_box(lua_State *L)
{
    alpm_list_t **box = lua_newuserdata(L, sizeof(alpm_list_t*));
    *box = NULL;

    if (luaL_newmetatable(L, "alpm_list_t")) {
        static luaL_Reg const methods[] = {
//            { "option_set_cachedirs",   lalpm_option_set_cachedirs },
            { NULL,                     NULL },
        };
        lua_newtable(L);
        luaL_register(L, NULL, methods);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);

    return box;
}
*/

/* int alpm_initialize(void); */
static int lalpm_initialize(lua_State *L)
{
    const int result = alpm_initialize();
    if (!lua_pushthread(L)) {
        luaL_error(L, "Can only initialize alpm from the main thread.");
    }
    GlobalState = L;
    lua_pushnumber(L, result);
    return 1;
}

/* int alpm_release(void); */
static int lalpm_release(lua_State *L)
{
    const int result = alpm_release();
    lua_pushnumber(L, result);

    return 1;
}

/* const char *alpm_version(void); */
static int lalpm_version(lua_State *L)
{
    const char *result = alpm_version();
    lua_pushstring(L, result);

    return 1;
}
/* const char *alpm_option_get_root(); */
static int lalpm_option_get_root(lua_State *L)
{
    const char *result = alpm_option_get_root();
    lua_pushstring(L, result);

    return 1;
}

/* int alpm_option_set_root(const char *root); */
static int lalpm_option_set_root(lua_State *L)
{
    const char *root = luaL_checkstring(L, 1);
    const int result = alpm_option_set_root(root);
    lua_pushnumber(L, result);

    return 1;
}

/* const char *alpm_option_get_dbpath(); */
static int lalpm_option_get_dbpath(lua_State *L)
{
    const char *result = alpm_option_get_dbpath();
    lua_pushstring(L, result);

    return 1;
}

/* int alpm_option_set_dbpath(const char *dbpath); */
static int lalpm_option_set_dbpath(lua_State *L)
{
    const char *dbpath = luaL_checkstring(L, 1);
    const int result = alpm_option_set_dbpath(dbpath);
    lua_pushnumber(L, result);

    return 1;
}

/* alpm_list_t *alpm_option_get_cachedirs(); */
static int lalpm_option_get_cachedirs(lua_State *L)
{
    alpm_list_t *list = alpm_option_get_cachedirs();
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* void alpm_option_set_cachedirs(alpm_list_t *cachedirs); */
static int lalpm_option_set_cachedirs(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    alpm_list_t *list = lstring_table_to_alpm_list(L, 1);
    alpm_option_set_cachedirs(list);

    return 0;
}

/* int alpm_option_add_cachedir(const char *cachedir); */
static int lalpm_option_add_cachedir(lua_State *L)
{
    const char *cachedir = luaL_checkstring(L, 1);
    const int result = alpm_option_add_cachedir(cachedir);
    lua_pushnumber(L, result);

    return 1;
}

/* void alpm_option_set_cachedirs(alpm_list_t *cachedirs); */

/* int alpm_option_remove_cachedir(const char *cachedir); */
static int lalpm_option_remove_cachedir(lua_State *L)
{
    const char *cachedir = luaL_checkstring(L, 1);
    const int result = alpm_option_remove_cachedir(cachedir);
    lua_pushnumber(L, result);

    return 1;
}

/* const char *alpm_option_get_logfile(); */
static int lalpm_option_get_logfile(lua_State *L)
{
    const char *result = alpm_option_get_logfile();
    lua_pushstring(L, result);

    return 1;
}

/* int alpm_option_set_logfile(const char *logfile); */
static int lalpm_option_set_logfile(lua_State *L)
{
    const char *logfile = luaL_checkstring(L, 1);
    const int result = alpm_option_set_logfile(logfile);
    lua_pushnumber(L, result);

    return 1;
}

/* const char *alpm_option_get_lockfile(); */
static int lalpm_option_get_lockfile(lua_State *L)
{
    const char *result = alpm_option_get_lockfile();
    lua_pushstring(L, result);

    return 1;
}

/* no set_lockfile, path is determined from dbpath */

/* unsigned short alpm_option_get_usesyslog(); */
static int lalpm_option_get_usesyslog(lua_State *L)
{
    unsigned short result = alpm_option_get_usesyslog();
    lua_pushnumber(L, result);

    return 1;
}

/* void alpm_option_set_usesyslog(unsigned short usesyslog); */
static int lalpm_option_set_usesyslog(lua_State *L)
{
    const int usesyslog = lua_toboolean(L, 1);
    alpm_option_set_usesyslog(usesyslog);

    return 0;
}

/* alpm_list_t *alpm_option_get_noupgrades(); */
static int lalpm_option_get_noupgrades(lua_State *L)
{
    alpm_list_t *list = alpm_option_get_noupgrades();
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* void alpm_option_add_noupgrade(const char *pkg); */
static int lalpm_option_add_noupgrade(lua_State *L)
{
    const char *pkg = luaL_checkstring(L, 1);
    alpm_option_add_noupgrade(pkg);

    return 0;
}

/* void alpm_option_set_noupgrades(alpm_list_t *noupgrade); */
static int lalpm_option_set_noupgrades(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    alpm_list_t *list = lstring_table_to_alpm_list(L, 1);
    alpm_option_set_noupgrades(list);

    return 0;
}
/* int alpm_option_remove_noupgrade(const char *pkg); */
static int lalpm_option_remove_noupgrade(lua_State *L)
{
    const char *pkg = luaL_checkstring(L, 1);
    const int result = alpm_option_remove_noupgrade(pkg);
    lua_pushnumber(L, result);

    return 1;
}

/* alpm_list_t *alpm_option_get_noextracts(); */
static int lalpm_option_get_noextracts(lua_State *L)
{
    alpm_list_t *list = alpm_option_get_noextracts();
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* void alpm_option_add_noextract(const char *pkg); */
static int lalpm_option_add_noextract(lua_State *L)
{
    const char *pkg = luaL_checkstring(L, 1);
    alpm_option_add_noextract(pkg);

    return 0;
}

/* void alpm_option_set_noextracts(alpm_list_t *noextract); */
static int lalpm_option_set_noextracts(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    alpm_list_t *list = lstring_table_to_alpm_list(L, 1);
    alpm_option_set_noextracts(list);

    return 0;
}

/* int alpm_option_remove_noextract(const char *pkg); */
static int lalpm_option_remove_noextract(lua_State *L)
{
    const char *pkg = luaL_checkstring(L, 1);
    const int result = alpm_option_remove_noextract(pkg);
    lua_pushnumber(L, result);

    return 1;
}

/* alpm_list_t *alpm_option_get_ignorepkgs(); */
static int lalpm_option_get_ignorepkgs(lua_State *L)
{
    alpm_list_t *list = alpm_option_get_ignorepkgs();
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* void alpm_option_add_ignorepkg(const char *pkg); */
static int lalpm_option_add_ignorepkg(lua_State *L)
{
    const char *pkg = luaL_checkstring(L, 1);
    alpm_option_add_ignorepkg(pkg);

    return 0;
}

/* void alpm_option_set_ignorepkgs(alpm_list_t *ignorepkgs); */
static int lalpm_option_set_ignorepkgs(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    alpm_list_t *list = lstring_table_to_alpm_list(L, 1);
    alpm_option_set_ignorepkgs(list);

    return 0;
}

/* int alpm_option_remove_ignorepkg(const char *pkg); */
static int lalpm_option_remove_ignorepkg(lua_State *L)
{
    const char *pkg = luaL_checkstring(L, 1);
    const int result = alpm_option_remove_ignorepkg(pkg);
    lua_pushnumber(L, result);

    return 1;
}

/* alpm_list_t *alpm_option_get_ignoregrps(); */
static int lalpm_option_get_ignoregrps(lua_State *L)
{
    alpm_list_t *list = alpm_option_get_ignoregrps();
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* void alpm_option_add_ignoregrp(const char *grp); */
static int lalpm_option_add_ignoregrp(lua_State *L)
{
    const char *grp = luaL_checkstring(L, 1);
    alpm_option_add_ignoregrp(grp);

    return 0;
}

/* void alpm_option_set_ignoregrps(alpm_list_t *ignoregrps); */
static int lalpm_option_set_ignoregrps(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    alpm_list_t *list = lstring_table_to_alpm_list(L, 1);
    alpm_option_set_ignoregrps(list);

    return 0;
}

/* int alpm_option_remove_ignoregrp(const char *grp); */
static int lalpm_option_remove_ignoregrp(lua_State *L)
{
    const char *grp = luaL_checkstring(L, 1);
    const int result = alpm_option_remove_ignoregrp(grp);
    lua_pushnumber(L, result);

    return 1;
}

/* const char *alpm_option_get_arch(); */
/* does not exist in Pacman v3.3.0 - libalpm v4.0.0, in git */
/*
static int lalpm_option_get_arch(lua_State *L)
{
    const char *result = alpm_option_get_arch();
    lua_pushstring(L, result);

    return 1;
}
*/

/* void alpm_option_set_arch(const char *arch); */
/* does not exist in Pacman v3.3.0 - libalpm v4.0.0, in git */
/*
static int lalpm_option_set_arch(lua_State *L)
{
    const char *arch = luaL_checkstring(L, 1);
    alpm_option_set_arch(arch);

    return 0;
}
*/

/* void alpm_option_set_usedelta(unsigned short usedelta); */
static int lalpm_option_set_usedelta(lua_State *L)
{
    const int usedelta = lua_toboolean(L, 1);
    alpm_option_set_usedelta(usedelta);

    return 0;
}

/* pmdb_t *alpm_option_get_localdb(); */
static int lalpm_option_get_localdb(lua_State *L)
{
    pmdb_t **box = push_pmdb_box(L);
    *box = alpm_option_get_localdb();
    if (*box == NULL) {
            /*luaL_error(L, "got a NULL pmdb_t from alpm_option_get_localdb()"); */
        lua_pushnil(L);
    }

    return 1;
}

/* alpm_list_t *alpm_option_get_syncdbs(); */
static int lalpm_option_get_syncdbs(lua_State *L)
{
    alpm_list_t *list = alpm_option_get_syncdbs();
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, PMDB_T);

    return 1;
}

/* pmdb_t *alpm_db_register_local(void); */
static int lalpm_db_register_local(lua_State *L)
{
    pmdb_t **box = push_pmdb_box(L);
    *box = alpm_db_register_local();
    if (*box == NULL) {
        /*luaL_error(L, "got a NULL pmdb_t from alpm_db_register_local()");*/
        lua_pushnil(L);
    }

    return 1;
}

/* pmdb_t *alpm_db_register_sync(const char *treename); */
static int lalpm_db_register_sync(lua_State *L)
{
    pmdb_t **box = push_pmdb_box(L);
    const char *treename = luaL_checkstring(L, 1);
    *box = alpm_db_register_sync(treename);
    if (*box == NULL) {
        /*luaL_error(L, "got a NULL pmdb_t from alpm_db_register_sync(treename)");*/
        lua_pushnil(L);
    }

    return 1;
}

/* int alpm_db_unregister_all(void); */
static int lalpm_db_unregister_all(lua_State *L)
{
    const int result = alpm_db_unregister_all();
    lua_pushnumber(L, result);

    return 1;
}
/* char *alpm_fetch_pkgurl(const char *url); */
static int lalpm_fetch_pkgurl(lua_State *L)
{
    const char *url = luaL_checkstring(L, 1);
    const char *result = alpm_fetch_pkgurl(url);
    lua_pushstring(L, result);
    free((void*)result);

    return 1;
}
/* int alpm_pkg_vercmp(const char *a, const char *b); */
static int lalpm_pkg_vercmp(lua_State *L)
{
    const char *a = luaL_checkstring(L, 1);
    const char *b = luaL_checkstring(L, 2);
    const int result = alpm_pkg_vercmp(a, b);
    lua_pushnumber(L, result);

    return 1;
}

/* pmtranstype_t alpm_trans_get_type(); */
static int lalpm_trans_get_type(lua_State *L)
{
    const char *list[] = {"T_T_UPGRADE","T_T_REMOVE","T_T_REMOVEUPGRADE","T_T_SYNC"};
    const int result = alpm_trans_get_type();
    if (result == -1) {
        lua_pushnumber(L, result);
    }
    else {
        lua_pushstring(L, list[result-1]);
    }

    return 1;
}

/* unsigned int alpm_trans_get_flags(); */
static int lalpm_trans_get_flags(lua_State *L)
{
    const unsigned int result = alpm_trans_get_flags();
    lua_pushnumber(L, result);

    return 1;
}
/* alpm_list_t * alpm_trans_get_pkgs(); */
static int lalpm_trans_get_pkgs(lua_State *L)
{
    alpm_list_t *list = alpm_trans_get_pkgs();
//    alpm_list_to_lstring_table(L, list);
    alpm_list_to_any_table(L, list, PMPKG_T);

    return 1;
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

static void
log_internal_error(const char *message, const char *context)
{
    /* TODO: send the error message to the libalpm logger here? */
    fprintf(stderr, "lualpm %s: %s\n", context, message);
}

static void
handle_pcall_error_unprotected(
    lua_State  *L,
    int         err,
    const char *context)
{
    switch (err) {
    case 0:                     /* success */
        break;
    case LUA_ERRMEM:
        log_internal_error("lualpm: ran out of memory calling Lua", context);
        break;
    case LUA_ERRERR:
    case LUA_ERRRUN:
        /* If a pcall fails it will push an error message or error
         * object on the stack.  The following usage of lua_type()
         * and lua_tostring() is safe by inspection of the Lua source
         * code -- this particular usage cannot throw Lua errors. */
        if (lua_type(L, -1) == LUA_TSTRING) {
            char const *msg = lua_tostring(L, -1);
            log_internal_error(msg, context);
        }
        else {
            log_internal_error("lualpm: error calling Lua "
                               "(received a non-string error object)",
                               context);
        }
    default:
        log_internal_error("lualpm: unknown error while calling Lua",
                           context);
    }
}

/* We use addresses of structs describing a callback as the key to a
 * callback in the Lua registry. */
typedef struct {
    const char *name;
} callback_key_t;

static callback_key_t trans_cb_progress_key[1] = {{ "transaction progress" }};
static callback_key_t trans_cb_event_key[1] = {{ "transaction event" }};
static callback_key_t trans_cb_conv_key[1] = {{ "transaction conversation" }};

static void
register_callback(lua_State *L, callback_key_t *key, int narg)
{
    lua_pushlightuserdata(L, key);
    lua_pushvalue(L, narg);
    lua_settable(L, LUA_REGISTRYINDEX);
}

static void
get_callback(lua_State *L, callback_key_t *key)
{
    lua_pushlightuserdata(L, key);
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (lua_isnil(L, -1)) {
        luaL_error(L, "no %s callback set!", key->name);
    }
}

struct progress_cb_args {
    pmtransprog_t  progress_type;
    const char    *pkg_name;
    int            percent;
    int            pkg_count;
    int            pkg_current;
};

/* This is a protected gateway for the progress callback.  It receives
 * the progress callback arguments on the stack as a lightuserdata
 * pointing to a struct progress_cb_args. */
static int
progress_cb_gateway_protected(lua_State *L)
{
    struct progress_cb_args *args = lua_touserdata(L, 1);

    /* We'll look for a callback to call in the registry. */
    get_callback(L, trans_cb_progress_key);
    push_pmtransprog(L, args->progress_type);
    push_string(L, args->pkg_name);
    lua_pushnumber(L, args->percent);
    lua_pushnumber(L, args->pkg_count);
    lua_pushnumber(L, args->pkg_current);
    lua_call(L, 3, 0);

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
    if (err)
        handle_pcall_error_unprotected(L, err, "progress callback");
}

/* int alpm_trans_init(pmtranstype_t type, pmtransflag_t flags,
                    alpm_trans_cb_event cb_event, alpm_trans_cb_conv conv,
                    alpm_trans_cb_progress cb_progress); */
static int lalpm_trans_init(lua_State *L)
{
    pmtranstype_t type = lstring_to_transtype(L, 1);
    pmtransflag_t flags = lstring_table_to_transflag(L, 2);
    register_callback(L, trans_cb_event_key, 3);
    register_callback(L, trans_cb_conv_key, 4);
    register_callback(L, trans_cb_progress_key, 5);
    const int result = alpm_trans_init(type, flags,
                                       NULL,
                                       NULL,
                                       progress_cb_gateway_unprotected);
    lua_pushnumber(L, result);
    return 1;
}

/* int alpm_trans_sysupgrade(int enable_downgrade); */
static int lalpm_trans_sysupgrade(lua_State *L)
{
    const int enable_downgrade = lua_tointeger(L, 1);
    const int result = alpm_trans_sysupgrade(enable_downgrade);
    lua_pushnumber(L, result);

    return 1;
}

/* int alpm_trans_addtarget(char *target); */
static int lalpm_trans_addtarget(lua_State *L)
{
    char *targetcopy;
    const char *target = luaL_checkstring(L, 1);
    targetcopy = strdup(target);
    const int result = alpm_trans_addtarget(targetcopy);
    free(targetcopy);
    lua_pushnumber(L, result);

    return 1;
}

/* int alpm_trans_prepare(alpm_list_t **data); */
static int lalpm_trans_prepare(lua_State *L)
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
                //printf("conflicting deps\n");
                break;
            case PM_ERR_FILE_CONFLICTS:
                alpm_list_to_any_table(L, list, PMFILECONFLICT_T);
                break;
            default:
                //printf("hmph\n");
                break;
        }
        //printf("HMPH\n");
        return 2;
    }

    return 1;
}

/* int alpm_trans_commit(alpm_list_t **data); */
static int lalpm_trans_commit(lua_State *L)
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
static int lalpm_trans_interrupt(lua_State *L)
{
    const int result = alpm_trans_interrupt();
    lua_pushnumber(L, result);

    return 1;
}
/* int alpm_trans_release(void); */
static int lalpm_trans_release(lua_State *L)
{
    const int result = alpm_trans_release();
    lua_pushnumber(L, result);

    return 1;
}

/* int alpm_depcmp(pmpkg_t *pkg, pmdepend_t *dep); */
static int lalpm_depcmp(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    pmdepend_t *dep = check_pmdepend(L, 2);
    const int result = alpm_depcmp(pkg, dep);
    lua_pushnumber(L, result);

    return 1;
}

/* alpm_list_t *alpm_checkdeps(alpm_list_t *pkglist, int reversedeps,
		alpm_list_t *remove, alpm_list_t *upgrade); */
static int lalpm_checkdeps(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    alpm_list_t *pkglist = lstring_table_to_alpm_list(L, 1);
    const int reversedeps = luaL_checknumber(L, 2);
    luaL_checktype(L, 2, LUA_TTABLE);
    alpm_list_t *remove = lstring_table_to_alpm_list(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    alpm_list_t *upgrade = lstring_table_to_alpm_list(L, 3);
    alpm_list_t *result = alpm_checkdeps(pkglist, reversedeps, remove, upgrade);
//    alpm_list_to_lstring_table(L, result);
    alpm_list_to_any_table(L, result, PMPKG_T);

    return 1;
}
/* alpm_list_t *alpm_deptest(pmdb_t *db, alpm_list_t *targets); */
static int lalpm_deptest(lua_State *L)
{
    pmdb_t *db = check_pmdb(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    alpm_list_t *targets = lstring_table_to_alpm_list(L, 2);
    alpm_list_t *result = alpm_deptest(db, targets);
//    alpm_list_to_lstring_table(L, result);
    alpm_list_to_any_table(L, result, STRING);

    return 1;
}

/* char *alpm_compute_md5sum(const char *name); */
static int lalpm_compute_md5sum(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    const char *result = alpm_compute_md5sum(name);
    lua_pushstring(L, result);

    return 1;
}

/* const char *alpm_strerror(int err); */
static int lalpm_strerror(lua_State *L)
{
    const int err = luaL_checknumber(L, 1);
    const char *result = alpm_strerror(err);
    lua_pushstring(L, result);

    return 1;
}

/* const char *alpm_strerrorlast(void); */
static int lalpm_strerrorlast(lua_State *L)
{
    const char *result = alpm_strerrorlast();
    lua_pushstring(L, result);

    return 1;
}


static luaL_Reg const pkg_funcs[] =
{
    { "pkg_load",                   lalpm_pkg_load },
    { "initialize",                 lalpm_initialize }, /* works */
    { "release",                    lalpm_release }, /* works */
    { "version",                    lalpm_version }, /* works */
    { "option_get_root",            lalpm_option_get_root }, /* works */
    { "option_set_root",            lalpm_option_set_root }, /* works */
    { "option_get_dbpath",          lalpm_option_get_dbpath }, /* works */
    { "option_set_dbpath",          lalpm_option_set_dbpath }, /* works */
    { "option_get_cachedirs",       lalpm_option_get_cachedirs }, /* works */
    { "option_add_cachedir",        lalpm_option_add_cachedir }, /* works */
    { "option_set_cachedirs",       lalpm_option_set_cachedirs }, /* works */
    { "option_remove_cachedir",     lalpm_option_remove_cachedir },
    { "option_get_logfile",         lalpm_option_get_logfile }, /* works */
    { "option_set_logfile",         lalpm_option_set_logfile }, /* works */
    { "option_get_lockfile",        lalpm_option_get_lockfile }, /* works */
    { "option_get_usesyslog",       lalpm_option_get_usesyslog }, /* works */
    { "option_set_usesyslog",       lalpm_option_set_usesyslog }, /* works */
    { "option_get_noupgrades",      lalpm_option_get_noupgrades }, /* works */
    { "option_add_noupgrade",       lalpm_option_add_noupgrade }, /* works */
    { "option_set_noupgrades",      lalpm_option_set_noupgrades },
    { "option_remove_noupgrade",    lalpm_option_remove_noupgrade }, /* works */
    { "option_get_noextracts",      lalpm_option_get_noextracts }, /* works */
    { "option_add_noextract",       lalpm_option_add_noextract }, /* works */
    { "option_set_noextracts",      lalpm_option_set_noextracts },
    { "option_remove_noextract",    lalpm_option_remove_noextract }, /* works */
    { "option_get_ignorepkgs",      lalpm_option_get_ignorepkgs }, /* works */
    { "option_add_ignorepkg",       lalpm_option_add_ignorepkg }, /* works */
    { "option_set_ignorepkgs",      lalpm_option_set_ignorepkgs },
    { "option_remove_ignorepkg",    lalpm_option_remove_ignorepkg }, /* works */
    { "option_get_ignoregrps",      lalpm_option_get_ignoregrps }, /* works */
    { "option_add_ignoregrp",       lalpm_option_add_ignoregrp }, /* works */
    { "option_set_ignoregrps",      lalpm_option_set_ignoregrps },
    { "option_remove_ignoregrp",    lalpm_option_remove_ignoregrp }, /* works */
/*    { "option_get_arch",            lalpm_option_get_arch }, */
/*    { "option_set_arch",            lalpm_option_set_arch }, */
    { "option_set_usedelta",        lalpm_option_set_usedelta }, /* works */
    { "option_get_localdb",         lalpm_option_get_localdb }, /* works */
    { "option_get_syncdbs",         lalpm_option_get_syncdbs }, /* works */
    { "db_register_local",          lalpm_db_register_local }, /* works */
    { "db_register_sync",           lalpm_db_register_sync }, /* works */
    { "db_unregister_all",          lalpm_db_unregister_all },
    { "fetch_pkgurl",               lalpm_fetch_pkgurl },
    { "pkg_vercmp",                 lalpm_pkg_vercmp },
    { "trans_get_type",             lalpm_trans_get_type },
    { "trans_init",                 lalpm_trans_init },
    { "trans_get_flags",            lalpm_trans_get_flags },
    { "trans_get_pkgs",             lalpm_trans_get_pkgs },
    { "trans_sysupgrade",           lalpm_trans_sysupgrade },
    { "trans_addtarget",            lalpm_trans_addtarget },
    { "trans_prepare",              lalpm_trans_prepare },
    { "trans_commit",               lalpm_trans_commit },
    { "trans_interrupt",            lalpm_trans_interrupt },
    { "trans_release",              lalpm_trans_release },

    { "depcmp",                     lalpm_depcmp },
    { "checkdeps",                  lalpm_checkdeps },
    { "deptest",                    lalpm_deptest },
    { "sync_newversion",            lalpm_sync_newversion },
    { "compute_md5sum",             lalpm_compute_md5sum },

    { "strerror",                   lalpm_strerror },
    { "strerrorlast",               lalpm_strerrorlast },
    { NULL,                         NULL }
};

int luaopen_lualpm(lua_State *L)
{
    lua_newtable(L);
    luaL_register(L, NULL, pkg_funcs);

    return 1;
}
