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

#include "lualpm.h"
#include "types.h"
#include "callback.h"

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
    { "option_set_fetchcb",         lalpm_option_set_fetchcb },
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
