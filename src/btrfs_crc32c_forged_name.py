# SPDX-License-Identifier: GPL-2.0

import struct
import sys
import os
import argparse

class CRC32(object):
  """A class to calculate and manipulate CRC32."""
  def __init__(self):
    self.polynom = 0x82F63B78
    self.table, self.reverse = [0]*256, [0]*256
    self._build_tables()

  def _build_tables(self):
    for i in range(256):
      fwd = i
      rev = i << 24
      for j in range(8, 0, -1):
        # build normal table
        if (fwd & 1) == 1:
          fwd = (fwd >> 1) ^ self.polynom
        else:
          fwd >>= 1
        self.table[i] = fwd & 0xffffffff
        # build reverse table =)
        if rev & 0x80000000 == 0x80000000:
          rev = ((rev ^ self.polynom) << 1) | 1
        else:
          rev <<= 1
        rev &= 0xffffffff
        self.reverse[i] = rev

  def calc(self, s):
    """Calculate crc32 of a string.
       Same crc32 as in (binascii.crc32)&0xffffffff.
    """
    crc = 0xffffffff
    for c in s:
      crc = (crc >> 8) ^ self.table[(crc ^ ord(c)) & 0xff]
    return crc^0xffffffff

  def forge(self, wanted_crc, s, pos=None):
    """Forge crc32 of a string by adding 4 bytes at position pos."""
    if pos is None:
      pos = len(s)

    # forward calculation of CRC up to pos, sets current forward CRC state
    fwd_crc = 0xffffffff
    for c in s[:pos]:
      fwd_crc = (fwd_crc >> 8) ^ self.table[(fwd_crc ^ ord(c)) & 0xff]

    # backward calculation of CRC up to pos, sets wanted backward CRC state
    bkd_crc = wanted_crc^0xffffffff
    for c in s[pos:][::-1]:
      bkd_crc = ((bkd_crc << 8) & 0xffffffff) ^ self.reverse[bkd_crc >> 24]
      bkd_crc ^= ord(c)

    # deduce the 4 bytes we need to insert
    for c in struct.pack('<L',fwd_crc)[::-1]:
      bkd_crc = ((bkd_crc << 8) & 0xffffffff) ^ self.reverse[bkd_crc >> 24]
      bkd_crc ^= ord(c)

    res = s[:pos] + struct.pack('<L', bkd_crc) + s[pos:]
    return res

  def parse_args(self):
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", default=os.getcwd(), dest='dir',
                        help="directory to generate forged names")
    parser.add_argument("-c", default=1, type=int, dest='count',
                        help="number of forged names to create")
    return parser.parse_args()

if __name__=='__main__':

  crc = CRC32()
  wanted_crc = 0x00000000
  count = 0
  args = crc.parse_args()
  dirpath=args.dir
  while count < args.count :
    origname = os.urandom (89).encode ("hex")[:-1].strip ("\x00")
    forgename = crc.forge(wanted_crc, origname, 4)
    if ("/" not in forgename) and ("\x00" not in forgename):
      srcpath=dirpath + '/' + str(count)
      dstpath=dirpath + '/' + forgename
      file (srcpath, 'a').close()
      os.rename(srcpath, dstpath)
      os.system('btrfs fi sync %s' % (dirpath))
      count+=1;
