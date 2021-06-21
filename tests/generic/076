#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2000-2004 Silicon Graphics, Inc.  All Rights Reserved.
#
# FS QA Test No. 076
#
# Test blockdev reads in parallel with filesystem reads/writes
#
. ./common/preamble
_begin_fstest metadata rw udf auto quick stress

_lets_get_pidst()
{
	if [ -n "$pid" ]; then
		kill -TERM $pid 2>/dev/null
		pid=""
		wait 2>/dev/null
	fi
}

# Override the default cleanup function.
_cleanup()
{
	echo "*** unmount"
	_scratch_unmount 2>/dev/null
	_lets_get_pidst
}
_register_cleanup "_cleanup; rm -f $tmp.*"

# Import common functions.
. ./common/filter

# real QA test starts here
_supported_fs generic

_require_scratch
_require_local_device $SCRATCH_DEV

echo "*** init fs"

_scratch_unmount >/dev/null 2>&1
echo "*** MKFS ***"                         >>$seqres.full
echo ""                                     >>$seqres.full
_scratch_mkfs                               >>$seqres.full 2>&1 \
	|| _fail "mkfs failed"
_scratch_mount

echo "*** test concurrent block/fs access"

cat $SCRATCH_DEV >/dev/null &
pid=$!

FSSTRESS_ARGS=`_scale_fsstress_args -d $SCRATCH_MNT -p 2 -n 2000 $FSSTRESS_AVOID`
echo "run fsstress with args: $FSSTRESS_ARGS" >>$seqres.full
$FSSTRESS_PROG $FSSTRESS_ARGS >>$seqres.full
_lets_get_pidst

echo "*** done"
status=0
exit
