#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright 2014 (C) Red Hat, Inc., Lukas Czerner <lczerner@redhat.com>
#
# FS QA Test No. 008
#
# Makes calls to fallocate zero range and checks tossed ranges
#
# Primarily tests page boundries and boundries that are
#  off-by-one to ensure we're only tossing what's expected
#
. ./common/preamble
_begin_fstest auto quick prealloc zero

# Import common functions.
. ./common/filter
. ./common/punch

# real QA test starts here

# Modify as appropriate.

_require_xfs_io_command "fzero"
_require_test

testfile=$TEST_DIR/008.$$

_test_block_boundaries 1024 fzero _filter_xfs_io_unique $testfile
_test_block_boundaries 2048 fzero _filter_xfs_io_unique $testfile
_test_block_boundaries 4096 fzero _filter_xfs_io_unique $testfile
_test_block_boundaries 65536 fzero _filter_xfs_io_unique $testfile

# success, all done
status=0
exit
