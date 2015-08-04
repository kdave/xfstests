/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef O_DIRECT
#define O_DIRECT	040000
#endif

#define WAITTIME	60
#define BUFSIZE		4096
#define ALIGNMENT	16384
#define TRUNCSIZE	1000

/* write data to disk - buffered/sync or direct
 * write different buffered data to disk
 * truncate
 * direct read back, see if server puts stale data down
 */

int
main(argc, argv)
int	argc;
char	**argv;
{
	int fd, err, elapsed;
	char *buf = NULL, *goodbuf = NULL;
	time_t starttime;
        char *filename="testfile";
	int c;

	if (argc != 3) {
		printf("Usage: trunc -f testfilename\n");
		exit(1);
        }

	while((c=getopt(argc,argv,"f:"))!=EOF) {
		switch (c) {
		case 'f':
			filename = optarg;
			break;
		default:
			fprintf(stderr,"Usage: trunc -f filename\n");
			exit(1);
		}
	}

	err = posix_memalign((void **)&buf, ALIGNMENT, BUFSIZE);
	if (err)
		fprintf(stderr, "posix_memalign failed: %s\n", strerror(err));

	err = posix_memalign((void **)&goodbuf, ALIGNMENT, BUFSIZE);
	if (err)
		fprintf(stderr, "posix_memalign failed: %s\n", strerror(err));

	err = unlink(filename);
/*	if (err < 0) perror("unlink failed");*/
        
	fd = open(filename, O_CREAT|O_RDWR|O_DIRECT, 0666);
	if (fd < 0) perror("direct open failed");

	memset(buf, 1, BUFSIZE);

	printf("direct write of 1's into file\n");	
	err = write(fd, buf, BUFSIZE);
	if (err < 0) perror("direct write failed");

	close(fd);
	
	fd = open(filename, O_CREAT|O_RDWR, 0666);
	if (fd < 0) perror("buffered open failed");

	/* 1 now on disk */

	memset(buf, 2, BUFSIZE);
	memset(goodbuf, 2, BUFSIZE);

	printf("buffered write of 2's into file\n");	
	err = write(fd, buf, BUFSIZE);
	if (err < 0) perror("buffered write failed");

	/* 1 now on disk, but 2 data is buffered */

	printf("truncate file\n");
	err = ftruncate(fd, TRUNCSIZE);
	if (err < 0) perror("ftruncate failed");
	starttime = time(NULL);

	printf("sync buffered data (2's)\n");
	err = fdatasync(fd);
	if (err < 0) perror("fdatasync failed");

	/* during truncate server may have read/modified/written last block */

	close(fd);

	fd = open(filename, O_CREAT|O_RDWR|O_DIRECT, 0666);
	if (fd < 0) perror("direct open failed");

	/* read what's really on disk now */

	printf("iterate direct reads for %ds or until failure...\n", WAITTIME);

	while ((elapsed = (time(NULL) - starttime)) <= WAITTIME) {

		/* printf(".");
		fflush(stdout);*/

		err = lseek(fd, 0, SEEK_SET);
		if (err < 0) perror("lseek failed");

		err = read(fd, buf, BUFSIZE);
		if (err < 0) perror("read failed");

		err = memcmp(buf, goodbuf, 100);
		if (err) {
			printf("\nFailed after %d secs: read %d's\n", elapsed, buf[0]); 
			return 1;
		}

		sleep(1);
	}
	
	free(buf);
	free(goodbuf);

	printf("Passed\n");
	return 0;
}


