#
# Copyright (C) 2011 SGI  All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
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
