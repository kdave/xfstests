/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

long	timebuf;

void
timesince(long timesec)
{
	long	d_since;	/* days */
	long	h_since;	/* hours */
	long	m_since;	/* minutes */
	long	s_since;	/* seconds */

	s_since = timebuf - timesec;
	d_since = s_since / 86400l ;
	s_since -= d_since * 86400l ;
	h_since = s_since / 3600l ;
	s_since -= h_since * 3600l ;
	m_since = s_since / 60l ;
	s_since -= m_since * 60l ;

	printf("(%05ld.%02ld:%02ld:%02ld)\n",
			d_since, h_since, m_since, s_since);
}

int
main(int argc, char **argv)
{
	struct stat64	sbuf;
	char		mode[10];
	int		i, c;

	time(&timebuf);

	for (i = 1; i < argc; i++) {

		if( lstat64(argv[i], &sbuf) < 0) {
			perror(argv[i]);
			continue;
		}

		printf("  File: \"%s\"\n", argv[i]);
		printf("  Size: %-10llu", sbuf.st_size);

		strcpy(mode,"----------");
		if (sbuf.st_mode & (S_IEXEC>>6))
			mode[9] = 'x';
		if (sbuf.st_mode & (S_IWRITE>>6))
			mode[8] = 'w';
		if (sbuf.st_mode & (S_IREAD>>6))
			mode[7] = 'r';
		if (sbuf.st_mode & (S_IEXEC>>3))
			mode[6] = 'x';
		if (sbuf.st_mode & (S_IWRITE>>3))
			mode[5] = 'w';
		if (sbuf.st_mode & (S_IREAD>>3))
			mode[4] = 'r';
		if (sbuf.st_mode & S_IEXEC)
			mode[3] = 'x';
		if (sbuf.st_mode & S_IWRITE)
			mode[2] = 'w';
		if (sbuf.st_mode & S_IREAD)
			mode[1] = 'r';
		if (sbuf.st_mode & S_ISVTX)
			mode[9] = 't';
		if (sbuf.st_mode & S_ISGID)
			mode[6] = 's';
		if (sbuf.st_mode & S_ISUID)
			mode[3] = 's';

		printf("   Filetype: ");
		switch (sbuf.st_mode & S_IFMT) {
		case S_IFSOCK:	
			puts("Socket");
			mode[0] = 's';
			break;
		case S_IFDIR:	
			puts("Directory");
			mode[0] = 'd';
			break;
		case S_IFCHR:	
			puts("Character Device");
			mode[0] = 'c';
			break;
		case S_IFBLK:	
			puts("Block Device");
			mode[0] = 'b';
			break;
		case S_IFREG:	
			puts("Regular File");
			mode[0] = '-';
			break;
		case S_IFLNK:	
			puts("Symbolic Link");
			mode[0] = 'l';
			break;
		case S_IFIFO:	
			puts("Fifo File");
			mode[0] = 'f';
			break;
		default:	
			puts("Unknown");
			mode[0] = '?';
		}

		printf("  Mode: (%04o/%s)", sbuf.st_mode & 07777, mode);
		printf("         Uid: (%d)", sbuf.st_uid);
		printf("  Gid: (%d)\n", sbuf.st_gid);
		printf("Device: %2d,%-2d", major(sbuf.st_dev),
				minor(sbuf.st_dev));
		c = printf("  Inode: %-10llu", (unsigned long long)sbuf.st_ino);
		if (c >= 10)
			putchar(' ');
		printf("Links: %-5d", sbuf.st_nlink);

		if ( ((sbuf.st_mode & S_IFMT) == S_IFCHR)
		    || ((sbuf.st_mode & S_IFMT) == S_IFBLK) )
			printf("     Device type: %2d,%-2d\n",
				major(sbuf.st_rdev), minor(sbuf.st_rdev));
		else
			printf("\n");

		printf("Access: %.24s",ctime(&sbuf.st_atime));
		timesince(sbuf.st_atime);
		printf("Modify: %.24s",ctime(&sbuf.st_mtime));
		timesince(sbuf.st_mtime);
		printf("Change: %.24s",ctime(&sbuf.st_ctime));
		timesince(sbuf.st_ctime);

		if (i+1 < argc)
			printf("\n");
	}
	if (i == 1) {
		fprintf(stderr, "Usage: lstat64 filename...\n");
		exit(1);
	}
	exit(0);
}
