LDFLAGS = -s
CFLAGS = -Wall -W -O2 -fPIC `pkg-config --cflags lua` \
	-std=c99 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
CC = gcc
SOFLAGS = -shared -pedantic -llua

INSTALL = install
INSTALL_DIR = $(INSTALL) -dm755
INSTALL_DATA = $(INSTALL) -Dm644
INSTALL_PROGRAM = $(INSTALL) -Dm755

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

lualpm_objects = lualpm/callback.o lualpm/db.o lualpm/delta.o		\
	lualpm/dep.o lualpm/group.o lualpm/option.o lualpm/package.o	\
	lualpm/sync.o lualpm/trans.o lualpm/types.o lualpm/lualpm.o

all: clyde lualpm

.PHONY: all lualpm clyde install install_lualpm install_clyde \
        clean uninstall uninstall_lualpm uninstall_clyde doc

lualpm/callback.o: lualpm/lualpm.h

lualpm/db.o: lualpm/types.h

lualpm/delta.o: lualpm/types.h

lualpm/dep.o: lualpm/types.h

lualpm/group.o: lualpm/types.h

lualpm/package.o: lualpm/types.h

lualpm/trans.o: lualpm/types.h lualpm/lualpm.h

lualpm/types.o: lualpm/types.h

lualpm.so: $(lualpm_objects)
	$(CC) $(CFLAGS) $(SOFLAGS) -lalpm -o $@ $^

lualpm: lualpm.so

clydelib/signal.so: clydelib/signal.c
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $^

clydelib/utilcore.so: clydelib/utilcore.c
	$(CC) $(CFLAGS) $(SOFLAGS) -o $@ $^

doc: man/clyde.8

man/clyde.8: man/clyde.ronn
	ronn man/clyde.ronn

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
	$(INSTALL_DATA) man/clyde$(manext) $(DESTDIR)$(man8dir)/clyde$(manext)
	$(INSTALL_DATA) extras/_clydezsh $(DESTDIR)$(zshcompdir)/_clyde
	$(INSTALL_DATA) extras/clydebash $(DESTDIR)$(bashcompdir)/clyde

clean:
	-rm -f *.so clydelib/*.so lualpm/*.o

uninstall_lualpm:
	rm -f $(DESTDIR)$(libdir)/lualpm.so

uninstall_clyde:
	rm -Rf $(DESTDIR)$(sharedir)/clydelib
	rm -Rf $(DESTDIR)$(libdir)/clydelib
	rm -f $(DESTDIR)$(bindir)/clyde $(DESTDIR)$(man8dir)/clyde$(manext)
	rm -f $(DESTDIR)$(zshcompdir)/_clyde
	rm -f $(DESTDIR)$(bashcompdir)/clyde

uninstall: uninstall_lualpm uninstall_clyde
