#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0
#
# Verify that vendor extension (0xE0) benign secondary entries with a
# given marker are present in an exFAT filesystem image or block device.
#
# Usage: exfat_verify_vendor_ext.py <device> <marker> [count]
#
# Scans for a live (non-deleted) FILE entry set containing <count>
# (default 1) vendor_ext entries with <marker>.  Prints "PASS" or "FAIL".
#
# See exfat_inject_vendor_ext.py for the on-disk layout description.

import sys

ENTRY_SIZE = 32

# exFAT directory entry type codes
TYPE_FILE = 0x85
TYPE_VENDOR_EXT = 0xE0

# Offsets within the file entry
FILE_NUM_EXT_OFF = 1


def main():
    if len(sys.argv) < 3:
        print("Usage: %s <device> <marker> [count]" % sys.argv[0],
              file=sys.stderr)
        sys.exit(1)

    dev, marker_str = sys.argv[1], sys.argv[2]
    expected = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    marker = marker_str.encode('ascii')

    with open(dev, 'rb') as f:
        data = f.read()

    # Walk directory entries looking for a FILE entry set that contains
    # the expected number of vendor_ext entries with our marker.
    for i in range(0, len(data) - ENTRY_SIZE, ENTRY_SIZE):
        if data[i] != TYPE_FILE:
            continue
        num_ext = data[i + FILE_NUM_EXT_OFF]
        found = 0
        for j in range(1, num_ext + 1):
            off = i + j * ENTRY_SIZE
            if off + ENTRY_SIZE > len(data):
                break
            if data[off] == TYPE_VENDOR_EXT and \
                    marker in data[off:off + ENTRY_SIZE]:
                found += 1
        if found >= expected:
            print("PASS")
            sys.exit(0)

    print("FAIL")
    sys.exit(1)


if __name__ == '__main__':
    main()
