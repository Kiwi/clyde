LDFLAGS = -s
CFLAGS = -Wall -W -O2 -fPIC
CC = gcc
INSTALL = install
INSTALL_DIR = $(INSTALL) -dm755
INSTALL_DATA = $(INSTALL) -Dm644
INSTALL_PROGRAM = $(INSTALL) -Dm755
LUACFLAGS = `pkg-config --cflags lua` -shared -pedantic -std=c99 \
            -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE

prefix = /usr
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
sharedir = $(prefix)/share/lua/5.1
libdir = $(prefix)/lib/lua/5.1
mandir = $(prefix)/share/man
man8dir = $(mandir)/man8
zshcompdir = $(prefix)/share/zsh/site-functions
bashcompdir = /etc/bash_completion.d
manext = .8

all: clyde lualpm

.PHONY: all lualpm clyde install install_lualpm install_clyde \
        clean uninstall

lualpm: lualpm.so

lualpm.so: lualpm.c
	$(CC) $(CFLAGS) $(LUACFLAGS) -lalpm -o lualpm.so lualpm.c

clydelib/signal.so: clydelib/lsignal.c
	$(CC) $(CFLAGS) $(LUACFLAGS) -o clydelib/signal.so clydelib/lsignal.c

clydelib/utilcore.so: clydelib/utilcore.c
	$(CC) $(CFLAGS) $(LUACFLAGS) -o clydelib/utilcore.so clydelib/utilcore.c

clyde: clydelib/signal.so clydelib/utilcore.so

install: install_lualpm install_clyde

install_lualpm: lualpm.so
	$(INSTALL_PROGRAM) lualpm.so $(DESTDIR)$(libdir)/lualpm.so

install_clyde: clyde
	$(INSTALL_PROGRAM) clyde $(DESTDIR)$(bindir)/clyde

	$(INSTALL_DIR) $(DESTDIR)$(sharedir)/clydelib
	$(INSTALL_PROGRAM) clydelib/utilcore.so \
	    $(DESTDIR)$(libdir)/clydelib/utilcore.so
	$(INSTALL_PROGRAM) clydelib/signal.so \
	    $(DESTDIR)$(libdir)/clydelib/signal.so
	$(INSTALL_DATA) clydelib/*.lua $(DESTDIR)$(sharedir)/clydelib/
	$(INSTALL_DATA) man/clyde.8 $(DESTDIR)$(man8dir)/clyde$(manext)

	if test -d $(zshcompdir) ; then
	    $(INSTALL_DATA) extras/_clydezsh $(DESTDIR)$(zshcompdir)/_clyde
	fi
	if test -d $(bashcompdir) ; then
	    $(INSTALL_DATA) extras/clydebash $(DESTDIR)$(bashcompdir)/clyde
	fi

clean:
	-rm -f *.so clydelib/*.so

uninstall:
	rm -f $(DESTDIR)$(libdir)/lualpm.so
	rm -Rf $(DESTDIR)$(sharedir)/clydelib
	rm -f $(DESTDIR)$(bindir)/clyde

	if test -f $(DESTDIR)$(zshcompdir)/_clyde ; then
	    rm -f $(DESTDIR)$(zshcompdir)/_clyde
	fi
	if test -f $(DESTDIR)$(bashcompdir)/clyde ; then
	    rm -f $(DESTDIR)$(bashcompdir)/clyde
	fi
