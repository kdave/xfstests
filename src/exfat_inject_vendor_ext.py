#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0
#
# Inject vendor extension (0xE0) benign secondary entries into an exFAT
# directory entry set on an unmounted filesystem image or block device.
#
# Usage: exfat_inject_vendor_ext.py <device> <marker> <filename> [count]
#
# Adds <count> (default 1) vendor_ext entries carrying <marker> into the
# entry set of <filename>.  The marker is stored in the custom-defined
# area so it can be verified after kernel operations.
#
# exFAT on-disk directory entry layout (each entry is 32 bytes):
#
#   Entry set for a file:
#     [0] File entry        (type 0x85)
#         byte 1: num_ext (number of secondary entries following)
#         bytes 2-3: entry set checksum (covers all entries, skipping these bytes)
#     [1] Stream extension  (type 0xC0)
#         byte 3: name_len (filename length in Unicode characters)
#         bytes 4-7: name_hash
#     [2..N] Filename entries (type 0xC1, each holds up to 15 UTF-16LE chars)
#         bytes 2-31: 15 UTF-16LE characters
#     [N+1..] Benign secondary entries (type 0xA0-0xFF, e.g. vendor_ext 0xE0)
#         Implementations MUST preserve these even if unrecognized (spec s8.2)
#
#   Vendor extension entry (type 0xE0):
#     byte 0: 0xE0 (entry type)
#     bytes 2-15: vendor GUID (we store our marker here)
#     bytes 18-31: vendor-defined data (we duplicate the marker here)
#
#   Free/unused directory slot: all 32 bytes are 0x00 (type byte == 0x00)
#

import struct
import sys

ENTRY_SIZE = 32

# exFAT directory entry type codes
TYPE_FILE = 0x85
TYPE_STREAM = 0xC0
TYPE_FILENAME = 0xC1
TYPE_VENDOR_EXT = 0xE0
TYPE_FREE = 0x00

# Offsets within specific entry types
FILE_NUM_EXT_OFF = 1        # byte offset of num_ext in file entry
FILE_CHECKSUM_OFF = 2       # byte offset of checksum in file entry
STREAM_NAME_LEN_OFF = 3    # byte offset of name_len in stream entry
FILENAME_CHARS_OFF = 2      # byte offset of first char in filename entry
CHARS_PER_NAME_ENTRY = 15   # UTF-16LE characters per filename entry


def find_entry_set(data, fname):
    """Scan raw data for a FILE+STREAM+NAME entry set matching fname."""
    pos = 0
    while pos < len(data) - ENTRY_SIZE * 3:
        if (data[pos] == TYPE_FILE and
                data[pos + ENTRY_SIZE] == TYPE_STREAM and
                data[pos + ENTRY_SIZE * 2] == TYPE_FILENAME):
            name_len = data[pos + ENTRY_SIZE + STREAM_NAME_LEN_OFF]
            num_name_entries = (name_len + CHARS_PER_NAME_ENTRY - 1) // \
                CHARS_PER_NAME_ENTRY
            chars = []
            for ne in range(num_name_entries):
                ne_off = pos + ENTRY_SIZE * (2 + ne)
                for c in range(CHARS_PER_NAME_ENTRY):
                    if len(chars) >= name_len:
                        break
                    ch = struct.unpack_from('<H', data,
                                           ne_off + FILENAME_CHARS_OFF +
                                           c * 2)[0]
                    chars.append(chr(ch))
            if ''.join(chars) == fname:
                return pos
        pos += ENTRY_SIZE
    return -1


def build_vendor_ext(marker_bytes, index):
    """Build a single vendor_ext directory entry with marker payload."""
    entry = bytearray(ENTRY_SIZE)
    entry[0] = TYPE_VENDOR_EXT
    tag = marker_bytes + struct.pack('B', index)
    # Store marker in both the GUID field (bytes 2-15) and
    # vendor-defined field (bytes 18-31)
    entry[2:2 + min(len(tag), 14)] = tag[:14]
    entry[18:18 + min(len(tag), 14)] = tag[:14]
    return entry


def update_checksum(data, file_off, num_ext):
    """Recompute entry set checksum (skip bytes 2-3 of file entry)."""
    es_len = ENTRY_SIZE * (1 + num_ext)
    es_data = data[file_off:file_off + es_len]
    chksum = 0
    for i in range(len(es_data)):
        if i == FILE_CHECKSUM_OFF or i == FILE_CHECKSUM_OFF + 1:
            continue
        chksum = (((chksum << 15) | (chksum >> 1)) + es_data[i]) & 0xFFFF
    return chksum


def main():
    if len(sys.argv) < 4:
        print("Usage: %s <device> <marker> <filename> [count]" % sys.argv[0],
              file=sys.stderr)
        sys.exit(1)

    dev, marker_str, fname = sys.argv[1], sys.argv[2], sys.argv[3]
    count = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    marker = marker_str.encode('ascii')

    with open(dev, 'r+b') as f:
        data = bytearray(f.read())

    file_off = find_entry_set(data, fname)
    if file_off < 0:
        print("ERROR: could not find file '%s'" % fname, file=sys.stderr)
        sys.exit(1)

    num_ext = data[file_off + FILE_NUM_EXT_OFF]
    es_end = file_off + ENTRY_SIZE * (1 + num_ext)

    # Verify free slots exist after the entry set
    for i in range(count):
        slot = es_end + i * ENTRY_SIZE
        if slot + ENTRY_SIZE > len(data) or data[slot] != TYPE_FREE:
            print("ERROR: no free slot at offset %d for entry %d" %
                  (slot, i), file=sys.stderr)
            sys.exit(1)

    # Write vendor_ext entries into the free slots
    for i in range(count):
        slot = es_end + i * ENTRY_SIZE
        entry = build_vendor_ext(marker, i)
        data[slot:slot + ENTRY_SIZE] = entry

    # Update num_ext in the file entry and recompute checksum
    num_ext += count
    data[file_off + FILE_NUM_EXT_OFF] = num_ext

    chksum = update_checksum(data, file_off, num_ext)
    struct.pack_into('<H', data, file_off + FILE_CHECKSUM_OFF, chksum)

    with open(dev, 'r+b') as f:
        f.write(data)

    print("Injected %d vendor_ext entries for '%s' at offset %d, num_ext=%d" %
          (count, fname, es_end, num_ext))


if __name__ == '__main__':
    main()
