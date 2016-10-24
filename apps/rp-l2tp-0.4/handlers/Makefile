# Generated automatically from Makefile.in by configure.
# $Id: Makefile.in,v 1.2 2002/09/30 19:45:00 dskoll Exp $
# Makefile for LNS handlers
# LIC: GPL

prefix=/usr/local
exec_prefix=${prefix}
mandir=${prefix}/man
docdir=/usr/local/doc/rp-pppoe-$(VERSION)
install=/usr/bin/install -c
install_dir=/usr/bin/install -c -d
sbindir=${exec_prefix}/sbin

CC=mips-linux-gcc
AR=mips-linux-ar
ARNLIB=mips-linux-ranlib
STRIP=mips-linux-strip

HANDLERS=sync-pppd.a cmd.a

OBJS=cmd.o pty.o sync-pppd.o dstring.o
SRCS=$(OBJS:.o=.c)
CFLAGS=-O2 -I.. -I../libevent -I../../libevent -Wall

all: $(HANDLERS) l2tp-control

l2tp-control: cmd-control.o
	$(CC) -o l2tp-control cmd-control.o

cmd-control.o: cmd-control.c
	$(CC) -c -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

sync-pppd.a:pty.o sync-pppd.o
	rm -f sync-pppd.a
	$(AR) -cq sync-pppd.a pty.o sync-pppd.o

cmd.a:cmd.o dstring.o
	rm -f cmd.a
	$(AR) -cq cmd.a dstring.o cmd.o

clean:
	rm -f *.so *.o *~ *.a
	rm -f l2tp-control

install: all
	-mkdir -p $(RPM_INSTALL_ROOT)$(sbindir)
	-mkdir -p $(RPM_INSTALL_ROOT)$(prefix)/lib/l2tp/plugins
	$(install) -m 755 -s l2tp-control $(RPM_INSTALL_ROOT)$(sbindir)
	$(install) -m 755 $(HANDLERS) $(RPM_INSTALL_ROOT)$(prefix)/lib/l2tp/plugins
distclean: clean
