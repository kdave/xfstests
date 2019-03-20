# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2005 Silicon Graphics, Inc.  All Rights Reserved.
#
# randomize stdin.

function randomize(array, N) {
  for(i = 0; i < N; i++) {
    j = int(rand()*N)
    if ( i != j) {
    tmp = array[i]
    array[i] = array[j]
    array[j] = tmp
    }
  }
return
}

BEGIN {
	srand(seed)
}
{
	array[NR - 1] = $0
}
END {
    randomize(array, NR)
    for (i = 0; i < NR; i++) printf("%s ", array[i])
}

