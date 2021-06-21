#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2014 Red Hat Inc.  All Rights Reserved.
#
# FS QA Test No. generic/028
#
# The following commit introduced a race condition that causes getcwd(2)
# to return "/" instead of correct path
#
# 232d2d6 dcache: Translating dentry into pathname without taking rename_lock
#
# These commits fixed the bug
# ede4ceb prepend_path() needs to reinitialize dentry/vfsmount/mnt on restarts
# f650080 __dentry_path() fixes
#
. ./common/preamble
_begin_fstest auto quick

# Import common functions.
. ./common/filter

# real QA test starts here
_supported_fs generic
_require_test

echo "Silence is golden"

$here/src/t_getcwd $TEST_DIR
status=$?
exit
