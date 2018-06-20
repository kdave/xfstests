# SPDX-License-Identifier: GPL-2.0+
# Copyright (C) 2011 SGI  All Rights Reserved.
#
#
#
TOPDIR = ../../..

include $(TOPDIR)/include/builddefs

LIB_SUBDIRS = lib
TOOL_SUBDIRS = cmd

SUBDIRS = $(LIB_SUBDIRS) $(TOOL_SUBDIRS)

default: $(SUBDIRS)

$(TOOL_SUBDIRS): $(LIB_SUBDIRS)

include $(BUILDRULES)

install: default

install-dev:
