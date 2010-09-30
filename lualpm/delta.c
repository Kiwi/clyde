#include <alpm.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "types.h"

/* DELTA CLASS */

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

pmdelta_t **push_pmdelta_box(lua_State *L)
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

