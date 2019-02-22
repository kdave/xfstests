# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2019 Nikolay Borisov, SUSE LLC.  All Rights Reserved.
#
# Parses btrfs' device tree for holes. For example given the followin device extents:
#	item 3 key (1 DEV_EXTENT 38797312) itemoff 16091 itemsize 48
#		dev extent chunk_tree 3
#		chunk_objectid 256 chunk_offset 30408704 length 1073741824
#		chunk_tree_uuid b8c8f022-452b-4fc1-bf5c-b42d9fba613e
#	item 4 key (1 DEV_EXTENT 1112539136) itemoff 16043 itemsize 48
#		dev extent chunk_tree 3
#		chunk_objectid 256 chunk_offset 30408704 length 1073741824
#		chunk_tree_uuid b8c8f022-452b-4fc1-bf5c-b42d9fba613e
#
# The scripts will find out if there is a hole between 38797312+1073741824 and
# the start offset of the next extent, in this case 1112539136 so no hole. But
# if there was it would have printed the size of the hole.
#
# Following paramters must be set:
#    * sectorsize - how many bytes per sector, used to convert script's output
#    to sectors.
#    * devsize - size of the device in bytes, used to output the final
#    hole (if any)

# Given a 'chunk_objectid 256 chunk_offset 22020096 length 8388608' line this
# function returns 8388608
function get_extent_size(line, tmp) {
	split(line, tmp)
	return tmp[6]
}

# Given a '	item 3 key (1 DEV_EXTENT 38797312) itemoff 16091 itemsize 48' line
# this function returns 38797312
function get_extent_offset(line, tmp) {
	split(line, tmp)
	gsub(/\)/,"", tmp[6])
	return tmp[6]
}

BEGIN {
	dev_extent_match="^.item [0-9]* key \\([0-9]* DEV_EXTENT [0-9]*\\).*"
	dev_extent_len_match="^\\s*chunk_objectid [0-9]* chunk_offset [0-9]* length [0-9]*$"
}

{
	if (match($0, dev_extent_match)) {
		extent_offset = get_extent_offset($0)
		if (prev_extent_end) {
			hole_size = extent_offset - prev_extent_end
			if (hole_size > 0) {
				print prev_extent_end / sectorsize, int((extent_offset - 1) / sectorsize)
			}
		}
	} else if (match($0, dev_extent_len_match)) {
		extent_size = get_extent_size($0)
		prev_extent_end = extent_offset + extent_size
	}
}

END {
	if (prev_extent_end) {
		print prev_extent_end / sectorsize, int((devsize - 1) / sectorsize)
	}
}

