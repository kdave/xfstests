TOPDIR = ../..
include $(TOPDIR)/include/builddefs

TARGETS = $(basename $(wildcard *.c))

CFILES = $(TARGETS:=.c)
LDIRT = $(TARGETS)

LIBAIO = -laio -lpthread

default: $(TARGETS)

include $(BUILDRULES)

$(TARGETS): %: %.c
	$(CC) -g -Wall $(LIBAIO) -o $@ $*.c

install:
	$(INSTALL) -m 755 -d $(PKG_LIB_DIR)/src/aio-dio-regress
	$(INSTALL) -m 755 $(TARGETS) $(PKG_LIB_DIR)/src/aio-dio-regress
