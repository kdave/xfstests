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

{
    srand()
    for (i = 0; i < NF; i++ ) array[i] = $(i+1)
    randomize(array, NF)
    for (i = 0; i < NF; i++) printf("%s ", array[i])
}

