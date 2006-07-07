#
# Copyright (c) 2000-2005 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

TARGETS = dirstress fill fill2 getpagesize holes lstat64 \
	nametest permname randholes runas truncfile usemem \
	mmapcat append_reader append_writer dirperf metaperf \
	devzero feature alloc fault fstest t_access_root \
	godown resvtest writemod makeextents itrash \
	multi_open_unlink dmiperf

LINUX_TARGETS = loggen xfsctl bstat t_mtab getdevicesize

IRIX_TARGETS = open_unlink

ifeq ($(PKG_PLATFORM),linux)
TARGETS += $(LINUX_TARGETS)
endif

ifeq ($(PKG_PLATFORM),irix)
TARGETS += $(IRIX_TARGETS)
endif

ifeq ($(HAVE_DB), true)
TARGETS += dbtest
endif

ifeq ($(PKG_PLATFORM),linux)
TARGETS += t_immutable
endif

CFILES = $(TARGETS:=.c)
LDIRT = $(TARGETS)

default: $(TARGETS)

include $(BUILDRULES)
LINKTEST = $(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS)

randholes: randholes.o $(LIBTEST)
	$(LINKTEST) $(LIBTEST) $(LDLIBS)

truncfile: truncfile.o $(LIBTEST)
	$(LINKTEST) $(LIBTEST) $(LDLIBS)

dbtest:	dbtest.o $(LIBTEST)
	$(LINKTEST) $(LIBTEST) $(LIBGDBM) $(LDLIBS)

nametest: nametest.o $(LIBTEST)
	$(LINKTEST) $(LIBTEST) $(LDLIBS)

bstat: bstat.o $(LIBHANDLE)
	$(LINKTEST) $(LIBHANDLE) $(LDLIBS)

t_immutable: t_immutable.o $(LIBHANDLE) $(LIBACL)
	$(LINKTEST) $(LIBACL) $(LIBHANDLE) $(LDLIBS)

loggen:	loggen.o
	$(LINKTEST) $(LDLIBS)

fstest:	fstest.o
	$(LINKTEST)

resvtest: resvtest.o
	$(LINKTEST)

itrash:	itrash.o
	$(LINKTEST)

multi_open_unlink: multi_open_unlink.o
	$(LINKTEST)

#scaleread: scaleread.o $(LDLIBS)
#	$(LINKTEST)

acl_get: acl_get.o $(LIBACL) $(LIBATTR)
	$(LINKTEST) $(LIBACL) $(LIBATTR) $(LDLIBS)

dmiperf: dmiperf.o $(LIBATTR)
	$(LINKTEST) $(LIBATTR) $(LDLIBS)


ifeq ($(PKG_PLATFORM),irix)
fill2: fill2.o
	$(LINKTEST)  -lgen

runas: runas.o
	$(LINKTEST)  -lgen

open_unlink: open_unlink.o $(LIBHANDLE)
	$(LINKTEST) $(LIBHANDLE) $(LDLIBS)

endif
