/* gcc -W -Wall -pedantic -std=c99 -D_GNU_SOURCE `pkg-config --cflags lua` -fPIC -shared -o utilcore.so utilcore.c */
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <signal.h>
//#include "gettext.h"
#include <locale.h>
#include <libintl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/prctl.h> /* for setprocname */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h> /* isatty, getuid */
#include <limits.h>
#include <wchar.h>
#include <termios.h>

//#define _XOPEN_SOURCE
extern int errno;

typedef struct winsize winsize;
static winsize *push_winsize(lua_State *L)
{
    winsize *box = lua_newuserdata(L, sizeof(winsize));

    if (luaL_newmetatable(L, "clyde_ttysize")) {
        static luaL_Reg const methods[] = {
            { NULL,                     NULL }
        };
        lua_newtable(L);
        luaL_register(L, NULL, methods);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);

    return box;
}
static int clyde_bindtextdomain(lua_State *L)
{
    const char *domainname = luaL_checkstring(L, 1);
    const char *dirname = luaL_checkstring(L, 2);
    const char *result = bindtextdomain(domainname, dirname);
    lua_pushstring(L, result);

    return 1;
}

static int clyde_textdomain(lua_State *L)
{
    const char *domainname = luaL_checkstring(L, 1);
    const char *result = textdomain(domainname);
    lua_pushstring(L, result);

    return 1;
}

static int clyde_gettext(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    const char *result = gettext(str);
    lua_pushstring(L, result);

    return 1;
}

static int clyde_lstat(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    struct stat st;
    const int result = lstat(path, &st);
    lua_pushnumber(L, result);
    lua_pushnumber(L, errno);

    return 2;
}


static int clyde_strerror(lua_State *L)
{
    const int error = lua_tonumber(L, 1);
    const char *result = strerror(error);
    lua_pushstring(L, result);

    return 1;
}

static int clyde_geteuid(lua_State *L)
{
    const int result = geteuid();
    lua_pushnumber(L, result);

    return 1;
}

static int clyde_access(lua_State *L)
{
    const char *slist[] = {
        "R_OK",
        "W_OK",
        "X_OK",
        "F_OK",
        NULL
    };
    const int ilist[] ={
        R_OK,
        W_OK,
        X_OK,
        F_OK
    };

    const char *path = luaL_checkstring(L, 1);
    const int i = luaL_checkoption(L, 2, NULL, slist);
    const int amode = ilist[i];
    const int result = access(path, amode);
    lua_pushnumber(L, result);
    lua_pushnumber(L, errno);

    return 2;
}

static int clyde_isatty(lua_State *L)
{
    const int val = lua_tointeger(L, 1);
    const int result = isatty(val);
    lua_pushnumber(L, result);

    return 1;
}

static int clyde_ioctl(lua_State *L)
{
    const int fildes = lua_tonumber(L, 1);
    const int request = lua_tonumber(L, 2);
    struct winsize *box = push_winsize(L);
    const int result = ioctl(fildes, request, box);
    lua_newtable(L);
    lua_pushstring(L, "ws_col");
    lua_pushnumber(L, box->ws_col);
    lua_settable(L, -3);

    lua_pushstring(L, "ws_row");
    lua_pushnumber(L, box->ws_row);
    lua_settable(L, -3);

    lua_pushstring(L, "ws_xpixel");
    lua_pushnumber(L, box->ws_xpixel);
    lua_settable(L, -3);

    lua_pushstring(L, "ws_ypixel");
    lua_pushnumber(L, box->ws_ypixel);
    lua_settable(L, -3);

    lua_pushnumber(L, result);

    return 2;
}

static int clyde_mkdir(lua_State *L)
{
    char const *path = luaL_checkstring(L, 1);
    mode_t mode = luaL_checknumber(L, 2);
    if (-1 != mkdir(path, mode)) {
        lua_pushnumber(L, 0);

        return 1;
    } else {
        int err = errno;
        lua_pushnil(L);
        lua_pushstring(L, strerror(err));
        lua_pushinteger(L, err);

        return 3;
    }
}

static int clyde_umask(lua_State *L)
{
        mode_t cmask = luaL_checkint(L, 1);
        cmask = umask(cmask);
        lua_pushnumber(L, cmask);
        return 1;
}

static int clyde_arch(lua_State *L)
{
    struct utsname un;
    uname(&un);
    lua_pushstring(L, un.machine);
    return 1;
}

static void throw_errno ( lua_State *L, const char * funcname )
{
    lua_pushfstring( L, "%s: %s", funcname, strerror( errno ));
    lua_error( L );
    return;
}

/* A semicolon is required at the end */
#define CHECK_ERR( FUNCNAME, FUNCCALL ) \
    if ( FUNCCALL == -1 ) throw_errno( L, FUNCNAME )

/* Sets the effective procedure name, to change the terminal's title. */
static int clyde_setprocname ( lua_State *L )
{
    const char *procname;
    
    procname = luaL_checkstring( L, 1 );
    CHECK_ERR( "prctl", prctl( PR_SET_NAME, procname, 0, 0, 0 ));
    
    return 0;
}

#define STDIN 0

/* Save our old termio struct for the signal handler. */
struct termios Old_termio;
struct sigaction Old_signals[2];

/* This is just here to avoid copy/paste code. */
static int restore_termio ( void )
{
    return tcsetattr( STDIN, TCSANOW, &Old_termio );
}

static void sig_restore_termio ( int signal )
{
    int i;

    restore_termio();

    /* Call lua-signal's old signal handler. */
    switch ( signal ) {
    case SIGINT : i = 0; break;
    case SIGTERM: i = 1; break;
    default: return;
    }

    /* Make sure it exists first. */
    if ( ( Old_signals+i )->sa_handler != NULL ) {
        (( Old_signals+i )->sa_handler )( signal );
    }

    return;
}

static int clyde_getchar ( lua_State *L ) {
    struct sigaction new_signal;
    struct termios new_termio;
    char input;

    if ( isatty( STDIN ) == 0 ) {
        /* The rest won't work if we aren't a real terminal. */
        input = getchar();
        lua_pushlstring( L, &input, 1 );
        return 1;
    }

    /* XXX: This sets the terminal, etc, even if it is already
       set to non-canonical mode. Lazy? Who cares? */

    CHECK_ERR( "tcgetattr", tcgetattr( STDIN, &Old_termio ));
    memcpy( &new_termio, &Old_termio, sizeof( struct termios ));

    /* Turn off canonical mode, meaning read one char at a time.
       Set timer to 0, to wait forever. Minimum chars is 1. */
    new_termio.c_lflag      &= ~ICANON;
    new_termio.c_cc[ VTIME ] = 0;
    new_termio.c_cc[ VMIN  ] = 1;
    CHECK_ERR( "tcsetattr", tcsetattr( STDIN, TCSANOW, &new_termio ));

    /* We should confirm it worked, according to the manpage. */
    CHECK_ERR( "tcgetattr", tcgetattr( STDIN, &new_termio ));
    if ( new_termio.c_lflag & ICANON
         || new_termio.c_cc[ VTIME ] != 0
         || new_termio.c_cc[ VMIN  ] != 1 ) {
        lua_pushstring( L, "Failed to set character read mode for terminal" );
        lua_error( L );
    }

    /* Create our own signal handler. Store old signal handler. */
    memset( &new_signal, 0, sizeof( struct sigaction ));
    new_signal.sa_handler = sig_restore_termio;
    CHECK_ERR( "sigaction", sigaction( SIGINT,  &new_signal, Old_signals+0 ));
    CHECK_ERR( "sigaction", sigaction( SIGTERM, &new_signal, Old_signals+1 ));
    /* XXX: Should we handle more signals? */

    input = getchar();
    lua_pushlstring( L, &input, 1 );

    /* Restore old signal handlers and terminal IO setup. */
    CHECK_ERR( "sigaction", sigaction( SIGINT,  Old_signals+0, NULL ));
    CHECK_ERR( "sigaction", sigaction( SIGTERM, Old_signals+1, NULL ));
    CHECK_ERR( "tcsetattr", restore_termio() );

    return 1;
}


static luaL_Reg const pkg_funcs[] = {
    { "bindtextdomain",             clyde_bindtextdomain },
    { "textdomain",                 clyde_textdomain },
    { "gettext",                    clyde_gettext },
    { "lstat",                      clyde_lstat },
    { "strerror",                   clyde_strerror },
    { "geteuid",                    clyde_geteuid },
    { "access",                     clyde_access },
    { "isatty",                     clyde_isatty },
    { "ioctl",                      clyde_ioctl },
    { "mkdir",                      clyde_mkdir },
    { "umask",                      clyde_umask },
    { "arch",                       clyde_arch },
    { "getchar",                    clyde_getchar },
    { "setprocname",                clyde_setprocname },
    { NULL,                         NULL}
};


int luaopen_clydelib_utilcore(lua_State *L)
{
    lua_newtable(L);
    luaL_register(L, NULL, pkg_funcs);

    return 1;
}
