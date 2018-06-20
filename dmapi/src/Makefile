# SPDX-License-Identifier: GPL-2.0+
# Copyright (C) 2011 SGI  All Rights Reserved.
#
#
#
TOPDIR = ../..

include $(TOPDIR)/include/builddefs

LIB_SUBDIRS = common
TOOL_SUBDIRS = sample_hsm simple suite1 suite2

SUBDIRS = $(LIB_SUBDIRS) $(TOOL_SUBDIRS)

default: $(SUBDIRS)

$(TOOL_SUBDIRS): $(LIB_SUBDIRS)

include $(BUILDRULES)

install: default
