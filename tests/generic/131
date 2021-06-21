#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2006 Silicon Graphics, Inc.  All Rights Reserved.
#
# FSQA Test No. 131
#
# lock test created from CXFSQA test lockfile_simple
#
. ./common/preamble
_begin_fstest perms auto quick

# Import common functions.
. ./common/filter
. ./common/locktest

# real QA test starts here
_supported_fs generic
_require_test
_require_test_fcntl_advisory_locks

_run_locktest

exit
