#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2016 SUSE Linux Products GmbH. All Rights Reserved.
#
# FSQA Test No. 346
#
# Test races between mmap and normal writes from racing threads
#
. ./common/preamble
_begin_fstest auto quick rw

# get standard environment and checks

# real QA test starts here
_supported_fs generic
_require_scratch
_require_test_program "holetest"

_scratch_mkfs >>$seqres.full 2>&1
_scratch_mount

$here/src/holetest -f -w $SCRATCH_MNT/testfile 1
$here/src/holetest -f -w $SCRATCH_MNT/testfile 16
$here/src/holetest -f -w $SCRATCH_MNT/testfile 256

status=0
exit
