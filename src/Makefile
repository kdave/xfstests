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

LINUX_TARGETS = xfsctl bstat t_mtab getdevicesize preallo_rw_pattern_reader \
	preallo_rw_pattern_writer ftrunc trunc fs_perms testx looptest \
	locktest unwritten_mmap bulkstat_unlink_test \
	bulkstat_unlink_test_modified t_dir_offset t_futimens t_immutable

SUBDIRS =

LLDLIBS = $(LIBATTR) $(LIBHANDLE) $(LIBACL)

ifeq ($(HAVE_XLOG_ASSIGN_LSN), true)
LINUX_TARGETS += loggen
endif

IRIX_TARGETS = open_unlink

ifeq ($(PKG_PLATFORM),linux)
TARGETS += $(LINUX_TARGETS)
endif

ifeq ($(PKG_PLATFORM),irix)
TARGETS += $(IRIX_TARGETS)
LLDLIBS += -lgen
endif

ifeq ($(HAVE_DB), true)
TARGETS += dbtest
LLDLIBS += $(LIBGDBM)
endif

ifeq ($(HAVE_AIO), true)
SUBDIRS += aio-dio-regress
endif

CFILES = $(TARGETS:=.c)
LDIRT = $(TARGETS)


default: depend $(TARGETS) $(SUBDIRS)

include $(BUILDRULES)

$(TARGETS): $(LIBTEST)
	@echo "    [CC]    $@"
	$(Q)$(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(LIBTEST)

LINKTEST = $(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS)

install: default $(addsuffix -install,$(SUBDIRS))
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)/src
	$(INSTALL) -m 755 $(TARGETS) $(PKG_LIB_DIR)/src
	$(INSTALL) -m 755 fill2attr fill2fs fill2fs_check scaleread.sh $(PKG_LIB_DIR)/src
	$(INSTALL) -m 644 dumpfile $(PKG_LIB_DIR)/src

%-install:
	$(MAKE) -C $* install

-include .dep
