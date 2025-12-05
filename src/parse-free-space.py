#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0
#
# Parse block group and extent tree output to create a free sector list.
#
# Usage:
#  ./parse-free-space -n <nodesize> -b <bg_dump> -e <extent_dump>
#
# nodesize:     The nodesize of the btrfs
# bg_dump:      The tree dump file that contains block group items
#               If block-group-tree feature is enabled, it's block group tree dump.
#               Otherwise it's extent tree dump
# extent_dump:  The tree dump file that contains extent items
#               Just the extent tree dump, requires all tree block and data have
#               corresponding extent/metadata item.
#
# The output is "%d %d", the first one is the sector number (in 512 byte) of the
# free space, the second one is the last sector number of the free range.

import getopt
import sys
import re

bg_match = "^.item [0-9]* key \\([0-9]* BLOCK_GROUP_ITEM [0-9]*\\).*"
metadata_match="^.item [0-9]* key \\([0-9]* METADATA_ITEM [0-9]*\\).*"
data_match="^.item [0-9]* key \\([0-9]* EXTENT_ITEM [0-9]*\\).*"

def parse_block_groups(file_path):
    bg_list = []
    with open(file_path, 'r') as bg_file:
        for line in bg_file:
            match = re.search(bg_match, line)
            if match == None:
                continue
            start = match.group(0).split()[3][1:]
            length = match.group(0).split()[5][:-1]
            bg_list.append({'start': int(start), 'length': int(length)})
    return sorted(bg_list, key=lambda d: d['start'])

def parse_extents(file_path):
    extent_list = []
    with open(file_path, 'r') as bg_file:
        for line in bg_file:
            match = re.search(data_match, line)
            if match:
                start = match.group(0).split()[3][1:]
                length = match.group(0).split()[5][:-1]
                extent_list.append({'start': int(start), 'length': int(length)})
                continue
            match = re.search(metadata_match, line)
            if match:
                start = match.group(0).split()[3][1:]
                length = nodesize
                extent_list.append({'start': int(start), 'length': int(length)})
                continue
    return sorted(extent_list, key=lambda d: d['start'])

def range_end(range):
    return range['start'] + range['length']

def calc_free_spaces(bg_list, extent_list):
    free_list = []
    bg_iter = iter(bg_list)
    cur_bg = next(bg_iter)
    prev_end = cur_bg['start']

    for cur_extent in extent_list:
        # Finished one bg, add the remaining free space.
        while range_end(cur_bg) <= cur_extent['start']:
            if range_end(cur_bg) > prev_end:
                free_list.append({'start': prev_end,
                                  'length': range_end(cur_bg) - prev_end})
            cur_bg = next(bg_iter)
            prev_end = cur_bg['start']

        if prev_end < cur_extent['start']:
            free_list.append({'start': prev_end,
                              'length': cur_extent['start'] - prev_end})
        prev_end = range_end(cur_extent)

    # Handle the remaining part in the bg
    if range_end(cur_bg) > prev_end:
        free_list.append({'start': prev_end,
                          'length': range_end(cur_bg) - prev_end})

    # Handle the remaining empty bgs (if any)
    for cur_bg in bg_iter:
        free_list.append({'start': cur_bg['start'],
                          'length': cur_bg['length']})


    return free_list

nodesize = 0
sectorsize = 512
bg_file_path = ''
extent_file_path = ''

opts, args = getopt.getopt(sys.argv[1:], 's:n:b:e:')
for o, a in opts:
    if o == '-n':
        nodesize = int(a)
    elif o == '-b':
        bg_file_path = a
    elif o == '-e':
        extent_file_path = a
    elif o == '-s':
        sectorsize = int(a)

if nodesize == 0 or sectorsize == 0:
    print("require -n <nodesize> and -s <sectorsize>")
    sys.exit(1)
if not bg_file_path or not extent_file_path:
    print("require -b <bg_file> and -e <extent_file>")
    sys.exit(1)

bg_list = parse_block_groups(bg_file_path)
extent_list = parse_extents(extent_file_path)
free_space_list = calc_free_spaces(bg_list, extent_list)
for free_space in free_space_list:
    print(free_space['start'] >> 9,
          (range_end(free_space) >> 9) - 1)
