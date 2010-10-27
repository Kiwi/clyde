#include <alpm.h>
#include <alpm_list.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "types.h"

/* PACKAGE GROUP CLASS */

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

pmgrp_t **push_pmgrp_box(lua_State *L)
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
