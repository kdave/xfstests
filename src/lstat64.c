/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#include <unistd.h>
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

void
usage(void)
{
	fprintf(stderr, "Usage: lstat64 [-t] filename ...\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	struct stat64	sbuf;
	int		i, c;
	int		terse_flag = 0;

	while ((c = getopt(argc, argv, "t")) != EOF) {
		switch (c) {
			case 't':
				terse_flag = 1;
				break;

			case '?':
				usage();
		}
	}
	if (optind == argc) {
		usage();
	}

	time(&timebuf);

	for (i = optind; i < argc; i++) {
		char mode[] = "----------";

		if( lstat64(argv[i], &sbuf) < 0) {
			perror(argv[i]);
			continue;
		}

		if (terse_flag) {
			printf("%s %llu ", argv[i], (unsigned long long)sbuf.st_size);
		}
		else {
			printf("  File: \"%s\"\n", argv[i]);
			printf("  Size: %-10llu", (unsigned long long)sbuf.st_size);
		}

		if (sbuf.st_mode & S_IXOTH)
			mode[9] = 'x';
		if (sbuf.st_mode & S_IWOTH)
			mode[8] = 'w';
		if (sbuf.st_mode & S_IROTH)
			mode[7] = 'r';
		if (sbuf.st_mode & S_IXGRP)
			mode[6] = 'x';
		if (sbuf.st_mode & S_IWGRP)
			mode[5] = 'w';
		if (sbuf.st_mode & S_IRGRP)
			mode[4] = 'r';
		if (sbuf.st_mode & S_IXUSR)
			mode[3] = 'x';
		if (sbuf.st_mode & S_IWUSR)
			mode[2] = 'w';
		if (sbuf.st_mode & S_IRUSR)
			mode[1] = 'r';
		if (sbuf.st_mode & S_ISVTX)
			mode[9] = 't';
		if (sbuf.st_mode & S_ISGID)
			mode[6] = 's';
		if (sbuf.st_mode & S_ISUID)
			mode[3] = 's';

		if (!terse_flag)
			printf("   Filetype: ");
		switch (sbuf.st_mode & S_IFMT) {
		case S_IFSOCK:	
			if (!terse_flag)
				puts("Socket");
			mode[0] = 's';
			break;
		case S_IFDIR:	
			if (!terse_flag)
				puts("Directory");
			mode[0] = 'd';
			break;
		case S_IFCHR:	
			if (!terse_flag)
				puts("Character Device");
			mode[0] = 'c';
			break;
		case S_IFBLK:	
			if (!terse_flag)
				puts("Block Device");
			mode[0] = 'b';
			break;
		case S_IFREG:	
			if (!terse_flag)
				puts("Regular File");
			mode[0] = '-';
			break;
		case S_IFLNK:	
			if (!terse_flag)
				puts("Symbolic Link");
			mode[0] = 'l';
			break;
		case S_IFIFO:	
			if (!terse_flag)
				puts("Fifo File");
			mode[0] = 'f';
			break;
		default:	
			if (!terse_flag)
				puts("Unknown");
			mode[0] = '?';
		}

		if (terse_flag) {
			printf("%s %d,%d\n", mode, (int)sbuf.st_uid, (int)sbuf.st_gid);
			continue;
		}

		printf("  Mode: (%04o/%s)", (unsigned int)(sbuf.st_mode & 07777), mode);
		printf("         Uid: (%d)", (int)sbuf.st_uid);
		printf("  Gid: (%d)\n", (int)sbuf.st_gid);
		printf("Device: %2d,%-2d", major(sbuf.st_dev),
				minor(sbuf.st_dev));
		printf("  Inode: %-9llu", (unsigned long long)sbuf.st_ino);
		printf(" Links: %-5ld", (long)sbuf.st_nlink);

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
	exit(0);
}
