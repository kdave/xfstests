#
# Copyright (c) 2000-2005 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

TARGETS = dirstress fill fill2 getpagesize holes lstat64 \
	nametest permname randholes runas truncfile usemem \
	mmapcat append_reader append_writer dirperf metaperf \
	devzero feature alloc fault fstest t_access_root \
	godown resvtest writemod makeextents itrash rename \
	multi_open_unlink dmiperf unwritten_sync genhashnames

LINUX_TARGETS = xfsctl bstat t_mtab getdevicesize \
	preallo_rw_pattern_reader preallo_rw_pattern_writer ftrunc trunc \
	fs_perms testx looptest locktest unwritten_mmap \
	bulkstat_unlink_test bulkstat_unlink_test_modified t_dir_offset

ifeq ($(HAVE_XLOG_ASSIGN_LSN), true)
LINUX_TARGETS += loggen
endif

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

ifeq ($(HAVE_AIO), true)
SUBDIRS += aio-dio-regress
endif

CFILES = $(TARGETS:=.c)
LDIRT = $(TARGETS)

default: $(TARGETS) $(SUBDIRS)

include $(BUILDRULES)
LINKTEST = $(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS)

randholes: randholes.o $(LIBTEST)
	$(LINKTEST) $(LIBTEST) $(LDLIBS)

truncfile: truncfile.o $(LIBTEST)
	$(LINKTEST) $(LIBTEST) $(LDLIBS)

dbtest:	dbtest.o $(LIBTEST)
	$(LINKTEST) $(LIBTEST) $(LIBGDBM) $(LDLIBS)

genhashnames: genhashnames.o
	$(LINKTEST)

nametest: nametest.o $(LIBTEST)
	$(LINKTEST) $(LIBTEST) $(LDLIBS)

bstat: bstat.o
	$(LINKTEST) $(LIBHANDLE) $(LDLIBS)

t_immutable: t_immutable.o
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
	$(LINKTEST) $(LIBATTR) $(LDLIBS)

#scaleread: scaleread.o $(LDLIBS)
#	$(LINKTEST)

acl_get: acl_get.o
	$(LINKTEST) $(LIBACL) $(LIBATTR) $(LDLIBS)

dmiperf: dmiperf.o
	$(LINKTEST) $(LIBATTR) $(LDLIBS)

preallo_rw_pattern_reader:
	$(CC) $(GCFLAGS) $(LDFLAGS) -DREAD iopat.c -o preallo_rw_pattern_reader

preallo_rw_pattern_writer:
	$(CC) $(GCFLAGS) $(LDFLAGS) -DWRITE iopat.c -o preallo_rw_pattern_writer

ftrunc: ftrunc.o
	$(LINKTEST)

trunc: trunc.o
	$(LINKTEST)

fs_perms: fs_perms.o
	$(LINKTEST)

testx: testx.o
	$(LINKTEST)

looptest: looptest.o
	$(LINKTEST)

locktest: locktest.o
	$(LINKTEST)

unwritten_sync: unwritten_sync.o
	$(LINKTEST)

ifeq ($(PKG_PLATFORM),irix)
fill2: fill2.o
	$(LINKTEST)  -lgen

runas: runas.o
	$(LINKTEST)  -lgen

open_unlink: open_unlink.o $(LIBHANDLE)
	$(LINKTEST) $(LIBHANDLE) $(LDLIBS)

endif

install:
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)/src
	$(INSTALL) -m 755 $(TARGETS) $(PKG_LIB_DIR)/src
	$(INSTALL) -m 755 fill2attr fill2fs fill2fs_check scaleread.sh $(PKG_LIB_DIR)/src
	$(INSTALL) -m 644 dumpfile $(PKG_LIB_DIR)/src
