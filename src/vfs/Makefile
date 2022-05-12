# SPDX-License-Identifier: GPL-2.0

TOPDIR = ../..
include $(TOPDIR)/include/builddefs

TARGETS = vfstest mount-idmapped
CFILES_VFSTEST = vfstest.c btrfs-idmapped-mounts.c idmapped-mounts.c utils.c
CFILES_MOUNT_IDMAPPED = mount-idmapped.c utils.c

HFILES = missing.h utils.h btrfs-idmapped-mounts.h idmapped-mounts.h
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

vfstest: $(CFILES_VFSTEST)
	@echo "    [CC]    $@"
	$(Q)$(LTLINK) $(CFILES_VFSTEST) -o $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS)

mount-idmapped: $(CFILES_MOUNT_IDMAPPED)
	@echo "    [CC]    $@"
	$(Q)$(LTLINK) $(CFILES_MOUNT_IDMAPPED) -o $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS)

install:
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)/src/vfs
	$(INSTALL) -m 755 $(TARGETS) $(PKG_LIB_DIR)/src/vfs

-include .dep
