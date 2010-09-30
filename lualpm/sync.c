#include <alpm.h>
#include <alpm_list.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* Functions that modify a transaction to add or remove packages
   are located here. */

/* int alpm_sync_sysupgrade(int enable_downgrade); */
int lalpm_sync_sysupgrade(lua_State *L)
{
    const int enable_downgrade = lua_tointeger(L, 1);
    const int result = alpm_sync_sysupgrade(enable_downgrade);
    lua_pushnumber(L, result);

    return 1;
}

/* int alpm_sync_target(char *target); */
int lalpm_sync_target(lua_State *L)
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
int lalpm_sync_dbtarget(lua_State *L)
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
int lalpm_add_target(lua_State *L)
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
int lalpm_remove_target(lua_State *L)
{
    const char *pkgname;
    int result;

    pkgname = luaL_checkstring(L, 1);
    result  = alpm_remove_target((char *)pkgname);

    lua_pushinteger(L, result);
    return 1;
}    

