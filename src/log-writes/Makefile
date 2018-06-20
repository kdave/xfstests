# SPDX-License-Identifier: GPL-2.0

TOPDIR = ../..
include $(TOPDIR)/include/builddefs

TARGETS = replay-log

CFILES = replay-log.c log-writes.c
LDIRT = $(TARGETS)

default: depend $(TARGETS)

depend: .dep

include $(BUILDRULES)

$(TARGETS): $(CFILES)
	@echo "    [CC]    $@"
	$(Q)$(LTLINK) $(CFILES) -o $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS)

install:
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)/src/log-writes
	$(INSTALL) -m 755 $(TARGETS) $(PKG_LIB_DIR)/src/log-writes

-include .dep
