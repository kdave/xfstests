#
# Copyright (c) 2003 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

LTLIBRARY = libtest.la
LT_CURRENT = 0
LT_REVISION = 0
LT_AGE = 0

#
# Everything (except for random.c) copied directly from LTP.
# Refer to http://ltp.sourceforge.net/ for complete source.
#
CFILES = dataascii.c databin.c datapid.c file_lock.c forker.c \
	pattern.c open_flags.c random_range.c string_to_tokens.c \
	str_to_bytes.c tlibio.c write_log.c \
	random.c

default: depend $(LTLIBRARY)

include $(BUILDRULES)

depend: .ltdep

install install-dev: default

-include .ltdep
