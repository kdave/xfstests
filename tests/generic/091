#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2000-2004 Silicon Graphics, Inc.  All Rights Reserved.
#
# FS QA Test No. 091
#
# fsx exercising direct IO -- sub-block sizes and concurrent buffered IO
#
. ./common/preamble
_begin_fstest rw auto quick

# Import common functions.
. ./common/filter

# real QA test starts here
_supported_fs generic
_require_test
_require_odirect

psize=`$here/src/feature -s`
bsize=`_min_dio_alignment $TEST_DEV`

# fsx usage:
# 
# -N numops: total # operations to do 
# -l flen: the upper bound on file size 
# -o oplen: the upper bound on operation size (64k default)
# -r readbdy: $psize would make reads page aligned (on i386)
# -t truncbdy: $psize would make truncates page aligned (on i386)
# -w writebdy: $psize would make writes page aligned (on i386)
# 
# -Z: O_DIRECT (use -R, -W, -r and -w too)
# -R: mapped read operations DISabled
# -W: mapped write operations DISabled

#run_fsx -N 10000            -l 500000 -r $psize -t $psize -w $psize -Z -R -W
 run_fsx -N 10000            -l 500000 -r PSIZE -t BSIZE -w BSIZE -Z -R -W
 run_fsx -N 10000  -o 8192   -l 500000 -r PSIZE -t BSIZE -w BSIZE -Z -R -W
#run_fsx -N 10000  -o 16384  -l 500000 -r PSIZE -t PSIZE -w PSIZE -Z -R -W
 run_fsx -N 10000  -o 32768  -l 500000 -r PSIZE -t BSIZE -w BSIZE -Z -R -W
#run_fsx -N 10000  -o 128000 -l 500000 -r PSIZE -t PSIZE -w PSIZE -Z -R -W
 run_fsx -N 10000  -o 8192   -l 500000 -r PSIZE -t BSIZE -w BSIZE -Z -R -W
#run_fsx -N 10000  -o 16384  -l 500000 -r PSIZE -t PSIZE -w PSIZE -Z -R -W
 run_fsx -N 10000  -o 32768  -l 500000 -r PSIZE -t BSIZE -w BSIZE -Z -R -W
#run_fsx -N 10000  -o 128000 -l 500000 -r PSIZE -t PSIZE -w PSIZE -Z -W
 run_fsx -N 10000  -o 128000 -l 500000 -r PSIZE -t BSIZE -w BSIZE -Z -W

status=0
exit
