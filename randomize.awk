# Copyright (c) 2005 Silicon Graphics, Inc.  All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it would be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write the Free Software Foundation,
# Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
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

