#
# Copyright (c) 2003-2006 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

LSRCFILES = \
	manual_format.m4 \
	package_acldev.m4 \
	package_aiodev.m4 \
	package_gdbmdev.m4 \
	package_attrdev.m4 \
	package_dmapidev.m4 \
	package_globals.m4 \
	package_libcdev.m4 \
	package_ncurses.m4 \
	package_pthread.m4 \
	package_ssldev.m4 \
	package_types.m4 \
	package_utilies.m4 \
	package_uuiddev.m4 \
	package_xfslibs.m4 \
	general_headers.m4 \
	visibility_hidden.m4 \
	multilib.m4

default: 

include $(BUILDRULES)

install install-dev install-lib: default
