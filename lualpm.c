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
