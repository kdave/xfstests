# 
# Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ../..
include $(TOPDIR)/include/builddefs

TARGETS = alloc bstat dbtest devzero dirstress fault feature fsstress \
	  fill fill2 holes ioctl loggen lstat64 nametest permname \
	  randholes testrmt truncfile usemem runas
CFILES = $(TARGETS:=.c) random.c
HFILES = global.h
LDIRT = $(TARGETS)

DBMLIB = -lgdbm
RMTLIB = $(TOPDIR)/dump/librmt/librmt.a

default: $(TARGETS)

include $(BUILDRULES)

install: default

# binaries using non-default objects/libs need an entry below
# 
RANDHOLES_OBJECTS = randholes.o random.o
randholes:	$(HFILES) $(RANDHOLES_OBJECTS)
		$(CCF) -o $@ $(LDFLAGS) $(RANDHOLES_OBJECTS) $(LDLIBS)

TRUNCFILE_OBJECTS = truncfile.o random.o
truncfile:	$(HFILES) $(TRUNCFILE_OBJECTS)
		$(CCF) -o $@ $(LDFLAGS) $(TRUNCFILE_OBJECTS) $(LDLIBS)

FSSTRESS_OBJECTS = fsstress.o random.o $(LIBATTR)
fsstress:	$(HFILES) $(FSSTRESS_OBJECTS)
		$(CCF) -o $@ $(LDFLAGS) $(FSSTRESS_OBJECTS) $(LDLIBS)

DBTEST_OBJECTS = dbtest.o random.o $(DBMLIB)
dbtest:		$(HFILES) $(DBTEST_OBJECTS)
		$(CCF) -o $@ $(LDFLAGS) $(DBTEST_OBJECTS) $(LDLIBS)

NAMETEST_OBJECTS = nametest.o random.o
nametest:	$(HFILES) $(NAMETEST_OBJECTS)
		$(CCF) -o $@ $(LDFLAGS) $(NAMETEST_OBJECTS) $(LDLIBS)

BSTAT_OBJECTS = bstat.o $(LIBHANDLE)
bstat:		$(HFILES) $(BSTAT_OBJECTS)
		$(CCF) -o $@ $(LDFLAGS) $(BSTAT_OBJECTS) $(LDLIBS)

TESTRMT_OBJECTS = testrmt.o $(RMTLIB)
testrmt:	$(TESTRMT_OBJECTS)
		$(CCF) -o $@ $(LDFLAGS) $(TESTRMT_OBJECTS) $(LDLIBS)

LOGGEN_OBJECTS = loggen.o $(LIBXFS)
loggen:		$(HFILES) $(LOGGEN_OBJECTS)
		$(CCF) -o $@ $(LDFLAGS) $(LOGGEN_OBJECTS) $(LDLIBS)
