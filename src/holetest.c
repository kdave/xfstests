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
#include <getopt.h>
#include <sys/wait.h>

#define THREADS 2

long page_size;
long page_offs[THREADS];
int use_wr[THREADS];
int prefault = 0;
int use_fork = 0;
int use_private = 0;

uint64_t get_id(void)
{
	if (!use_fork)
		return (uint64_t) pthread_self();
	return getpid();
}

void prefault_mapping(char *addr, long npages)
{
	long i;

	for (i = 0; i < npages; i++) {
		if (addr[i * page_size] != 0) {
			fprintf(stderr, "Prefaulting found non-zero value in "
				"page %ld: %d\n", i, (int)addr[i * page_size]);
		}
	}
}

int verify_mapping(char *vastart, long npages, uint64_t *expect)
{
	int errcnt = 0;
	int i;
	char *va;

	for (va = vastart; npages > 0; va += page_size, npages--) {
		for (i = 0; i < THREADS; i++) {
			if (*(uint64_t*)(va + page_offs[i]) != expect[i]) {
				printf("ERROR: thread %d, "
				       "offset %08llx, %08llx != %08llx\n", i,
				       (unsigned long long) (va + page_offs[i] - vastart),
				       (unsigned long long) *(uint64_t*)(va + page_offs[i]),
				       (unsigned long long) expect[i]);
				errcnt++;
			}
		}
	}
	return errcnt;
}

void *pt_page_marker(void *args)
{
	void **a = args;
	char *va = (char *)a[1];
	long npages = (long)a[2];
	long i;
	long pgoff = page_offs[(long)a[3]];
	uint64_t tid = get_id();
	long errors = 0;

	if (prefault && use_fork)
		prefault_mapping(va, npages);

	/* mark pages */
	for (i = 0; i < npages; i++)
		*(uint64_t *)(va + pgoff + i * page_size) = tid;

	if (use_private && use_fork) {
		uint64_t expect[THREADS] = {};

		expect[(long)a[3]] = tid;
		errors = verify_mapping(va, npages, expect);
	}

	return (void *)errors;
}  /* pt_page_marker() */

void *pt_write_marker(void *args)
{
	void **a = args;
	int fd = (long)a[0];
	long npages = (long)a[2];
	long pgoff = page_offs[(long)a[3]];
	uint64_t tid = get_id();
	long i;

	/* mark pages */
	for (i = 0; i < npages; i++)
		pwrite(fd, &tid, sizeof(tid), i * page_size + pgoff);

	return NULL;
}

int test_this(int fd, loff_t sz)
{
	long npages;
	char *vastart;
	void *targs[THREADS][4];
	pthread_t t[THREADS];
	uint64_t tid[THREADS];
	int errcnt = 0;
	int i;

	npages = sz / page_size;
	printf("INFO: sz = %llu\n", (unsigned long long)sz);

	/* mmap it */
	vastart = mmap(NULL, sz, PROT_READ | PROT_WRITE,
		       use_private ? MAP_PRIVATE : MAP_SHARED, fd, 0);
	if (MAP_FAILED == vastart) {
		perror("mmap()");
		exit(20);
	}

	if (prefault && !use_fork)
		prefault_mapping(vastart, npages);

	/* prepare the thread args */
	for (i = 0; i < THREADS; i++) {
		targs[i][0] = (void *)(long)fd;
		targs[i][1] = vastart;
		targs[i][2] = (void *)npages;
		targs[i][3] = (void *)(long)i;
	}

	for (i = 0; i < THREADS; i++) {
		if (!use_fork) {
			/* start two threads */
			if (pthread_create(&t[i], NULL,
				   use_wr[i] ? pt_write_marker : pt_page_marker,
				   &targs[i])) {
				perror("pthread_create");
				exit(21);
			}
			tid[i] = (uint64_t)t[i];
			printf("INFO: thread %d created\n", i);
		} else {
			/*
			 * Flush stdout before fork, otherwise some lines get
			 * duplicated... ?!?!?
			 */
			fflush(stdout);
			tid[i] = fork();
			if (tid[i] < 0) {
				int j;

				perror("fork");
				for (j = 0; j < i; j++)
					waitpid(tid[j], NULL, 0);
				exit(21);
			}
			/* Child? */
			if (!tid[i]) {
				void *ret;

				if (use_wr[i])
					ret = pt_write_marker(&targs[i]);
				else
					ret = pt_page_marker(&targs[i]);
				exit(ret ? 1 : 0);
			}
			printf("INFO: process %d created\n", i);
		}
	}

	/* wait for them to finish */
	for (i = 0; i < THREADS; i++) {
		if (!use_fork) {
			void *status;

			pthread_join(t[i], &status);
			if (status)
				errcnt++;
		} else {
			int status;

			waitpid(tid[i], &status, 0);
			if (!WIFEXITED(status) || WEXITSTATUS(status) > 0)
				errcnt++;
		}
	}

	/* check markers on each page */
	/* For private mappings & fork we should see no writes happen */
	if (use_private && use_fork)
		for (i = 0; i < THREADS; i++)
			tid[i] = 0;
	errcnt = verify_mapping(vastart, npages, tid);
	munmap(vastart, sz);

	if (use_private) {
		/* Check that no writes propagated into original file */
		for (i = 0; i < THREADS; i++)
			tid[i] = 0;
		vastart = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fd, 0);
		if (vastart == MAP_FAILED) {
			perror("mmap()");
			exit(20);
		}
		errcnt += verify_mapping(vastart, npages, tid);
		munmap(vastart, sz);
	}

	printf("INFO: %d error(s) detected\n", errcnt);


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
	int opt;

	page_size = getpagesize();
	step = page_size / THREADS;
	page_offs[0] = step / 2;
	for (i = 1; i < THREADS; i++)
		page_offs[i] = page_offs[i-1] + step;

	while ((opt = getopt(argc, argv, "fwrFp")) > 0) {
		switch (opt) {
		case 'f':
			/* ignore errors */
			stoponerror = 0;
			break;
		case 'w':
			/* use writes instead of mmap for one thread */
			use_wr[0] = 1;
			break;
		case 'r':
			/* prefault mmapped area by reading it */
			prefault = 1;
			break;
		case 'F':
			/* create processes instead of threads */
			use_fork = 1;
			break;
		case 'p':
			/* Use private mappings for testing */
			use_private = 1;
			break;
		default:
			fprintf(stderr, "ERROR: Unknown option character.\n");
			exit(1);
		}
	}

	if (optind != argc - 2) {
		fprintf(stderr, "ERROR: usage: holetest [-fwrFp] "
			"FILENAME FILESIZEinMB\n");
		exit(1);
	}
	if (use_private && use_wr[0]) {
		fprintf(stderr, "ERROR: Combinations of writes and private"
			"mappings not supported.\n");
		exit(1);
	}

	path = argv[optind];
	sz = strtol(argv[optind + 1], &endch, 10);
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
