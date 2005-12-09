#
# Copyright (c) 2003-2005 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

TARGETS = doio fsstress fsx growfiles iogen
CFILES = $(TARGETS:=.c)
HFILES = doio.h
LDIRT = $(TARGETS)
LCFLAGS = -DXFS
IFLAG = -I$(TOPDIR)/src #Used for including $(TOPDIR)/src/global.h

ifeq ($(HAVE_ATTR_LIST), true)
LCFLAGS += -DHAVE_ATTR_LIST
endif
ifeq ($(PKG_PLATFORM),irix)
LCFLAGS += -DHAVE_ATTR_LIST
endif

ifeq ($(HAVE_AIO), true)
TARGETS += aio-stress
LCFLAGS += -DAIO
LIBAIO = -laio -lpthread
endif

default: $(TARGETS)

include $(BUILDRULES)
LINKTEST = $(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS)

doio: doio.c $(LIBTEST)
	$(LINKTEST) $(LIBTEST) $(IFLAG)

fsstress: fsstress.c $(LIBATTR) $(LIBTEST)
	$(LINKTEST) $(LIBATTR) $(LIBTEST) $(LDLIBS)  $(IFLAG)

fsx: fsx.c
	$(LINKTEST) $(LIBAIO) $(LDLIBS)  $(IFLAG)

growfiles: growfiles.c $(LIBTEST)
	$(LINKTEST) $(LIBTEST) $(LDLIBS)  $(IFLAG)

iogen: iogen.c $(LIBTEST)
	$(LINKTEST) $(LIBTEST)  $(IFLAG)

ifeq ($(HAVE_AIO), true)
aio-stress: aio-stress.c
	$(LINKTEST) $(LIBAIO) $(LDLIBS)
endif
