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


/* A proxy to alpm_logaction() to be called from the Lua side of the
 * fence.  Unlike libalpm's alpm_logaction(), this function does not
 * support a format string, but will merely concatenate its arguments
 * into a single string instead. */
static int
lalpm_logaction(lua_State *L)
{
    char const *s;
    lua_concat(L, lua_gettop(L));
    s = lua_tostring(L, -1);
    if (!s) {
        return luaL_error(L, "arguments must be convertible to strings");
    }
    alpm_logaction("%s", s);
    return 0;
}

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
/* int alpm_pkg_load(const char *filename, int full, pmpkg_t **pkg); */
/* lua prototype is pkg, ret = alpm.pkg_load(filename, true/false) */
static int lalpm_pkg_load(lua_State *L)
{
    pmpkg_t *pkg = NULL;
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
    alpm_list_to_any_table(L, list, STRING);
    FREELIST(list);

    return 1;
}
/* const char *alpm_pkg_get_filename(pmpkg_t *pkg); */
static int lalpm_pkg_get_filename(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_filename(pkg);
    push_string(L, result);

    return 1;
}
/* const char *alpm_pkg_get_name(pmpkg_t *pkg); */
static int lalpm_pkg_get_name(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_name(pkg);
    push_string(L, result);

    return 1;
}

/* const char *alpm_pkg_get_version(pmpkg_t *pkg); */
static int lalpm_pkg_get_version(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_version(pkg);
    push_string(L, result);

    return 1;
}

/* const char *alpm_pkg_get_desc(pmpkg_t *pkg); */
static int lalpm_pkg_get_desc(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_desc(pkg);
    push_string(L, result);

    return 1;
}

/* const char *alpm_pkg_get_url(pmpkg_t *pkg); */
static int lalpm_pkg_get_url(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_url(pkg);
    push_string(L, result);

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
    push_string(L, result);

    return 1;
}

/* const char *alpm_pkg_get_md5sum(pmpkg_t *pkg); */
static int lalpm_pkg_get_md5sum(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_md5sum(pkg);
    push_string(L, result);

    return 1;
}

/* const char *alpm_pkg_get_arch(pmpkg_t *pkg); */
static int lalpm_pkg_get_arch(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    const char *result = alpm_pkg_get_arch(pkg);
    push_string(L, result);

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
    push_string(L, PKGREASON_TOSTR[reason]);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_licenses(pmpkg_t *pkg); */
static int lalpm_pkg_get_licenses(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_licenses(pkg);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_groups(pmpkg_t *pkg); */
static int lalpm_pkg_get_groups(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_groups(pkg);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_depends(pmpkg_t *pkg); */
static int lalpm_pkg_get_depends(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_depends(pkg);
    alpm_list_to_any_table(L, list, PMDEPEND_T);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_optdepends(pmpkg_t *pkg); */
static int lalpm_pkg_get_optdepends(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_optdepends(pkg);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_conflicts(pmpkg_t *pkg); */
static int lalpm_pkg_get_conflicts(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_conflicts(pkg);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_provides(pmpkg_t *pkg); */
static int lalpm_pkg_get_provides(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_provides(pkg);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_deltas(pmpkg_t *pkg); */
static int lalpm_pkg_get_deltas(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_deltas(pkg);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_replaces(pmpkg_t *pkg); */
static int lalpm_pkg_get_replaces(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_replaces(pkg);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_files(pmpkg_t *pkg); */
static int lalpm_pkg_get_files(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_files(pkg);
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* alpm_list_t *alpm_pkg_get_backup(pmpkg_t *pkg); */
static int lalpm_pkg_get_backup(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    alpm_list_t *list = alpm_pkg_get_backup(pkg);
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

/* int alpm_pkg_has_scriptlet(pmpkg_t *pkg); */
static int lalpm_pkg_has_scriptlet(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    int result = alpm_pkg_has_scriptlet(pkg);
    lua_pushnumber(L, result);

    return 1;
}

/* int alpm_pkg_has_force(pmpkg_t *pkg); */
static int lalpm_pkg_has_force(lua_State *L)
{
    pmpkg_t *pkg = check_pmpkg(L, 1);
    int result = alpm_pkg_has_force(pkg);
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

alpm_list_t *ldatabase_table_to_alpm_list(lua_State *L, int narg)
{
    alpm_list_t *newlist = NULL;
    size_t i, len = lua_objlen(L, narg);
    for (i = 1; i <= len; i++) {
        lua_rawgeti(L, narg, i);
        const int index = -1;
        pmdb_t *data = *(pmdb_t**)lua_touserdata(L, index);
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
        lua_pushnil(L);
    }
    alpm_list_free(dbs_sync);
    return 1;
}

/* char *alpm_dep_compute_string(const pmdepend_t *dep); */
static int lalpm_dep_compute_string(lua_State *L)
{
    const pmdepend_t *dep = check_pmdepend(L, 1);
    char *result = alpm_dep_compute_string(dep);
    push_string(L, result);
    free((void*)result);

    return 1;
}
static pmpkg_t **push_pmpkg_box(lua_State *L)
{
    pmpkg_t **box = lua_newuserdata(L, sizeof(pmpkg_t*));
    *box = NULL;

    if (luaL_newmetatable(L, "pmpkg_t")) {
        static luaL_Reg const methods[] = {
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
            { "pkg_get_db",             lalpm_pkg_get_db },
            { "pkg_changelog_open",     lalpm_pkg_changelog_open },
            { "pkg_changelog_read",     lalpm_pkg_changelog_read },
            { "pkg_changelog_close",    lalpm_pkg_changelog_close },
            { "pkg_has_scriptlet",      lalpm_pkg_has_scriptlet },
            { "pkg_has_force",          lalpm_pkg_has_force },
            { "pkg_download_size",      lalpm_pkg_download_size },
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
    push_string(L, result);

    return 1;
}

/* const char *alpm_delta_get_to(pmdelta_t *delta); */
static int lalpm_delta_get_to(lua_State *L)
{
    pmdelta_t *delta = check_pmdelta(L, 1);
    const char *result = alpm_delta_get_to(delta);
    push_string(L, result);

    return 1;
}

/* const char *alpm_delta_get_filename(pmdelta_t *delta); */
static int lalpm_delta_get_filename(lua_State *L)
{
    pmdelta_t *delta = check_pmdelta(L, 1);
    const char *result = alpm_delta_get_filename(delta);
    push_string(L, result);

    return 1;
}

/* const char *alpm_delta_get_md5sum(pmdelta_t *delta); */
static int lalpm_delta_get_md5sum(lua_State *L)
{
    pmdelta_t *delta = check_pmdelta(L, 1);
    const char *result = alpm_delta_get_md5sum(delta);
    push_string(L, result);

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
    push_string(L, result);

    return 1;
}

/* alpm_list_t *alpm_grp_get_pkgs(const pmgrp_t *grp); */
static int lalpm_grp_get_pkgs(lua_State *L)
{
    const pmgrp_t *grp = check_pmgrp(L, 1);
    alpm_list_t *list = alpm_grp_get_pkgs(grp);
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

static pmconflict_t **push_pmconflict_box(lua_State *L)
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

static pmfileconflict_t **push_pmfileconflict_box(lua_State *L)
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

static changelog *push_changelog_box(lua_State *L)
{
    changelog *box = lua_newuserdata(L, sizeof(changelog));

    if (luaL_newmetatable(L, "alpm_changelog")) {
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

/* int alpm_initialize(void); */
static int lalpm_initialize(lua_State *L)
{
    const int result = alpm_initialize();
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
    push_string(L, result);

    return 1;
}

static int
push_loglevel(lua_State *L, pmloglevel_t level)
{
    switch(level) {
#define f(x) case PM_LOG_ ## x: return push_string(L, "LOG_" #x)
        f(ERROR);
        f(WARNING);
        f(DEBUG);
        f(FUNCTION);
#undef f
        default:
            assert(0 && "[BUG] unexpected pmloglevel_t");
    }
    return 0;
}


/* alpm_cb_log alpm_option_get_logcb(); */
/* void alpm_option_set_logcb(alpm_cb_log cb); */
static callback_key_t log_cb_key[1] = {{ "log callback" }};

struct log_cb_args {
    pmloglevel_t level;
    const char  *msg;
};

static int
log_cb_gateway_protected(lua_State *L)
{
    struct log_cb_args *args = lua_touserdata(L, 1);
    get_callback(L, log_cb_key);
    push_loglevel(L, args->level);
    push_string(L, args->msg);
    lua_call(L, 2, 0);

    return 0;
}

static void
log_cb_gateway_unprotected(pmloglevel_t level, char *fmt, va_list list)
{
    lua_State *L = GlobalState;
    struct log_cb_args args[1];
    int err;
    assert(L && "[BUG] no global Lua state in log callback");
    args->level = level;
    char *s = NULL;
    err = vasprintf(&s, fmt, list);
    assert(err != -1 && "[BUG] out of memory");
    args->msg = s;
    err = lua_cpcall(L, log_cb_gateway_protected, args);
    free(s);
    if (err) {
        handle_pcall_error_unprotected(L, err, "log callback");
    }
}

static int lalpm_option_set_logcb(lua_State *L)
{
    register_callback(L, log_cb_key, 1);
    alpm_option_set_logcb(log_cb_gateway_unprotected);

    return 0;
}

/* alpm_cb_download alpm_option_get_dlcb(); */
/* void alpm_option_set_dlcb(alpm_cb_download cb); */
static callback_key_t dl_cb_key[1] = {{ "download progress" }};

struct dl_cb_args {
    const char  *filename;
    off_t       xfered;
    off_t       total;
};

static int
dl_cb_gateway_protected(lua_State *L)
{
    struct dl_cb_args *args = lua_touserdata(L, 1);
    get_callback(L, dl_cb_key);
    push_string(L, args->filename);
    lua_pushnumber(L, args->xfered);
    lua_pushnumber(L, args->total);
    lua_call(L, 3, 0);

    return 0;
}

static void
dl_cb_gateway_unprotected(const char *f, off_t x, off_t t)
{
    lua_State *L = GlobalState;
    struct dl_cb_args args[1];
    int err;
    assert(L && "[BUG] no global Lua state in download progress callback");
    args->filename = f;
    args->xfered = x;
    args->total = t;
    err = lua_cpcall(L, dl_cb_gateway_protected, args);
    if (err) {
        handle_pcall_error_unprotected(L, err, "download callback");
    }
}

static int lalpm_option_set_dlcb(lua_State *L)
{
    register_callback(L, dl_cb_key, 1);
    alpm_option_set_dlcb(dl_cb_gateway_unprotected);

    return 0;
}

/* alpm_cb_fetch alpm_option_get_fetchcb(); */
/* void alpm_option_set_fetchcb(alpm_cb_fetch cb); */

/* alpm_cb_totaldl alpm_option_get_totaldlcb(); */
/* void alpm_option_set_totaldlcb(alpm_cb_totaldl cb); */
static callback_key_t totaldl_cb_key[1] = {{ "total download progress" }};

struct totaldl_cb_args {
    off_t       total;
};

static int
totaldl_cb_gateway_protected(lua_State *L)
{
    struct totaldl_cb_args *args = lua_touserdata(L, 1);
    get_callback(L, totaldl_cb_key);
    lua_pushnumber(L, args->total);
    lua_call(L, 1, 0);

    return 0;
}

static void
totaldl_cb_gateway_unprotected(off_t t)
{
    lua_State *L = GlobalState;
    struct totaldl_cb_args args[1];
    int err;
    assert(L && "[BUG] no global Lua state in total download progress callback");
    args->total = t;
    err = lua_cpcall(L, totaldl_cb_gateway_protected, args);
    if (err) {
        handle_pcall_error_unprotected(L, err, "total download callback");
    }
}

static int lalpm_option_set_totaldlcb(lua_State *L)
{
    register_callback(L, totaldl_cb_key, 1);
    alpm_option_set_totaldlcb(totaldl_cb_gateway_unprotected);

    return 0;
}

/* const char *alpm_option_get_root(); */
static int lalpm_option_get_root(lua_State *L)
{
    const char *result = alpm_option_get_root();
    push_string(L, result);

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
    push_string(L, result);

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
    push_string(L, result);

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
    push_string(L, result);

    return 1;
}

/* no set_lockfile, path is determined from dbpath */

/* int alpm_option_get_usesyslog(); */
static int lalpm_option_get_usesyslog(lua_State *L)
{
    int result = alpm_option_get_usesyslog();
    lua_pushnumber(L, result);

    return 1;
}

/* void alpm_option_set_usesyslog(int usesyslog); */
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
static int lalpm_option_get_arch(lua_State *L)
{
    const char *result = alpm_option_get_arch();
    push_string(L, result);

    return 1;
}

/* void alpm_option_set_arch(const char *arch); */
static int lalpm_option_set_arch(lua_State *L)
{
    const char *arch = luaL_checkstring(L, 1);
    alpm_option_set_arch(arch);

    return 0;
}

/* void alpm_option_set_usedelta(int usedelta); */
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
        lua_pushnil(L);
    }

    return 1;
}

/* alpm_list_t *alpm_option_get_syncdbs(); */
static int lalpm_option_get_syncdbs(lua_State *L)
{
    alpm_list_t *list = alpm_option_get_syncdbs();
    alpm_list_to_any_table(L, list, PMDB_T);

    return 1;
}

/* pmdb_t *alpm_db_register_local(void); */
static int lalpm_db_register_local(lua_State *L)
{
    pmdb_t **box = push_pmdb_box(L);
    *box = alpm_db_register_local();
    if (*box == NULL) {
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
    push_string(L, result);
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

/* unsigned int alpm_trans_get_flags(); */
static int lalpm_trans_get_flags(lua_State *L)
{
    pmtransflag_t x = alpm_trans_get_flags();
    if (x == (pmtransflag_t)-1) {
        raise_last_pm_error(L);
    }

    return push_transflags_table(L, x);
}

/* alpm_list_t * alpm_trans_get_add(); */
static int lalpm_trans_get_add(lua_State *L)
{
    alpm_list_t *list = alpm_trans_get_add();
    alpm_list_to_any_table(L, list, PMPKG_T);

    return 1;
}

/* alpm_list_t * alpm_trans_get_remove(); */
static int lalpm_trans_get_remove(lua_State *L)
{
    alpm_list_t *list = alpm_trans_get_remove();
    alpm_list_to_any_table(L, list, PMPKG_T);

    return 1;
}

struct event_cb_args {
    pmtransevt_t    event;
    void            *data1;
    void            *data2;
};

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
        handle_pcall_error_unprotected(L, err, "event callback");
    }
}

struct conv_cb_args {
    pmtransconv_t   type;
    void            *data1;
    void            *data2;
    void            *data3;
    int             *response;
};

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
        handle_pcall_error_unprotected(L, err, "conversation callback");
    }
}

static callback_key_t trans_cb_progress_key[1] = {{ "transaction progress" }};

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
        handle_pcall_error_unprotected(L, err, "progress callback");
    }
}

/* int alpm_trans_init(pmtransflag_t flags,
                    alpm_trans_cb_event cb_event, alpm_trans_cb_conv conv,
                    alpm_trans_cb_progress cb_progress); */
static int lalpm_trans_init(lua_State *L)
{
    pmtransflag_t flags = check_transflags_table(L, 1);
    lua_pushvalue(L, 2);
    lua_setfield(L, LUA_REGISTRYINDEX, "lualpm: events callback table");
    lua_pushvalue(L, 3);
    lua_setfield(L, LUA_REGISTRYINDEX, "lualpm: conversations callback table");
    register_callback(L, trans_cb_progress_key, 4);
    const int result = alpm_trans_init(flags,
                                       event_cb_gateway_unprotected,
                                       conversation_cb_gateway_unprotected,
                                       progress_cb_gateway_unprotected);
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

/* int alpm_sync_sysupgrade(int enable_downgrade); */
static int lalpm_sync_sysupgrade(lua_State *L)
{
    const int enable_downgrade = lua_tointeger(L, 1);
    const int result = alpm_sync_sysupgrade(enable_downgrade);
    lua_pushnumber(L, result);

    return 1;
}

/* int alpm_sync_target(char *target); */
static int lalpm_sync_target(lua_State *L)
{
    const char *pkgname;
    int result;

    pkgname = luaL_checkstring(L, 1);
    result  = alpm_sync_target((char *)pkgname);

    lua_pushinteger(L, result);
    return 1;
}

/* I would rename the following three functions if I were the owner...
   think these functions have horrible names.  dbtarget is not so
   bad... the others?  bleh.  -juster */

/* "trans_sync_fromdb" */
/* int alpm_sync_dbtarget(char *db, char *target); */
static int lalpm_sync_dbtarget(lua_State *L)
{
    const char *db, *pkgname;
    int result;

    db      = luaL_checkstring(L, 1);
    pkgname = luaL_checkstring(L, 2);
    
    result = alpm_sync_dbtarget((char *)db, (char *)pkgname);

    lua_pushinteger(L, result);
    return 1;
}

/* "trans_add_pkgfile" this adds a pkgfile to the transaction */
/* int alpm_add_target(char *target); */
static int lalpm_add_target(lua_State *L)
{
    const char *pkgfile;
    int result;

    pkgfile = luaL_checkstring(L, 1);
    result  = alpm_add_target((char *)pkgfile);

    lua_pushinteger(L, result);
    return 1;
}    

/* Denotes a package to be removed.
 * It does not remove a target from the to-be-installed list.
 * "trans_uninstall" perhaps?  This name is the worst.
 * int alpm_remove_target(char *target); */
static int lalpm_remove_target(lua_State *L)
{
    const char *pkgname;
    int result;

    pkgname = luaL_checkstring(L, 1);
    result  = alpm_remove_target((char *)pkgname);

    lua_pushinteger(L, result);
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
    alpm_list_to_any_table(L, result, STRING);

    return 1;
}

/* char *alpm_compute_md5sum(const char *name); */
static int lalpm_compute_md5sum(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    const char *result = alpm_compute_md5sum(name);
    push_string(L, result);

    return 1;
}

/* const char *alpm_strerror(int err); */
static int lalpm_strerror(lua_State *L)
{
    const int err = luaL_checknumber(L, 1);
    const char *result = alpm_strerror(err);
    push_string(L, result);

    return 1;
}

/* const char *alpm_strerrorlast(void); */
static int lalpm_strerrorlast(lua_State *L)
{
    const char *result = alpm_strerrorlast();
    push_string(L, result);

    return 1;
}

static int lalpm_pm_errno(lua_State *L)
{
    if (!pm_errno) {
        lua_pushnil(L);
        return 1;
    } else {
        switch(pm_errno) {
#define Sym(x) case PM_ERR_ ## x: return push_string(L, "P_E_" #x)
        Sym(MEMORY);
        Sym(SYSTEM);
        Sym(BADPERMS);
        Sym(NOT_A_FILE);
        Sym(NOT_A_DIR);
        Sym(WRONG_ARGS);
            /* Interface */
        Sym(HANDLE_NULL);
        Sym(HANDLE_NOT_NULL);
        Sym(HANDLE_LOCK);
            /* Databases */
        Sym(DB_OPEN);
        Sym(DB_CREATE);
        Sym(DB_NULL);
        Sym(DB_NOT_NULL);
        Sym(DB_NOT_FOUND);
        Sym(DB_WRITE);
        Sym(DB_REMOVE);
            /* Servers */
        Sym(SERVER_BAD_URL);
        Sym(SERVER_NONE);
            /* Transactions */
        Sym(TRANS_NOT_NULL);
        Sym(TRANS_NULL);
        Sym(TRANS_DUP_TARGET);
        Sym(TRANS_NOT_INITIALIZED);
        Sym(TRANS_NOT_PREPARED);
        Sym(TRANS_ABORT);
        Sym(TRANS_TYPE);
        Sym(TRANS_NOT_LOCKED);
            /* Packages */
        Sym(PKG_NOT_FOUND);
        Sym(PKG_IGNORED);
        Sym(PKG_INVALID);
        Sym(PKG_OPEN);
        Sym(PKG_CANT_REMOVE);
        Sym(PKG_INVALID_NAME);
        Sym(PKG_INVALID_ARCH);
        Sym(PKG_REPO_NOT_FOUND);
            /* Deltas */
        Sym(DLT_INVALID);
        Sym(DLT_PATCHFAILED);
            /* Dependencies */
        Sym(UNSATISFIED_DEPS);
        Sym(CONFLICTING_DEPS);
        Sym(FILE_CONFLICTS);
            /* Misc */
        Sym(RETRIEVE);
        Sym(INVALID_REGEX);
            /* External library errors */
        Sym(LIBARCHIVE);
        Sym(LIBFETCH);
        Sym(EXTERNAL_DOWNLOAD);
#undef Sym
        default:
            assert(0 && "[BUG] unexpected pm_errno");
        }
    }
    return 0;
}


static luaL_Reg const pkg_funcs[] =
{
    { "pkg_load",                   lalpm_pkg_load },
    { "initialize",                 lalpm_initialize }, /* works */
    { "release",                    lalpm_release }, /* works */
    { "version",                    lalpm_version }, /* works */
//    { "option_get_logcb",           lalpm_option_get_logcb },
    { "option_set_logcb",           lalpm_option_set_logcb },
//    { "option_get_dlcb",            lalpm_option_get_dlcb },
    { "option_set_dlcb",            lalpm_option_set_dlcb },
//    { "option_get_fetchcb",         lalpm_option_get_fetchcb },
//    { "option_set_fetchcb",         lalpm_option_set_fetchb },
//    { "option_get_totaldlcb",       lalpm_option_get_totaldlcb },
    { "option_set_totaldlcb",       lalpm_option_set_totaldlcb },
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
    { "option_get_arch",            lalpm_option_get_arch },
    { "option_set_arch",            lalpm_option_set_arch },
    { "option_set_usedelta",        lalpm_option_set_usedelta }, /* works */
    { "option_get_localdb",         lalpm_option_get_localdb }, /* works */
    { "option_get_syncdbs",         lalpm_option_get_syncdbs }, /* works */
    { "db_register_local",          lalpm_db_register_local }, /* works */
    { "db_register_sync",           lalpm_db_register_sync }, /* works */
    { "db_unregister_all",          lalpm_db_unregister_all },
    { "fetch_pkgurl",               lalpm_fetch_pkgurl },
    { "pkg_vercmp",                 lalpm_pkg_vercmp },
    { "trans_init",                 lalpm_trans_init },
    { "trans_get_flags",            lalpm_trans_get_flags },
    { "trans_get_add",              lalpm_trans_get_add },
    { "trans_get_remove",           lalpm_trans_get_remove },
    { "trans_prepare",              lalpm_trans_prepare },
    { "trans_commit",               lalpm_trans_commit },
    { "trans_interrupt",            lalpm_trans_interrupt },
    { "trans_release",              lalpm_trans_release },

    { "sync_sysupgrade",            lalpm_sync_sysupgrade },
    { "sync_target",                lalpm_sync_target     },
    { "sync_dbtarget",              lalpm_sync_dbtarget   },
    { "add_target",                 lalpm_add_target      },
    { "remove_target",              lalpm_remove_target   },

    { "depcmp",                     lalpm_depcmp },
    { "checkdeps",                  lalpm_checkdeps },
    { "deptest",                    lalpm_deptest },
    { "sync_newversion",            lalpm_sync_newversion },
    { "compute_md5sum",             lalpm_compute_md5sum },

    { "strerror",                   lalpm_strerror },
    { "strerrorlast",               lalpm_strerrorlast },
    { "pm_errno",                   lalpm_pm_errno },
    { "logaction",                  lalpm_logaction },
    { NULL,                         NULL }
};

int luaopen_lualpm(lua_State *L)
{
    if (!lua_pushthread(L)) {
        luaL_error(L, "Can only initialize alpm from the main thread.");
    }
    GlobalState = L;

    lua_newtable(L);
    luaL_register(L, NULL, pkg_funcs);

    return 1;
}
