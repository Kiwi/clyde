#include <alpm.h>
#include <alpm_list.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "types.h"

/* PACKAGE CLASS */

/* int alpm_pkg_load(const char *filename, int full, pmpkg_t **pkg); */
/* lua prototype is pkg, ret = alpm.pkg_load(filename, true/false) */
int lalpm_pkg_load(lua_State *L)
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

pmpkg_t **push_pmpkg_box(lua_State *L)
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

changelog *push_changelog_box(lua_State *L)
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
