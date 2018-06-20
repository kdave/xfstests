# SPDX-License-Identifier: GPL-2.0+
# Copyright (C) 2011 SGI  All Rights Reserved.
#
#
#
TOPDIR = ../../../..

include $(TOPDIR)/include/builddefs

LTLIBRARY = libdmtest.la

LTLDFLAGS = -static

LT_CURRENT = 1
LT_REVISION = 0
LT_AGE = 0

CFILES = find_session.c print.c stubs.c util.c
HFILES = hsm.h dmport.h

CFLAGS += -I$(TOPDIR)/dmapi/src/common -I/usr/include/xfs

default: depend $(LTLIBRARY)

depend: .ltdep

include $(BUILDRULES)

install install-dev: default

-include .ltdep
