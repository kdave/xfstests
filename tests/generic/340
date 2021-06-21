#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2016 SUSE Linux Products GmbH. All Rights Reserved.
#
# FSQA Test No. 340
#
# Test mmap writing races from racing threads
#
. ./common/preamble
_begin_fstest auto

# get standard environment and checks

# real QA test starts here
_supported_fs generic
_require_scratch
_require_test_program "holetest"

_scratch_mkfs >>$seqres.full 2>&1
_scratch_mount

$here/src/holetest -f $SCRATCH_MNT/testfile 1
$here/src/holetest -f $SCRATCH_MNT/testfile 16
$here/src/holetest -f $SCRATCH_MNT/testfile 256

status=0
exit
