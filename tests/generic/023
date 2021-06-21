#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2014 Miklos Szeredi.  All Rights Reserved.
#
# FS QA Test No. generic/023
#
# Check renameat2 syscall without flags
#
. ./common/preamble
_begin_fstest auto quick

# Import common functions.
. ./common/renameat2

_supported_fs generic

_require_test
_require_renameat2
_require_symlinks

# real QA test starts here

rename_dir=$TEST_DIR/$$
mkdir -p $rename_dir
_rename_tests $rename_dir
rmdir $rename_dir

# success, all done
status=0
exit
