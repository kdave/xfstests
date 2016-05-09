/*
 * holetest -- test simultaneous page faults on hole-backed pages
 * Copyright (C) 2015  Hewlett Packard Enterprise Development LP
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


/*
 * holetest
 *
 * gcc -Wall -pthread -o holetest holetest.c
 *
 * This test tool exercises page faults on hole-y portions of an mmapped
 * file.  The file is created, sized using various methods, mmapped, and
 * then two threads race to write a marker to different offsets within
 * each mapped page.  Once the threads have finished marking each page,
 * the pages are checked for the presence of the markers.
 *
 * The file is sized four different ways: explicitly zero-filled by the
 * test, posix_fallocate(), fallocate(), and ftruncate().  The explicit
 * zero-fill does not really test simultaneous page faults on hole-backed
 * pages, but rather serves as control of sorts.
 *
 * Usage:
 *
 *   holetest [-f] FILENAME FILESIZEinMB
 *
 * Where:
 *
 *   FILENAME is the name of a non-existent test file to create
 *
 *   FILESIZEinMB is the desired size of the test file in MiB
 *
 * If the test is successful, FILENAME will be unlinked.  By default,
 * if the test detects an error in the page markers, then the test exits
 * immediately and FILENAME is left.  If -f is given, then the test
 * continues after a marker error and FILENAME is unlinked, but will
 * still exit with a non-0 status.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define THREADS 2

long page_size;
long page_offs[THREADS];

void *pt_page_marker(void *args)
{
	void **a = args;
	char *va = (char *)a[0];
	long npages = (long)a[1];
	long pgoff = (long)a[2];
	uint64_t tid = (uint64_t)pthread_self();

	va += pgoff;

	/* mark pages */
	for (; npages > 0; va += page_size, npages--)
		*(uint64_t *)(va) = tid;

	return NULL;
}  /* pt_page_marker() */


int test_this(int fd, loff_t sz)
{
	long npages;
	char *vastart;
	char *va;
	void *targs[THREADS][3];
	pthread_t t[THREADS];
	uint64_t tid[THREADS];
	int errcnt;
	int i;

	npages = sz / page_size;
	printf("INFO: sz = %llu\n", (unsigned long long)sz);

	/* mmap it */
	vastart = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (MAP_FAILED == vastart) {
		perror("mmap()");
		exit(20);
	}

	/* prepare the thread args */
	for (i = 0; i < THREADS; i++) {
		targs[i][0] = vastart;
		targs[i][1] = (void *)npages;
		targs[i][2] = (void *)page_offs[i];
	}

	for (i = 0; i < THREADS; i++) {
		/* start two threads */
		if (pthread_create(&t[i], NULL, pt_page_marker, &targs[i])) {
			perror("pthread_create");
			exit(21);
		}
		tid[i] = (uint64_t)t[i];
		printf("INFO: thread %d created\n", i);
	}

	/* wait for them to finish */
	for (i = 0; i < THREADS; i++)
		pthread_join(t[i], NULL);

	/* check markers on each page */
	errcnt = 0;
	for (va = vastart; npages > 0; va += page_size, npages--) {
		for (i = 0; i < THREADS; i++) {
			if (*(uint64_t*)(va + page_offs[i]) != tid[i]) {
				printf("ERROR: thread %d, "
				       "offset %08lx, %08lx != %08lx\n", i,
				       (va + page_offs[i] - vastart),
				       *(uint64_t*)(va + page_offs[i]), tid[i]);
				errcnt += 1;
			}
		}
	}

	printf("INFO: %d error(s) detected\n", errcnt);

	munmap(vastart, sz);

	return errcnt;
}

int main(int argc, char **argv)
{
	int stoponerror = 1;
	char *path;
	loff_t sz;
	int fd;
	int errcnt;
	int toterr = 0;
	int i, step;
	char *endch;

	page_size = getpagesize();
	step = page_size / THREADS;
	page_offs[0] = step / 2;
	for (i = 1; i < THREADS; i++)
		page_offs[i] = page_offs[i-1] + step;

	/* process command line */
	argc--; argv++;
	/* ignore errors? */
	if ((argc == 3) && !strcmp(argv[0], "-f")) {
		stoponerror = 0;
		argc--;
		argv++;
	}
	/* file name and size */
	if (argc != 2 || argv[0][0] == '-') {
		fprintf(stderr, "ERROR: usage: holetest [-f] "
			"FILENAME FILESIZEinMB\n");
		exit(1);
	}
	path = argv[0];
	sz = strtol(argv[1], &endch, 10);
	if (*endch || sz < 1) {
		fprintf(stderr, "ERROR: bad FILESIZEinMB\n");
		exit(1);
	}
	sz <<= 20;

	/*
	 * we're going to run our test in several different ways:
	 *
	 * 1. explictly zero-filled
	 * 2. posix_fallocated
	 * 3. ftruncated
	 */


	/*
	 * explicitly zero-filled
	 */
	printf("\nINFO: zero-filled test...\n");

	/* create the file */
	fd = open(path, O_RDWR | O_EXCL | O_CREAT, 0644);
	if (fd < 0) {
		perror(path);
		exit(2);
	}

	/* truncate it to size */
	if (ftruncate(fd, sz)) {
		perror("ftruncate()");
		exit(3);
	}

	/* explicitly zero-fill */
	{
		char*   va = mmap(NULL, sz, PROT_READ | PROT_WRITE,
				  MAP_SHARED, fd, 0);
		if (MAP_FAILED == va) {
			perror("mmap()");
			exit(4);
		}
		memset(va, 0, sz);
		munmap(va, sz);
	}

	/* test it */
	errcnt = test_this(fd, sz);
	toterr += errcnt;
	close(fd);
	if (stoponerror && errcnt > 0)
		exit(5);

	/* cleanup */
	if (unlink(path)) {
		perror("unlink()");
		exit(6);
	}


	/*
	 * posix_fallocated
	 */
	printf("\nINFO: posix_fallocate test...\n");

	/* create the file */
	fd = open(path, O_RDWR | O_EXCL | O_CREAT, 0644);
	if (fd < 0) {
		perror(path);
		exit(7);
	}

	/* fill it to size */
	if (posix_fallocate(fd, 0, sz)) {
		perror("posix_fallocate()");
		exit(8);
	}

	/* test it */
	errcnt = test_this(fd, sz);
	toterr += errcnt;
	close(fd);
	if (stoponerror && errcnt > 0)
		exit(9);

	/* cleanup */
	if (unlink(path)) {
		perror("unlink()");
		exit(10);
	}

	/*
	 * ftruncated
	 */
	printf("\nINFO: ftruncate test...\n");

	/* create the file */
	fd = open(path, O_RDWR | O_EXCL | O_CREAT, 0644);
	if (fd < 0) {
		perror(path);
		exit(15);
	}

	/* truncate it to size */
	if (ftruncate(fd, sz)) {
		perror("ftruncate()");
		exit(16);
	}

	/* test it */
	errcnt = test_this(fd, sz);
	toterr += errcnt;
	close(fd);
	if (stoponerror && errcnt > 0)
		exit(17);

	/* cleanup */
	if (unlink(path)) {
		perror("unlink()");
		exit(18);
	}

	/* done */
	if (toterr > 0)
		exit(19);
	return 0;
}
