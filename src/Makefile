# 
# Copyright (c) 2000-2004 Silicon Graphics, Inc.  All Rights Reserved.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
# 
# This program is distributed in the hope that it would be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# 
# Further, this software is distributed without any warranty that it is
# free of the rightful claim of any third person regarding infringement
# or the like.  Any license provided herein, whether implied or
# otherwise, applies only to this software file.  Patent licenses, if
# any, provided herein do not apply to combinations of this program with
# other software, or any other product whatsoever.
# 
# You should have received a copy of the GNU General Public License along
# with this program; if not, write the Free Software Foundation, Inc., 59
# Temple Place - Suite 330, Boston MA 02111-1307, USA.
# 
# Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
# Mountain View, CA  94043, or:
# 
# http://www.sgi.com 
# 
# For further information regarding this notice, see: 
# 
# http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

TARGETS = dirstress fill fill2 getpagesize holes lstat64 \
	nametest permname randholes runas truncfile usemem \
	mmapcat append_reader append_writer dirperf metaperf \
	devzero feature alloc fault fstest t_access_root \
	godown resvtest

LINUX_TARGETS = loggen xfsctl bstat t_mtab

ifeq ($(PKG_PLATFORM),linux)
TARGETS += $(LINUX_TARGETS)
endif

ifeq ($(HAVE_DB), true)
TARGETS += dbtest
endif

ifeq ($(PKG_PLATFORM),linux)
TARGETS += t_immutable
endif

CFILES = $(TARGETS:=.c)
LDIRT = $(TARGETS)

default: $(TARGETS)

include $(BUILDRULES)
LINKTEST = $(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS)

randholes: randholes.o $(LIBTEST)
	$(LINKTEST) $(LIBTEST) $(LDLIBS)

truncfile: truncfile.o $(LIBTEST)
	$(LINKTEST) $(LIBTEST) $(LDLIBS)

dbtest:	dbtest.o $(LIBTEST)
	$(LINKTEST) $(LIBTEST) $(LIBGDBM) $(LDLIBS)

nametest: nametest.o $(LIBTEST)
	$(LINKTEST) $(LIBTEST) $(LDLIBS)

bstat: bstat.o $(LIBHANDLE)
	$(LINKTEST) $(LIBHANDLE) $(LDLIBS)

t_immutable: t_immutable.o $(LIBHANDLE) $(LIBACL)
	$(LINKTEST) $(LIBACL) $(LIBHANDLE) $(LDLIBS)

loggen:	loggen.o
	$(LINKTEST) $(LDLIBS)

fstest:	fstest.o
	$(LINKTEST)

resvtest: resvtest.o
	$(LINKTEST)

#scaleread: scaleread.o $(LDLIBS)
#	$(LINKTEST)

acl_get: acl_get.o $(LIBACL) $(LIBATTR)
	$(LINKTEST) $(LIBACL) $(LIBATTR) $(LDLIBS)


ifeq ($(PKG_PLATFORM),irix)
fill2: fill2.o 
	$(LINKTEST)  -lgen

runas: runas.o 
	$(LINKTEST)  -lgen
endif
