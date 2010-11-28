#include <stdio.h>
#include <assert.h>
#include <lua.h>
#include "callback.h"

/* A global is required for alpm C -> Lua gateways to get their Lua
 * goodness from.  Unfortunately alpm callbacks have no userdata or
 * other pointer which the client could use to send their own
 * parameters in. */

/* This can probably be fixed by using lua's c-closures but maybe later...
   -JD */

lua_State *GlobalState;

void
cb_log_error ( const char *cbname, const char *message )
{
    /* TODO: send the error message to the libalpm logger here? */
    fprintf(stderr, "lualpm (%s callback): %s\n", cbname, message);
}

void
cb_register ( lua_State *L, callback_key_t *key )
{
    int argtype;
    argtype = lua_type( L, -1 );
    if ( argtype != LUA_TFUNCTION && argtype != LUA_TNIL ) {
        lua_pushstring( L, "Only nil or a function can be registered "
                           "as a callback" );
        lua_error( L );
    }
    lua_pushlightuserdata( L, key );
    lua_pushvalue( L, -2 );
    lua_settable( L, LUA_REGISTRYINDEX );

    /* Pop our original argument off of the stack. */
    lua_pop( L, 1 );
    return;
}

int
cb_lookup ( callback_key_t *key )
{
    lua_pushlightuserdata( GlobalState, key );
    lua_gettable( GlobalState, LUA_REGISTRYINDEX );

    if ( lua_isnil( GlobalState, -1 )) {
        /* cb_log_error( key->name, "Value for callback is nil!" ); */
        lua_pop( GlobalState, 1 );
        return 0;
    }
    else if ( lua_type( GlobalState, -1 ) != LUA_TFUNCTION ) {
        cb_log_error( key->name, "Value for callback is not a function!" );
        lua_pop( GlobalState, 1 );
        return 0;
    }

    return 1;
}

void
cb_error_handler ( const char *cbname, int err )
{
    switch (err) {
    case 0:                     /* success */
        break;
    case LUA_ERRMEM:
        cb_log_error( cbname, "ran out of memory running Lua" );
        break;
    case LUA_ERRERR:
    case LUA_ERRRUN:
        if (lua_type(GlobalState, -1) == LUA_TSTRING) {
            const char *msg = lua_tostring(GlobalState, -1);
            cb_log_error( cbname, msg );
        }
        else {
            cb_log_error( cbname,
                          "error running Lua "
                          "(received a non-string error object)" );
        }
        break;
    default:
        cb_log_error( cbname, "unknown error while running Lua" );
    }
}

/****************************************************************************/

/* Use these macros to define a callback.
   The macros create a key variable called "cb_key_$NAME"
   A C function that can be passed as a function pointer
   is created, named "cb_cfunc_$NAME".
*/ 

#define BEGIN_CALLBACK( NAME, ... ) \
    callback_key_t cb_key_ ## NAME = { #NAME " callback" }; \
                                                            \
    void cb_cfunc_ ## NAME ( __VA_ARGS__ )                  \
    {                                                       \
    lua_State *L = GlobalState;                             \
    if ( ! cb_lookup( &cb_key_ ## NAME )) { return; }

#define END_CALLBACK( NAME, ARGCOUNT )                      \
    int lua_err = lua_pcall( L, ARGCOUNT, 0, 0 );           \
    if ( lua_err != 0 ) {                                   \
        cb_error_handler( #NAME, lua_err );                 \
    }                                                       \
    return;                                                 \
    } /* end of cb_cfunc */

BEGIN_CALLBACK( log, pmloglevel_t level, char *fmt, va_list vargs )
{
    char * fmted = NULL;
    int err      = vasprintf( &fmted, fmt, vargs );
    assert( err != -1 && "[BUG] vasprintf ran out of memory" );
    
    lua_pushstring( L, fmted );
    push_loglevel( L, level );
}
END_CALLBACK( log, 2 )

BEGIN_CALLBACK( dl, const char *filename, off_t xfered, off_t total )
{
    lua_pushstring( L, filename );
    lua_pushnumber( L, xfered );
    lua_pushnumber( L, total );
}
END_CALLBACK( dl, 3 )    

BEGIN_CALLBACK( totaldl, off_t total )
{
    lua_pushnumber( L, total );
}
END_CALLBACK( totaldl, 1 )

#undef BEGIN_CALLBACK
#undef END_CALLBACK

/* The fetch callback is tricker because it returns -1 if an error
   occurs. */
callback_key_t cb_key_fetch = { "fetch callback" };

int cb_cfunc_fetch ( const char *url, const char *localpath, int force )
{
    lua_State *L = GlobalState;

    if ( cb_lookup( &cb_key_fetch ) == 0 ) { return -1; }

    lua_pushstring( L, url );
    lua_pushstring( L, localpath );
    lua_pushboolean( L, force );
    int lua_err = lua_pcall( L, 3, 1, 0 );
    if ( lua_err != 0 ) {
        cb_error_handler( "fetch", lua_err );
        return -1;
    }

    if ( lua_isnil( L, 1 ) || !lua_isnumber( L, 1 )) { return 1; }
    return lua_tointeger( L, 1 );
}

/****************************************************************************/
/* TRANSACTION CALLBACKS                                                    */

#define BEGIN_TRANS_CALLBACK( NAME, ... ) \
    callback_key_t transcb_key_ ## NAME = { #NAME " callback" }; \
                                                                 \
    void transcb_cfunc_ ## NAME ( __VA_ARGS__ )                  \
    {                                                            \
    lua_State *L = GlobalState;                                  \
                                                                 \
    if ( ! cb_lookup( &transcb_key_ ## NAME )) { return; }       \
    lua_newtable( L );

#define END_TRANS_CALLBACK( NAME )                  \
    int lua_err = lua_pcall( L, 1, 0, 0 );          \
    if ( lua_err != 0 ) {                           \
        cb_error_handler( #NAME, lua_err );         \
    }                                               \
    return;                                         \
    } /* end of transcb_cfunc */

/* TRANSACTION EVENT CALLBACK ***********************************************/

#define EVT_TEXT( KEY, STR )                    \
    lua_pushstring( L, STR );                   \
    lua_setfield( L, -2, KEY );

#define EVT_NAME( NAME )     EVT_TEXT( "name", NAME )
#define EVT_STATUS( STATUS ) EVT_TEXT( "status", STATUS )

#define EVT_PKG( KEY, PKG )                     \
    *( push_pmpkg_box( L ) ) = (pmpkg_t *)PKG;  \
    lua_setfield( L, -2, KEY );

BEGIN_TRANS_CALLBACK( event, pmtransevt_t event, void *arg_one, void *arg_two )
{
    switch ( event ) {
    case PM_TRANS_EVT_CHECKDEPS_START:
        EVT_NAME("checkdeps")
        EVT_STATUS("start")
        break;
    case PM_TRANS_EVT_CHECKDEPS_DONE:
        EVT_NAME("checkdeps")
        EVT_STATUS("done")
        break;
    case PM_TRANS_EVT_FILECONFLICTS_START:
        EVT_NAME("fileconflicts")
        EVT_STATUS("start")
        break;
	case PM_TRANS_EVT_FILECONFLICTS_DONE:
        EVT_NAME("fileconflicts")
        EVT_STATUS("done")
        break;
	case PM_TRANS_EVT_RESOLVEDEPS_START:
        EVT_NAME("resolvedeps")
        EVT_STATUS("start")
        break;
	case PM_TRANS_EVT_RESOLVEDEPS_DONE:
        EVT_NAME("resolvedeps")
        EVT_STATUS("done")
        break;
	case PM_TRANS_EVT_INTERCONFLICTS_START:
        EVT_NAME("interconflicts")
        EVT_STATUS("start")
        break;
	case PM_TRANS_EVT_INTERCONFLICTS_DONE:
        EVT_NAME("interconflicts")
        EVT_STATUS("done")
        EVT_PKG("target", arg_one)
        break;
	case PM_TRANS_EVT_ADD_START:
        EVT_NAME("add")
        EVT_STATUS("start")
        EVT_PKG("package", arg_one)
        break;
	case PM_TRANS_EVT_ADD_DONE:
        EVT_NAME("add")
        EVT_STATUS("done")
        EVT_PKG("package", arg_one)
        break;
	case PM_TRANS_EVT_REMOVE_START:
        EVT_NAME("remove")
        EVT_STATUS("start")
        EVT_PKG("package", arg_one)
		break;
	case PM_TRANS_EVT_REMOVE_DONE:
        EVT_NAME("remove")
        EVT_STATUS("done")
        EVT_PKG("package", arg_one)
		break;
	case PM_TRANS_EVT_UPGRADE_START:
        EVT_NAME("upgrade")
        EVT_STATUS("start")
        EVT_PKG("package", arg_one)
		break;
	case PM_TRANS_EVT_UPGRADE_DONE:
        EVT_NAME("upgrade")
        EVT_STATUS("done")
        EVT_PKG("new", arg_one)
        EVT_PKG("old", arg_two)
		break;
	case PM_TRANS_EVT_INTEGRITY_START:
        EVT_NAME("integrity")
        EVT_STATUS("start")
		break;
	case PM_TRANS_EVT_INTEGRITY_DONE:
        EVT_NAME("integrity")
        EVT_STATUS("done")
		break;
	case PM_TRANS_EVT_DELTA_INTEGRITY_START:
        EVT_NAME("delta_integrity")
        EVT_STATUS("start")
		break;
	case PM_TRANS_EVT_DELTA_INTEGRITY_DONE:
        EVT_NAME("delta_integrity")
        EVT_STATUS("done")
		break;
	case PM_TRANS_EVT_DELTA_PATCHES_START:
        EVT_NAME("delta_patches")
        EVT_STATUS("start")
		break;
	case PM_TRANS_EVT_DELTA_PATCHES_DONE:
        EVT_NAME("delta_patches")
        EVT_STATUS("done")
        EVT_TEXT("pkgname", arg_one)
        EVT_TEXT("patch", arg_two)
		break;
	case PM_TRANS_EVT_DELTA_PATCH_START:
        EVT_NAME("delta_patch")
        EVT_STATUS("start")
        EVT_TEXT("pkgname", arg_one)
        EVT_TEXT("patch", arg_two)
		break;
	case PM_TRANS_EVT_DELTA_PATCH_DONE:
        EVT_NAME("delta_patch")
        EVT_STATUS("done")
		break;
	case PM_TRANS_EVT_DELTA_PATCH_FAILED:
        EVT_NAME("delta_patch")
        EVT_STATUS("failed")
        EVT_TEXT("error", arg_one)
		break;
	case PM_TRANS_EVT_SCRIPTLET_INFO:
        EVT_NAME("scriptlet")
        EVT_STATUS("info")
        EVT_TEXT("text", arg_one)
		break;
    case PM_TRANS_EVT_RETRIEVE_START:
        EVT_NAME("retrieve")
        EVT_STATUS("start")
        EVT_TEXT("db", arg_one)
        break;        
    default:
        return;
    }
}
END_TRANS_CALLBACK( event )
    
#define EVT_PKGLIST( NAME, LIST ) \
    alpm_list_to_any_table( L, LIST, PMPKG_T ); \
    lua_setfield( L, -2, NAME );

BEGIN_TRANS_CALLBACK( conv, pmtransconv_t type,
                      void *arg_one, void *arg_two, void *arg_three,
                      int *response )
{
    switch ( type ) {
    case PM_TRANS_CONV_INSTALL_IGNOREPKG:
        EVT_NAME( "install_ignore" )
        EVT_PKG ( "package", arg_one )
        break;
    case PM_TRANS_CONV_REPLACE_PKG:
        EVT_NAME( "replace_package" )
        EVT_PKG ( "old", arg_one )
        EVT_PKG ( "new", arg_two )
        EVT_TEXT( "db",  arg_three  )
        break;
    case PM_TRANS_CONV_CONFLICT_PKG:
        EVT_NAME( "package_conflict" )
        EVT_TEXT( "target",   arg_one )
        EVT_TEXT( "local",    arg_two )
        EVT_TEXT( "conflict", arg_three )
        break;
    case PM_TRANS_CONV_REMOVE_PKGS:
        EVT_NAME   ( "remove_packages" )
        EVT_PKGLIST( "packages", arg_one )
        break;
    case PM_TRANS_CONV_LOCAL_NEWER:
        EVT_NAME( "local_newer"      )
        EVT_PKG ( "package", arg_one )
        break;
    case PM_TRANS_CONV_CORRUPTED_PKG:
        EVT_NAME( "corrupted_file" )
        EVT_TEXT( "filename", arg_one )
        break;
    default:
        return;
    }
    int lua_err = lua_pcall( L, 1, 1, 0 );
    if ( lua_err != 0 ) {
        cb_error_handler( "conversation", lua_err );
    }
}

/* We cannot use the generic transaction callback ending. */
*response = lua_toboolean( L, -1 );
return;
}

#define EVT_INT( KEY, INT )                     \
    lua_pushnumber( L, INT );                   \
    lua_setfield( L, -2, KEY );

callback_key_t transcb_key_progress = { "progress callback" };
                                                                 \
void transcb_cfunc_progress ( pmtransprog_t type,
                              const char * desc,
                              int item_progress,
                              int total_count,
                              int total_pos )
{
    char * name;
    int lua_error;
    lua_State * L;

    L = GlobalState;

    if ( ! cb_lookup( &transcb_key_progress )) { return; }

    switch( type ) {
    case PM_TRANS_PROGRESS_ADD_START:       name = "add";       break;
    case PM_TRANS_PROGRESS_UPGRADE_START:   name = "upgrade";   break;
    case PM_TRANS_PROGRESS_REMOVE_START:    name = "remove";    break;
    case PM_TRANS_PROGRESS_CONFLICTS_START: name = "conflicts"; break;
    default:                                name = "UNKNOWN";   break;
    }

    lua_pushstring(  L, name );
    lua_pushstring(  L, desc );
    lua_pushinteger( L, item_progress );
    lua_pushinteger( L, total_count );
    lua_pushinteger( L, total_pos );

    lua_error = lua_pcall( L, 5, 1, 0 );
    if ( lua_error != 0 ) {
        cb_error_handler( "progress", lua_error );
    }

    return;
}

#undef EVT_INT
#undef EVT_NAME
#undef EVT_STATUS
#undef EVT_PKG
#undef EVT_PKGLIST
#undef EVT_TEXT

#undef BEGIN_TRANS_CALLBACK
#undef END_TRANS_CALLBACK
