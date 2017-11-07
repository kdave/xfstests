TOPDIR = ../..
include $(TOPDIR)/include/builddefs

TARGETS = $(wildcard *.py) $(wildcard *.sql)

depend: .dep

include $(BUILDRULES)

install:
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)/src/perf
	$(INSTALL) -m 755 $(TARGETS) $(PKG_LIB_DIR)/src/perf

-include .dep
