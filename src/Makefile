# 
# Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
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

TARGETS = alloc acl_get bstat devzero dirstress fault feature \
	  fill fill2 getpagesize holes xfsctl loggen lstat64 \
	  nametest permname randholes runas truncfile usemem \
	  fstest mmapcat append_reader append_writer
ifeq ($(ENABLE_DBM), yes)
TARGETS += dbtest
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

loggen:	loggen.o $(LIBXFS)
	$(LINKTEST) $(LIBXFS) $(LDLIBS)

acl_get: acl_get.o $(LIBACL) $(LIBATTR)
	$(LINKTEST) $(LIBACL) $(LIBATTR) $(LDLIBS)
