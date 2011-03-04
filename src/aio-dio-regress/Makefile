TOPDIR = ../..
include $(TOPDIR)/include/builddefs

TARGETS = $(basename $(wildcard *.c))

CFILES = $(TARGETS:=.c)
LDIRT = $(TARGETS)

LLDLIBS = -laio -lpthread

default: depend $(TARGETS)

depend: .dep

include $(BUILDRULES)

$(TARGETS):
	@echo "    [CC]    $@"
	$(Q)$(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(LIBTEST)

install:
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)/src/aio-dio-regress
	$(INSTALL) -m 755 $(TARGETS) $(PKG_LIB_DIR)/src/aio-dio-regress

-include .dep
