// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 Intel Corporation.
 *
 * As of kernel version 4.18-rc6 Linux has an issue with ext4+DAX where DMA
 * and direct I/O operations aren't synchronized with respect to operations
 * which can change the block mappings of an inode.  This means that we can
 * schedule an I/O for an inode and have the block mapping for that inode
 * change before the I/O is actually complete.  So, blocks which were once
 * allocated to a given inode and then freed could still have I/O operations
 * happening to them.  If these blocks have also been reallocated to a
 * different inode, this interaction can lead to data corruption.
 *
 * This test exercises four of the paths in ext4 which hit this issue.
 */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PAGE(a) ((a)*0x1000)
#define FILE_SIZE PAGE(4)

void *dax_data;
int nodax_fd;
int dax_fd;
bool done;

#define err_exit(op)                                                          \
{                                                                             \
	fprintf(stderr, "%s %s: %s\n", __func__, op, strerror(errno));        \
	exit(1);                                                              \
}

#if defined(FALLOC_FL_PUNCH_HOLE) && defined(FALLOC_FL_KEEP_SIZE)
void punch_hole_fn(void *ptr)
{
	ssize_t read;
	int rc;

	while (!done) {
		read = 0;

		do {
			rc = pread(nodax_fd, dax_data + read, FILE_SIZE - read,
					read);
			if (rc > 0)
				read += rc;
		} while (rc > 0);

		if (read != FILE_SIZE || rc != 0)
			err_exit("pread");

		rc = fallocate(dax_fd,
				FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
				0, FILE_SIZE);
		if (rc < 0)
			err_exit("fallocate");

		usleep(rand() % 1000);
	}
}
#else
void punch_hole_fn(void *ptr) { }
#endif

#if defined(FALLOC_FL_ZERO_RANGE) && defined(FALLOC_FL_KEEP_SIZE)
void zero_range_fn(void *ptr)
{
	ssize_t read;
	int rc;

	while (!done) {
		read = 0;

		do {
			rc = pread(nodax_fd, dax_data + read, FILE_SIZE - read,
					read);
			if (rc > 0)
				read += rc;
		} while (rc > 0);

		if (read != FILE_SIZE || rc != 0)
			err_exit("pread");

		rc = fallocate(dax_fd,
				FALLOC_FL_ZERO_RANGE | FALLOC_FL_KEEP_SIZE,
				0, FILE_SIZE);
		if (rc < 0)
			err_exit("fallocate");

		usleep(rand() % 1000);
	}
}
#else
void zero_range_fn(void *ptr) { }
#endif

void truncate_down_fn(void *ptr)
{
	ssize_t read;
	int rc;

	while (!done) {
		read = 0;

		if (ftruncate(dax_fd, 0) < 0)
			err_exit("ftruncate");
		if (fallocate(dax_fd, 0, 0, FILE_SIZE) < 0)
			err_exit("fallocate");

		do {
			rc = pread(nodax_fd, dax_data + read, FILE_SIZE - read,
					read);
			if (rc > 0)
				read += rc;
		} while (rc > 0);

		/*
		 * For this test we ignore errors from pread().  These errors
		 * can happen if we try and read while the other thread has
		 * made the file size 0.
		 */

		usleep(rand() % 1000);
	}
}

#ifdef FALLOC_FL_COLLAPSE_RANGE
void collapse_range_fn(void *ptr)
{
	ssize_t read;
	int rc;

	while (!done) {
		read = 0;

		if (fallocate(dax_fd, 0, 0, FILE_SIZE) < 0)
			err_exit("fallocate 1");
		if (fallocate(dax_fd, FALLOC_FL_COLLAPSE_RANGE, 0, PAGE(1)) < 0)
			err_exit("fallocate 2");
		if (fallocate(dax_fd, 0, 0, FILE_SIZE) < 0)
			err_exit("fallocate 3");

		do {
			rc = pread(nodax_fd, dax_data + read, FILE_SIZE - read,
					read);
			if (rc > 0)
				read += rc;
		} while (rc > 0);

		/* For this test we ignore errors from pread. */

		usleep(rand() % 1000);
	}
}
#else
void collapse_range_fn(void *ptr) { }
#endif

void run_test(void (*test_fn)(void *))
{
	const int NUM_THREADS = 2;
	pthread_t worker_thread[NUM_THREADS];
	int i;

	done = 0;
	for (i = 0; i < NUM_THREADS; i++)
		pthread_create(&worker_thread[i], NULL, (void*)test_fn, NULL);

	sleep(1);
	done = 1;

	for (i = 0; i < NUM_THREADS; i++)
		pthread_join(worker_thread[i], NULL);
}

int main(int argc, char *argv[])
{
	int err;

	if (argc != 3) {
		printf("Usage: %s <dax file> <non-dax file>\n",
				basename(argv[0]));
		exit(0);
	}

	dax_fd = open(argv[1], O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
	if (dax_fd < 0)
		err_exit("dax_fd open");

	nodax_fd = open(argv[2], O_RDWR|O_CREAT|O_DIRECT, S_IRUSR|S_IWUSR);
	if (nodax_fd < 0)
		err_exit("nodax_fd open");

	if (ftruncate(dax_fd, 0) < 0)
		err_exit("dax_fd ftruncate");
	if (fallocate(dax_fd, 0, 0, FILE_SIZE) < 0)
		err_exit("dax_fd fallocate");

	if (ftruncate(nodax_fd, 0) < 0)
		err_exit("nodax_fd ftruncate");
	if (fallocate(nodax_fd, 0, 0, FILE_SIZE) < 0)
		err_exit("nodax_fd fallocate");

	dax_data = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED,
			dax_fd, 0);
	if (dax_data == MAP_FAILED)
		err_exit("mmap");

	run_test(&punch_hole_fn);
	run_test(&zero_range_fn);
	run_test(&truncate_down_fn);
	run_test(&collapse_range_fn);

	if (munmap(dax_data, FILE_SIZE) != 0)
		err_exit("munmap");

	err = close(dax_fd);
	if (err < 0)
		err_exit("dax_fd close");

	err = close(nodax_fd);
	if (err < 0)
		err_exit("nodax_fd close");

	return 0;
}
