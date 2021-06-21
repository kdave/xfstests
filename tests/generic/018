#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2009 Eric Sandeen.  All Rights Reserved.
#
# FS QA Test No. generic/018
#
# Basic defragmentation sanity tests
#
. ./common/preamble
_begin_fstest auto fsr quick defrag

# Import common functions.
. ./common/filter
. ./common/defrag

# real QA test starts here
_supported_fs generic

# We require scratch so that we'll have free contiguous space
_require_scratch
_scratch_mkfs >/dev/null 2>&1
_scratch_mount

_require_defrag

fragfile=$SCRATCH_MNT/fragfile.$$

rm -f $fragfile

# Craft some fragmented files, defrag them, check the result.
bsize=$(_get_file_block_size $SCRATCH_MNT)

echo "zero-length file:" | tee -a $seqres.full
touch $fragfile
_defrag --before 0 --after 0 $fragfile

echo "Sparse file (no blocks):" | tee -a $seqres.full
$XFS_IO_PROG -f -c "truncate 1m" $fragfile
_defrag --before 0 --after 0 $fragfile

echo "Contiguous file:" | tee -a $seqres.full
$XFS_IO_PROG -f -c "pwrite -b $((4 * bsize)) 0 $((4 * bsize))" $fragfile \
					> /dev/null
_defrag --before 1 --after 1 $fragfile

echo "Write backwards sync, but contiguous - should defrag to 1 extent" | tee -a $seqres.full
for i in `seq 9 -1 0`; do
	$XFS_IO_PROG -fs -c "pwrite -b $bsize $((i * bsize)) $bsize" $fragfile \
					> /dev/null
done

# Accept fewer fragments than we might expect; we don't have perfect control.
_defrag --max_before 10 --min_before 5 --after 1 $fragfile

echo "Write backwards sync leaving holes - defrag should do nothing" | tee -a $seqres.full
for i in `seq 31 -2 0`; do
	$XFS_IO_PROG -fs -c "pwrite -b $bsize $((i * bsize)) $bsize" $fragfile \
					> /dev/null
done
_defrag --before 16 --after 16 $fragfile

echo "Write forwards sync leaving holes - defrag should do nothing" | tee -a $seqres.full
for i in `seq 0 2 31`; do
	$XFS_IO_PROG -fs -c "pwrite -b $bsize $((i * bsize)) $bsize" $fragfile \
					> /dev/null
done
_defrag --before 16 --after 16 $fragfile

status=0
exit
