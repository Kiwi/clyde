/*
 * lsignal.c -- Signal Handler Library for Lua
 *
 * Copyright (C) 2010  Patrick J. Donnelly (batrick@batbytes.com)
 *
 * This software is distributed under the same license as Lua 5.0:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE. 
*/

#define LUA_LIB_NAME      "signal"
#define LUA_LIB_VERSION   1.2
#define LUA_SIGNAL_NAME   "LUA_SIGNAL"

#if !(defined(_POSIX_SOURCE) || defined(sun) || defined(__sun))
  #define INCLUDE_KILL  1
  #define INCLUDE_PAUSE 1
  #define USE_SIGACTION 1
#endif

#include <lua.h>
#include <lauxlib.h>

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

struct lua_signal
{
  const char *name; /* name of the signal */
  const int sig; /* the signal */
};

static const struct lua_signal lua_signals[] = {
  /* ANSI C signals */
#ifdef SIGABRT
  {"SIGABRT", SIGABRT},
#endif
#ifdef SIGFPE
  {"SIGFPE", SIGFPE},
#endif
#ifdef SIGILL
  {"SIGILL", SIGILL},
#endif
#ifdef SIGINT
  {"SIGINT", SIGINT},
#endif
#ifdef SIGSEGV
  {"SIGSEGV", SIGSEGV},
#endif
#ifdef SIGTERM
  {"SIGTERM", SIGTERM},
#endif
  /* posix signals */
#ifdef SIGHUP
  {"SIGHUP", SIGHUP},
#endif
#ifdef SIGQUIT
  {"SIGQUIT", SIGQUIT},
#endif
#ifdef SIGTRAP
  {"SIGTRAP", SIGTRAP},
#endif
#ifdef SIGKILL
  {"SIGKILL", SIGKILL},
#endif
#ifdef SIGUSR1
  {"SIGUSR1", SIGUSR1},
#endif
#ifdef SIGUSR2
  {"SIGUSR2", SIGUSR2},
#endif
#ifdef SIGPIPE
  {"SIGPIPE", SIGPIPE},
#endif
#ifdef SIGALRM
  {"SIGALRM", SIGALRM},
#endif
#ifdef SIGCHLD
  {"SIGCHLD", SIGCHLD},
#endif
#ifdef SIGCONT
  {"SIGCONT", SIGCONT},
#endif
#ifdef SIGSTOP
  {"SIGSTOP", SIGSTOP},
#endif
#ifdef SIGTTIN
  {"SIGTTIN", SIGTTIN},
#endif
#ifdef SIGTTOU
  {"SIGTTOU", SIGTTOU},
#endif
  /* some BSD signals */
#ifdef SIGIOT
  {"SIGIOT", SIGIOT},
#endif
#ifdef SIGBUS
  {"SIGBUS", SIGBUS},
#endif
#ifdef SIGCLD
  {"SIGCLD", SIGCLD},
#endif
#ifdef SIGURG
  {"SIGURG", SIGURG},
#endif
#ifdef SIGXCPU
  {"SIGXCPU", SIGXCPU},
#endif
#ifdef SIGXFSZ
  {"SIGXFSZ", SIGXFSZ},
#endif
#ifdef SIGVTALRM
  {"SIGVTALRM", SIGVTALRM},
#endif
#ifdef SIGPROF
  {"SIGPROF", SIGPROF},
#endif
#ifdef SIGWINCH
  {"SIGWINCH", SIGWINCH},
#endif
#ifdef SIGPOLL
  {"SIGPOLL", SIGPOLL},
#endif
#ifdef SIGIO
  {"SIGIO", SIGIO},
#endif
  /* add odd signals */
#ifdef SIGSTKFLT
  {"SIGSTKFLT", SIGSTKFLT}, /* stack fault */
#endif
#ifdef SIGSYS
  {"SIGSYS", SIGSYS},
#endif
  {NULL, 0}
};

/*
 *  The signal counts in the 1st half of the array are modified by
 *  the handler.  The corresponding signal counts in the 2nd half
 *  are modifed by the hook routine.
 */
static volatile sig_atomic_t *signal_stack = NULL;
static int signal_stack_top;
static lua_State *ML = NULL;
static struct hook {
  lua_Hook hook;
  int mask;
  int count;
} old_hook = {NULL, 0, 0};

static void hook (lua_State *L, lua_Debug *ar)
{
  int i, j;
  assert(L == ML);
  for (i = 0; i < signal_stack_top; i++)
    while (signal_stack[i] != signal_stack[i+signal_stack_top])
    {
      lua_getfield(L, LUA_REGISTRYINDEX, LUA_SIGNAL_NAME);
      lua_pushinteger(L, i);
      lua_rawget(L, -2);
      lua_replace(L, -2); /* replace _R.LUA_SIGNAL_NAME */
      assert(lua_isfunction(L, -1));
      for (j = 0; lua_signals[j].name != NULL; j++)
        if (lua_signals[j].sig == i)
        {
          lua_pushstring(L, lua_signals[j].name);
          break;
        }
      if (lua_signals[j].name == NULL) lua_pushliteral(L, "");
      lua_pushinteger(L, i);
      lua_call(L, 2, 0);
      signal_stack[i+signal_stack_top]++;
    }
  lua_sethook(ML, old_hook.hook, old_hook.mask, old_hook.count);
  old_hook.hook = NULL;
}

static void handle (int sig)
{
  assert(ML != NULL);
  if (old_hook.hook == NULL) /* replace it */
  {
    old_hook.hook = lua_gethook(ML);
    old_hook.mask = lua_gethookmask(ML);
    old_hook.count = lua_gethookcount(ML);
    lua_sethook(ML, hook, LUA_MASKCOUNT, 1);
  }
  signal_stack[sig]++;
}

static int get_signal (lua_State *L, int idx)
{
  switch (lua_type(L, idx))
  {
    case LUA_TNUMBER:
      return (int) lua_tointeger(L, idx);
    case LUA_TSTRING:
      lua_pushvalue(L, idx);
      lua_rawget(L, LUA_ENVIRONINDEX);
      if (!lua_isnumber(L, -1))
        return luaL_argerror(L, idx, "invalid signal string");
      lua_replace(L, idx);
      return (int) lua_tointeger(L, idx);
    default:
      return luaL_argerror(L, idx, "expected signal string/number");
  }
}

static int status (lua_State *L, int s)
{
  if (s)
  {
    lua_pushboolean(L, 1);
    return 1;
  }
  else
  {
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));
    return 2;
  }
}

/*
 * old_handler[, err] == signal(signal [, func])
 *
 * signal = signal number or string
 * func/"ignore"/"default" = Lua function to call
*/  
static int l_signal (lua_State *L)
{
  enum {IGNORE, DEFAULT, SET};
  static const char *options[] = {"ignore", "default", NULL};
  int sig = get_signal(L, 1);
  int option;

  if (lua_isstring(L, 2))
    option = luaL_checkoption(L, 2, NULL, options);
  else if (lua_isnil(L, 2))
    option = DEFAULT;
  else
    option = (luaL_checktype(L, 2, LUA_TFUNCTION), SET);

  lua_pushvalue(L, 1);
  lua_rawget(L, LUA_ENVIRONINDEX); /* return old handler */

  lua_pushvalue(L, 1);
  switch (option)
  {
    case IGNORE:
      lua_pushnil(L);
      lua_rawset(L, LUA_ENVIRONINDEX);
      signal(sig, SIG_IGN);
      signal_stack[sig+signal_stack_top] = signal_stack[sig] = 0;
      break;
    case DEFAULT:
      lua_pushnil(L);
      lua_rawset(L, LUA_ENVIRONINDEX);
      signal(sig, SIG_DFL);
      signal_stack[sig+signal_stack_top] = signal_stack[sig] = 0;
      break;
    case SET:
      lua_pushvalue(L, 2);
      lua_rawset(L, LUA_ENVIRONINDEX);

#if USE_SIGACTION
      {
        struct sigaction act;
        act.sa_handler = handle;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        if (sigaction(sig, &act, NULL))
          return status(L, 0);
      }
#else
      if (signal(sig, handle) == SIG_ERR)
        return status(L, 0);
#endif
      break;
    default: assert(0);
  }

  return 1;
}

/*
 * status, err = raise(signal)
 *
 * signal = signal number or string
*/  
static int l_raise (lua_State *L)
{
  return status(L, raise(get_signal(L, 1)) == 0);
}

#if INCLUDE_KILL

/* define some posix only functions */

/*
 * status, err = kill(pid, signal)
 *
 * pid = process id
 * signal = signal number or string
*/  
static int l_kill (lua_State *L)
{
  return status(L, kill(luaL_checkinteger(L, 1), get_signal(L, 2)) == 0);
}

#endif

#if INCLUDE_PAUSE

static int l_pause (lua_State *L) /* race condition free */
{
  sigset_t mask, old_mask;
  if (sigfillset(&mask) == -1) return status(L, 0);
  if (sigprocmask(SIG_BLOCK, &mask, &old_mask) == -1) return status(L, 0);
  if (sigsuspend(&old_mask) != -1) abort(); /* that's strange */
  return status(L, 0);
}

#endif

static int interrupted (lua_State *L)
{
  return luaL_error(L, "interrupted!");
}

static int library_gc (lua_State *L)
{
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_SIGNAL_NAME);
  lua_pushnil(L);
  while (lua_next(L, -2))
  {
    if (lua_isnumber(L, -2)) /* <signal, function> */
      signal((int) lua_tointeger(L, -2), SIG_DFL);
    lua_pop(L, 1); /* value */
  }
  signal_stack = NULL;
  ML = NULL;
  old_hook.hook = NULL;
  signal_stack_top = 0;
  return 0;
}

int luaopen_signal (lua_State *L)
{
  static const struct luaL_Reg lib[] = {
    {"signal", l_signal},
    {"raise", l_raise},
#if INCLUDE_KILL
    {"kill", l_kill},
#endif
#if INCLUDE_PAUSE
    {"pause", l_pause},
#endif
    {NULL, NULL}
  };

  int i;
  int max_signal;

  ML = L;
  if (lua_pushthread(L))
    lua_pop(L, 1);
  else
    luaL_error(L, "library should be opened by the main thread");

  /* environment */
  lua_newtable(L);
  lua_replace(L, LUA_ENVIRONINDEX);
  lua_pushvalue(L, LUA_ENVIRONINDEX);
  lua_setfield(L, LUA_REGISTRYINDEX, LUA_SIGNAL_NAME); /* for hooks */

  /* add the library */
  luaL_register(L, LUA_LIB_NAME, lib);
  lua_pushnumber(L, LUA_LIB_VERSION);
  lua_setfield(L, -2, "version");

  for (i = 0, max_signal = 0; lua_signals[i].name != NULL; i++)
    if (lua_signals[i].sig > max_signal)
      max_signal = lua_signals[i].sig+1; /* +1 !!! (for < loops) */

  signal_stack = lua_newuserdata(L, sizeof(volatile sig_atomic_t)*max_signal*2);
  lua_newtable(L);
  lua_pushcfunction(L, library_gc);
  lua_setfield(L, -2, "__gc");
  lua_setmetatable(L, -2); /* when userdata is gc'd, close library */
  memset((void *) signal_stack, 0, sizeof(volatile sig_atomic_t)*max_signal*2);
  signal_stack_top = max_signal;
  lua_pushboolean(L, 1);
  lua_rawset(L, LUA_ENVIRONINDEX);

  while (i--) /* i set from previous for loop */
  {
    lua_pushstring(L, lua_signals[i].name);
    lua_pushinteger(L, lua_signals[i].sig);
    lua_rawset(L, LUA_ENVIRONINDEX); /* add copy to environment table */
    lua_pushstring(L, lua_signals[i].name);
    lua_pushinteger(L, lua_signals[i].sig);
    lua_settable(L, -3); /* add copy to signal table */
  }

  /* set default interrupt handler */
  lua_getfield(L, -1, "signal");
  lua_pushinteger(L, SIGINT);
  lua_pushcfunction(L, interrupted);
  lua_call(L, 2, 0);

  return 1;
}

/* Embed signal into clydelib: clydelib.signal */
int luaopen_clydelib_signal (lua_State *L) {
    return luaopen_signal(L);
}

/* TODO: We should package lua-signal separately and update clyde's
   PKGBUILD -JD */
