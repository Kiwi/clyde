#include <alpm.h>
#include <alpm_list.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "types.h"
#include "lualpm.h"
#include "callback.h"

#define DEFINE_CB_OPT( NAME ) \
    int lalpm_option_set_ ## NAME ## cb ( lua_State *L )    \
    {                                                       \
    if ( lua_isnil( L, 1 )) {                               \
        alpm_option_set_ ## NAME ## cb( NULL );             \
    }                                                       \
    else {                                                  \
    cb_register( L, &cb_key_ ## NAME );                     \
    alpm_option_set_ ## NAME ## cb( cb_cfunc_ ## NAME );    \
    }                                                       \
    return 0;                                               \
    }

DEFINE_CB_OPT( log )
DEFINE_CB_OPT( dl )
DEFINE_CB_OPT( totaldl )
DEFINE_CB_OPT( fetch )

#undef DEFINE_CB_OPT

/* const char *alpm_option_get_root(); */
int lalpm_option_get_root(lua_State *L)
{
    const char *result = alpm_option_get_root();
    push_string(L, result);

    return 1;
}

/* int alpm_option_set_root(const char *root); */
int lalpm_option_set_root(lua_State *L)
{
    const char *root = luaL_checkstring(L, 1);
    const int result = alpm_option_set_root(root);
    lua_pushnumber(L, result);

    return 1;
}

/* const char *alpm_option_get_dbpath(); */
int lalpm_option_get_dbpath(lua_State *L)
{
    const char *result = alpm_option_get_dbpath();
    push_string(L, result);

    return 1;
}

/* int alpm_option_set_dbpath(const char *dbpath); */
int lalpm_option_set_dbpath(lua_State *L)
{
    const char *dbpath = luaL_checkstring(L, 1);
    const int result = alpm_option_set_dbpath(dbpath);
    lua_pushnumber(L, result);

    return 1;
}

/* alpm_list_t *alpm_option_get_cachedirs(); */
int lalpm_option_get_cachedirs(lua_State *L)
{
    alpm_list_t *list = alpm_option_get_cachedirs();
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* void alpm_option_set_cachedirs(alpm_list_t *cachedirs); */
int lalpm_option_set_cachedirs(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    alpm_list_t *list = lstring_table_to_alpm_list(L, 1);
    alpm_option_set_cachedirs(list);

    return 0;
}

/* int alpm_option_add_cachedir(const char *cachedir); */
int lalpm_option_add_cachedir(lua_State *L)
{
    const char *cachedir = luaL_checkstring(L, 1);
    const int result = alpm_option_add_cachedir(cachedir);
    lua_pushnumber(L, result);

    return 1;
}

/* void alpm_option_set_cachedirs(alpm_list_t *cachedirs); */

/* int alpm_option_remove_cachedir(const char *cachedir); */
int lalpm_option_remove_cachedir(lua_State *L)
{
    const char *cachedir = luaL_checkstring(L, 1);
    const int result = alpm_option_remove_cachedir(cachedir);
    lua_pushnumber(L, result);

    return 1;
}

/* const char *alpm_option_get_logfile(); */
int lalpm_option_get_logfile(lua_State *L)
{
    const char *result = alpm_option_get_logfile();
    push_string(L, result);

    return 1;
}

/* int alpm_option_set_logfile(const char *logfile); */
int lalpm_option_set_logfile(lua_State *L)
{
    const char *logfile = luaL_checkstring(L, 1);
    const int result = alpm_option_set_logfile(logfile);
    lua_pushnumber(L, result);

    return 1;
}

/* const char *alpm_option_get_lockfile(); */
int lalpm_option_get_lockfile(lua_State *L)
{
    const char *result = alpm_option_get_lockfile();
    push_string(L, result);

    return 1;
}

/* no set_lockfile, path is determined from dbpath */

/* int alpm_option_get_usesyslog(); */
int lalpm_option_get_usesyslog(lua_State *L)
{
    int result = alpm_option_get_usesyslog();
    lua_pushnumber(L, result);

    return 1;
}

/* void alpm_option_set_usesyslog(int usesyslog); */
int lalpm_option_set_usesyslog(lua_State *L)
{
    const int usesyslog = lua_toboolean(L, 1);
    alpm_option_set_usesyslog(usesyslog);

    return 0;
}

/* alpm_list_t *alpm_option_get_noupgrades(); */
int lalpm_option_get_noupgrades(lua_State *L)
{
    alpm_list_t *list = alpm_option_get_noupgrades();
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* void alpm_option_add_noupgrade(const char *pkg); */
int lalpm_option_add_noupgrade(lua_State *L)
{
    const char *pkg = luaL_checkstring(L, 1);
    alpm_option_add_noupgrade(pkg);

    return 0;
}

/* void alpm_option_set_noupgrades(alpm_list_t *noupgrade); */
int lalpm_option_set_noupgrades(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    alpm_list_t *list = lstring_table_to_alpm_list(L, 1);
    alpm_option_set_noupgrades(list);

    return 0;
}
/* int alpm_option_remove_noupgrade(const char *pkg); */
int lalpm_option_remove_noupgrade(lua_State *L)
{
    const char *pkg = luaL_checkstring(L, 1);
    const int result = alpm_option_remove_noupgrade(pkg);
    lua_pushnumber(L, result);

    return 1;
}

/* alpm_list_t *alpm_option_get_noextracts(); */
int lalpm_option_get_noextracts(lua_State *L)
{
    alpm_list_t *list = alpm_option_get_noextracts();
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* void alpm_option_add_noextract(const char *pkg); */
int lalpm_option_add_noextract(lua_State *L)
{
    const char *pkg = luaL_checkstring(L, 1);
    alpm_option_add_noextract(pkg);

    return 0;
}

/* void alpm_option_set_noextracts(alpm_list_t *noextract); */
int lalpm_option_set_noextracts(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    alpm_list_t *list = lstring_table_to_alpm_list(L, 1);
    alpm_option_set_noextracts(list);

    return 0;
}

/* int alpm_option_remove_noextract(const char *pkg); */
int lalpm_option_remove_noextract(lua_State *L)
{
    const char *pkg = luaL_checkstring(L, 1);
    const int result = alpm_option_remove_noextract(pkg);
    lua_pushnumber(L, result);

    return 1;
}

/* alpm_list_t *alpm_option_get_ignorepkgs(); */
int lalpm_option_get_ignorepkgs(lua_State *L)
{
    alpm_list_t *list = alpm_option_get_ignorepkgs();
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* void alpm_option_add_ignorepkg(const char *pkg); */
int lalpm_option_add_ignorepkg(lua_State *L)
{
    const char *pkg = luaL_checkstring(L, 1);
    alpm_option_add_ignorepkg(pkg);

    return 0;
}

/* void alpm_option_set_ignorepkgs(alpm_list_t *ignorepkgs); */
int lalpm_option_set_ignorepkgs(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    alpm_list_t *list = lstring_table_to_alpm_list(L, 1);
    alpm_option_set_ignorepkgs(list);

    return 0;
}

/* int alpm_option_remove_ignorepkg(const char *pkg); */
int lalpm_option_remove_ignorepkg(lua_State *L)
{
    const char *pkg = luaL_checkstring(L, 1);
    const int result = alpm_option_remove_ignorepkg(pkg);
    lua_pushnumber(L, result);

    return 1;
}

/* alpm_list_t *alpm_option_get_ignoregrps(); */
int lalpm_option_get_ignoregrps(lua_State *L)
{
    alpm_list_t *list = alpm_option_get_ignoregrps();
    alpm_list_to_any_table(L, list, STRING);

    return 1;
}

/* void alpm_option_add_ignoregrp(const char *grp); */
int lalpm_option_add_ignoregrp(lua_State *L)
{
    const char *grp = luaL_checkstring(L, 1);
    alpm_option_add_ignoregrp(grp);

    return 0;
}

/* void alpm_option_set_ignoregrps(alpm_list_t *ignoregrps); */
int lalpm_option_set_ignoregrps(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    alpm_list_t *list = lstring_table_to_alpm_list(L, 1);
    alpm_option_set_ignoregrps(list);

    return 0;
}

/* int alpm_option_remove_ignoregrp(const char *grp); */
int lalpm_option_remove_ignoregrp(lua_State *L)
{
    const char *grp = luaL_checkstring(L, 1);
    const int result = alpm_option_remove_ignoregrp(grp);
    lua_pushnumber(L, result);

    return 1;
}

/* const char *alpm_option_get_arch(); */
int lalpm_option_get_arch(lua_State *L)
{
    const char *result = alpm_option_get_arch();
    push_string(L, result);

    return 1;
}

/* void alpm_option_set_arch(const char *arch); */
int lalpm_option_set_arch(lua_State *L)
{
    const char *arch = luaL_checkstring(L, 1);
    alpm_option_set_arch(arch);

    return 0;
}

/* void alpm_option_set_usedelta(int usedelta); */
int lalpm_option_set_usedelta(lua_State *L)
{
    const int usedelta = lua_toboolean(L, 1);
    alpm_option_set_usedelta(usedelta);

    return 0;
}

/* pmdb_t *alpm_option_get_localdb(); */
int lalpm_option_get_localdb(lua_State *L)
{
    pmdb_t **box = push_pmdb_box(L);
    *box = alpm_option_get_localdb();
    if (*box == NULL) {
        lua_pushnil(L);
    }

    return 1;
}

/* alpm_list_t *alpm_option_get_syncdbs(); */
int lalpm_option_get_syncdbs(lua_State *L)
{
    alpm_list_t *list = alpm_option_get_syncdbs();
    alpm_list_to_any_table(L, list, PMDB_T);

    return 1;
}

