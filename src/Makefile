# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2000-2005 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

TARGETS = dirstress fill fill2 getpagesize holes lstat64 \
	nametest permname randholes runas truncfile usemem \
	mmapcat append_reader append_writer dirperf metaperf \
	devzero feature alloc fault fstest t_access_root \
	godown resvtest writemod writev_on_pagefault makeextents itrash rename \
	multi_open_unlink unwritten_sync genhashnames t_holes \
	t_mmap_writev t_truncate_cmtime dirhash_collide t_rename_overwrite \
	holetest t_truncate_self af_unix t_mmap_stale_pmd \
	t_mmap_cow_race t_mmap_fallocate fsync-err t_mmap_write_ro \
	t_ext4_dax_journal_corruption t_ext4_dax_inline_corruption \
	t_ofd_locks t_mmap_collision mmap-write-concurrent \
	t_get_file_time t_create_short_dirs t_create_long_dirs t_enospc \
	t_mmap_writev_overlap checkpoint_journal mmap-rw-fault allocstale

LINUX_TARGETS = xfsctl bstat t_mtab getdevicesize preallo_rw_pattern_reader \
	preallo_rw_pattern_writer ftrunc trunc fs_perms testx looptest \
	locktest unwritten_mmap bulkstat_unlink_test deduperace \
	bulkstat_unlink_test_modified t_dir_offset t_futimens t_immutable \
	stale_handle pwrite_mmap_blocked t_dir_offset2 seek_sanity_test \
	seek_copy_test t_readdir_1 t_readdir_2 fsync-tester nsexec cloner \
	renameat2 t_getcwd e4compact test-nextquota punch-alternating \
	attr-list-by-handle-cursor-test listxattr dio-interleaved t_dir_type \
	dio-invalidate-cache stat_test t_encrypted_d_revalidate \
	attr_replace_test swapon mkswap t_attr_corruption t_open_tmpfiles \
	fscrypt-crypt-util bulkstat_null_ocount splice-test chprojid_fail \
	detached_mounts_propagation ext4_resize t_readdir_3 splice2pipe

EXTRA_EXECS = dmerror fill2attr fill2fs fill2fs_check scaleread.sh \
	      btrfs_crc32c_forged_name.py

SUBDIRS = log-writes perf

LLDLIBS = $(LIBHANDLE) $(LIBACL) -lpthread -lrt

ifeq ($(HAVE_XLOG_ASSIGN_LSN), true)
LINUX_TARGETS += loggen
endif

ifeq ($(HAVE_FIEMAP), true)
LINUX_TARGETS += fiemap-tester t_stripealign
endif

ifeq ($(HAVE_OPEN_BY_HANDLE_AT), true)
LINUX_TARGETS += open_by_handle
endif

ifeq ($(HAVE_FALLOCATE), true)
LCFLAGS += -DHAVE_FALLOCATE
endif

ifeq ($(PKG_PLATFORM),linux)
TARGETS += $(LINUX_TARGETS)
endif

ifeq ($(HAVE_DB), true)
TARGETS += dbtest
LLDLIBS += $(LIBGDBM)
endif

ifeq ($(HAVE_AIO), true)
TARGETS += t_mmap_dio
SUBDIRS += aio-dio-regress
LLDLIBS += -laio
endif

ifeq ($(HAVE_URING), true)
TARGETS += uring_read_fault
LLDLIBS += -luring
endif

SUBDIRS += vfs
ifeq ($(HAVE_LIBCAP), true)
LLDLIBS += -lcap
endif

CFILES = $(TARGETS:=.c)
LDIRT = $(TARGETS) fssum


default: depend $(LDIRT) $(SUBDIRS)

depend: .dep

include $(BUILDRULES)

fssum: fssum.c md5.c
	@echo "    [CC]    $@"
	$(Q)$(LTLINK) fssum.c md5.c -o $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS)

$(TARGETS): $(LIBTEST)
	@echo "    [CC]    $@"
	$(Q)$(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(LIBTEST)

LINKTEST = $(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS)

install: default $(addsuffix -install,$(SUBDIRS))
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)/src
	$(LTINSTALL) -m 755 $(LDIRT) $(PKG_LIB_DIR)/src
	$(LTINSTALL) -m 755 $(EXTRA_EXECS) $(PKG_LIB_DIR)/src
	$(LTINSTALL) -m 644 dumpfile $(PKG_LIB_DIR)/src

%-install:
	$(MAKE) -C $* install

-include .dep
