LDFLAGS = -s
CFLAGS = -Wall -W -O2 -fPIC
CC = gcc
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
zshcompdir = /usr/share/zsh/site-functions
manext = .8

BIN = clyde

.PHONY: lualpm clyde install_lualpm install_clyde clean uninstall

lualpm:
	$(CC) $(CFLAGS) $(AFLAG) -lalpm `pkg-config --cflags lua` -shared -o lualpm.so lualpm.c -pedantic -D_FILE_OFFSET_BITS=64 -std=c99 -D_GNU_SOURCE

clyde:
	$(CC) $(CFLAGS) $(AFLAG) `pkg-config --cflags lua` -shared -o clydelib/utilcore.so clydelib/utilcore.c -pedantic -D_FILE_OFFSET_BITS=64 -std=c99 -D_GNU_SOURCE
	$(CC) $(CFLAGS) $(AFLAG) -shared -o clydelib/signal.so clydelib/lsignal.c

install_lualpm:
	$(INSTALL_PROGRAM) lualpm.so $(DESTDIR)$(libdir)/lualpm.so
	
install_clyde: lualpm
	$(INSTALL_PROGRAM) clyde $(DESTDIR)$(bindir)/clyde
	$(INSTALL_DIR) $(DESTDIR)$(sharedir)/clydelib
	$(INSTALL_PROGRAM) clydelib/utilcore.so $(DESTDIR)$(libdir)/clydelib/utilcore.so
	$(INSTALL_PROGRAM) clydelib/signal.so $(DESTDIR)$(libdir)/clydelib/signal.so
	$(INSTALL_DATA) clydelib/*.lua $(DESTDIR)$(sharedir)/clydelib/
	$(INSTALL_DATA) man/clyde.8 $(DESTDIR)$(man8dir)/clyde$(manext)
	$(INSTALL_DATA) extras/_clyde $(DESTDIR)$(zshcompdir)/_clyde

clean:
	-rm -f *.so *.o

uninstall:
	rm -f $(DESTDIR)$(libdir)/lualpm.so
	rm -Rf $(DESTDIR)$(sharedir)/clydelib
	rm -f $(DESTDIR)$(bindir)/clyde
