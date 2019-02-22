# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2019 Nikolay Borisov, SUSE LLC.  All Rights Reserved.
#
# Parses btrfs' extent tree for holes. Holes are the ranges between 2 adjacent
# extent blocks. For example if we have the following 2 metadata items in the
# extent tree:
#	item 6 key (30425088 METADATA_ITEM 0) itemoff 16019 itemsize 33
#	item 7 key (30490624 METADATA_ITEM 0) itemoff 15986 itemsize 33
#
# There is a whole of 64k between then - 30490624−30425088 = 65536
# Same logic applies for adjacent EXTENT_ITEMS.
#
# The script requires the following parameters passed on command line:
#     * sectorsize - how many bytes per sector, used to convert the output of
#     the script to sectors.
#     * nodesize - size of metadata extents, used for internal calculations

# Given an extent line "item 2 key (13672448 EXTENT_ITEM 65536) itemoff 16153 itemsize 53"
# or "item 6 key (30425088 METADATA_ITEM 0) itemoff 16019 itemsize 33" returns
# either 65536 (for data extents) or the fixes nodesize value for metadata
# extents.
function get_extent_size(line, tmp) {
	if (line ~ data_match || line ~ bg_match) {
		split(line, tmp)
		gsub(/\)/,"", tmp[6])
		return tmp[6]
	} else if (line ~ metadata_match) {
		return nodesize
	}
}

# given a 'item 2 key (13672448 EXTENT_ITEM 65536) itemoff 16153 itemsize 53'
# and returns 13672448.
function get_extent_offset(line, tmp) {
	split(line, tmp)
	gsub(/\(/,"",tmp[4])
	return tmp[4]
}

# This function parses all the extents belonging to a particular block group
# which are accumulated in lines[] and calculates the offsets of the holes
# part of this block group.
#
# base_offset and bg_line are local variables
function print_array(base_offset, bg_line)
{
	if (match(lines[0], bg_match)) {
		# we don't have an extent at the beginning of of blockgroup, so we
		# have a hole between blockgroup offset and first extent offset
		bg_line = lines[0]
		prev_size=0
		prev_offset=get_extent_offset(bg_line)
		delete lines[0]
	} else {
		# we have an extent at the beginning of block group, so initialize
		# the prev_* vars correctly
		bg_line = lines[1]
		prev_size = get_extent_size(lines[0])
		prev_offset = get_extent_offset(lines[0])
		delete lines[1]
		delete lines[0]
	}

	bg_offset=get_extent_offset(bg_line)
	bgend=bg_offset + get_extent_size(bg_line)

	for (i in lines) {
			cur_size = get_extent_size(lines[i])
			cur_offset = get_extent_offset(lines[i])
			if (cur_offset  != prev_offset + prev_size)
				print int((prev_size + prev_offset) / sectorsize), int((cur_offset-1) / sectorsize)
			prev_size = cur_size
			prev_offset = cur_offset
	}

	print int((prev_size + prev_offset) / sectorsize), int((bgend-1) / sectorsize)
	total_printed++
	delete lines
}

BEGIN {
	loi_match="^.item [0-9]* key \\([0-9]* (BLOCK_GROUP_ITEM|METADATA_ITEM|EXTENT_ITEM) [0-9]*\\).*"
	metadata_match="^.item [0-9]* key \\([0-9]* METADATA_ITEM [0-9]*\\).*"
	data_match="^.item [0-9]* key \\([0-9]* EXTENT_ITEM [0-9]*\\).*"
	bg_match="^.item [0-9]* key \\([0-9]* BLOCK_GROUP_ITEM [0-9]*\\).*"
	node_match="^node.*$"
	leaf_match="^leaf [0-9]* flags"
	line_count=0
	total_printed=0
	skip_lines=0
}

{
	# skip lines not belonging to a leaf
	if (match($0, node_match)) {
		skip_lines=1
	} else if (match($0, leaf_match)) {
		skip_lines=0
	}

	if (!match($0, loi_match) || skip_lines == 1) next;

	# we have a line of interest, we need to parse it. First check if there is
	# anything in the array
	if (line_count==0) {
		lines[line_count++]=$0;
	} else {
		prev_line=lines[line_count-1]
		split(prev_line, prev_line_fields)
		prev_objectid=prev_line_fields[4]
		objectid=$4

		if (objectid == prev_objectid && match($0, bg_match)) {
			if (total_printed>0) {
				# We are adding a BG after we have added its first extent
				# previously, consider this a record ending event and just print
				# the array

				delete lines[line_count-1]
				print_array()
				# we now start a new array with current and previous lines
				line_count=0
				lines[line_count++]=prev_line
				lines[line_count++]=$0
			} else {
				# first 2 added lines are EXTENT and BG that match, in this case
				# just add them
				lines[line_count++]=$0

			}
		} else if (match($0, bg_match)) {
			# ordinary end of record
			print_array()
			line_count=0
			lines[line_count++]=$0
		} else {
			lines[line_count++]=$0
		}
	}
}

END {
	print_array()
}
