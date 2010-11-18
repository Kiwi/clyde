#include <string.h>
#include <assert.h>
#include <lualib.h>
#include <lauxlib.h>
#include "types.h"

const char * PKGREASON_TOSTR[] = { "P_R_EXPLICIT", "P_R_DEPEND" };

#define Constants(name) constant_t const name ##_constants[] = {
#define EndConstants { NULL, 0 } };

Constants(transflag)
#define Sym(x) { "T_F_" #x, PM_TRANS_FLAG_ ## x }
    Sym(NODEPS),
    Sym(FORCE),
    Sym(NOSAVE),
    Sym(CASCADE),
    Sym(RECURSE),
    Sym(DBONLY),
    Sym(ALLDEPS),
    Sym(DOWNLOADONLY),
    Sym(NOSCRIPTLET),
    Sym(NOCONFLICTS),
    Sym(NEEDED),
    Sym(ALLEXPLICIT),
    Sym(UNNEEDED),
    Sym(RECURSEALL),
    Sym(NOLOCK),
#undef Sym
EndConstants

/* TODO: Can we replace this with luaL_pushstring? */
int
push_string(lua_State *L, char const *s)
{
    if (!s) lua_pushnil(L);
    else lua_pushstring(L, s);
    return 1;
}

int
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

int
raise_last_pm_error(lua_State *L)
{
    if (pm_errno)
        luaL_error(L, "alpm error: %s", alpm_strerrorlast());
    return luaL_error(L, "[BUG] raising an alpm error without pm_errno being set");
}

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

int
push_typed_object(lua_State *L, types value_type, void *value)
{
    if (value == NULL) {
        lua_pushnil(L);
        return 1;
    }
    switch (value_type) {
    case STRING: push_string(L, value); return 1;
    case PMDB_T: push_pmdb(L, value); return 1;
    case PMPKG_T: push_pmpkg(L, value); return 1;
    case PMDELTA_T: push_pmdelta(L, value); return 1;
    case PMGRP_T: push_pmgrp(L, value); return 1;
/*     case PMTRANS_T: push_pmtrans(L, value); return 1; */
    case PMDEPEND_T: push_pmdepend(L, value); return 1;
    case PMDEPMISSING_T: push_pmdepmissing(L, value); return 1;
    case PMCONFLICT_T: push_pmconflict(L, value); return 1;
    case PMFILECONFLICT_T: push_pmfileconflict(L, value); return 1;
    }
    assert(0 && "[BUG] unhandled value type");
    return 0;
}

int
alpm_list_to_any_table(lua_State *L, alpm_list_t *list, enum types value_type)
{
    size_t j = 1;
    lua_newtable(L);
    while (list) {
        void *value = alpm_list_getdata(list);
        if (push_typed_object(L, value_type, value))
            lua_rawseti(L, -2, j++);
        list = alpm_list_next(list);
    }
    return 1;
}

/* CONSTANTS ****************************************************************/

int
push_constant(
    lua_State *L,
    int value,
    constant_t const constants[])
{
    int i;
    for (i=0; constants[i].name; i++) {
        if (constants[i].value == value) {
            push_string(L, constants[i].name);
            return 1;
        }
    }
    lua_pushnumber(L, value);
    return 1;
}

int
check_constant(
    lua_State *L,
    int narg,
    constant_t const constants[],
    char const *extramsg)
{
    int i;

    if (lua_type(L, narg) == LUA_TNUMBER) {
        int find = lua_tonumber(L, narg);
        for (i=0; constants[i].name; i++) {
            if (constants[i].value == find){
                return constants[i].value;
            }
        }
    }
    else if (lua_type(L, narg) == LUA_TSTRING) {
        char const *name = lua_tostring(L, narg);
        for (i=0; constants[i].name; i++) {
            if (0 == strcmp(constants[i].name, name)) {
                return constants[i].value;
            }
        }
    }

    if (narg >= 1)
        luaL_argerror(L, narg, extramsg);
    else
        luaL_error(L, "%s", extramsg);
    assert(0 && "unreachable");
    return -1;
}

pmtransflag_t
check_transflag(lua_State *L, int narg)
{
    return check_constant(L, narg, transflag_constants,
                          "expected a pmtransflag_t");
}

int
push_transflag(lua_State *L, pmtransflag_t f)
{
    return push_constant(L, f, transflag_constants);
}

pmtransflag_t
check_transflags_table(lua_State *L, int narg)
{
    pmtransflag_t flags = 0;
    luaL_checkstack(L, 3, "no space to iterate over transflags");
    lua_pushnil(L);
    while (lua_next(L, narg)) {
        flags |= check_transflag(L, -1);
        lua_pop(L, 1);
    }
    return flags;
}

int
push_transflags_table(lua_State *L, pmtransflag_t flags)
{
    unsigned num_flags = 0;
    unsigned i;
    luaL_checkstack(L, 3, "error making a transflags table");
    lua_newtable(L);
    for (i=0; i < 32; i++) {
        pmtransflag_t flag = 1 << i;
        if (flags & flag) {
            ++num_flags;
            push_transflag(L, flag);
            lua_rawseti(L, -2, num_flags);
        }
    }
    return 1;
}

void *
check_box(lua_State *L, int narg, char const *typename)
{
    void **box = luaL_checkudata(L, narg, typename);
    if (*box == NULL) {
        luaL_error(L, "Empty %s box", typename);
    }
    return *box;
}
