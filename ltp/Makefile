#
# Copyright (c) 2003-2005 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

TARGETS = doio fsstress fsx growfiles iogen
SCRIPTS = rwtest.sh
CFILES = $(TARGETS:=.c)
HFILES = doio.h
LDIRT = $(TARGETS)
LCFLAGS = -DXFS
LCFLAGS += -I$(TOPDIR)/src #Used for including $(TOPDIR)/src/global.h

ifeq ($(HAVE_ATTR_LIST), true)
LCFLAGS += -DHAVE_ATTR_LIST
LLDLIBS += $(LIBATTR)
endif

ifeq ($(HAVE_AIO), true)
TARGETS += aio-stress
LCFLAGS += -DAIO
LLDLIBS += -laio -lpthread
endif

ifeq ($(HAVE_FALLOCATE), true)
LCFLAGS += -DFALLOCATE
endif

default: depend $(TARGETS)

depend: .dep

include $(BUILDRULES)

$(TARGETS): $(LIBTEST)
	@echo "    [CC]    $@"
	$(Q)$(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(LIBTEST)

install: default
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)/ltp
	$(LTINSTALL) -m 755 $(TARGETS) $(PKG_LIB_DIR)/ltp
	$(INSTALL) -m 755 $(SCRIPTS) $(PKG_LIB_DIR)/ltp

-include .dep
