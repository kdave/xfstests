# SPDX-License-Identifier: GPL-2.0

TOPDIR = ../..
include $(TOPDIR)/include/builddefs

TARGETS = idmapped-mounts

CFILES = idmapped-mounts.c utils.c
HFILES = missing.h utils.h
LLDLIBS += -pthread
LDIRT = $(TARGETS)

ifeq ($(HAVE_LIBCAP), true)
LLDLIBS += -lcap
endif

ifeq ($(HAVE_URING), true)
LLDLIBS += -luring
endif

default: depend $(TARGETS)

depend: .dep

include $(BUILDRULES)

$(TARGETS): $(CFILES)
	@echo "    [CC]    $@"
	$(Q)$(LTLINK) $(CFILES) -o $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS)

install:
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)/src/idmapped-mounts
	$(INSTALL) -m 755 $(TARGETS) $(PKG_LIB_DIR)/src/idmapped-mounts

-include .dep
