// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 IBM.
 * All Rights Reserved.
 *
 * simple mmap write multithreaded test for testing enospc
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>

#define pr_debug(fmt, ...) do { \
    if (verbose) { \
        printf (fmt, ##__VA_ARGS__); \
    } \
} while (0)

struct thread_s {
	int id;
};

static float file_ratio[2] = {0.5, 0.5};
static unsigned long fsize = 0;
static char *base_dir = ".";
static char *ratio = "1";

static bool use_fallocate = false;
static bool verbose = false;

pthread_barrier_t bar;

void handle_sigbus(int sig)
{
	pr_debug("Enospc test failed with SIGBUS\n");
	exit(7);
}

void enospc_test(int id)
{
	int fd;
	char fpath[255] = {0};
	char *addr;
	unsigned long size = 0;

	/*
	 * Comma separated values against -r option indicates that the file
	 * should be divided into two small files.
	 * The file_ratio specifies the proportion in which the file sizes must
	 * be divided into.
	 *
	 * Half of the files will be divided into size of the first ratio and the
	 * rest of the following ratio
	 */

	if (id % 2 == 0)
		size = fsize * file_ratio[0];
	else
		size = fsize * file_ratio[1];

	pthread_barrier_wait(&bar);

	sprintf(fpath, "%s/mmap-file-%d", base_dir, id);
	pr_debug("Test write phase starting file %s fsize %lu, id %d\n", fpath, size, id);

	signal(SIGBUS, handle_sigbus);

	fd = open(fpath, O_RDWR | O_CREAT, 0644);
	if (fd < 0) {
		pr_debug("Open failed\n");
		exit(1);
	}

	if (use_fallocate)
		assert(fallocate(fd, 0, 0, size) == 0);
	else
		assert(ftruncate(fd, size) == 0);

	addr = mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
	assert(addr != MAP_FAILED);

	for (int i = 0; i < size; i++) {
		addr[i] = 0xAB;
	}

	assert(munmap(addr, size) != -1);
	close(fd);
	pr_debug("Test write phase completed...file %s, fsize %lu, id %d\n", fpath, size, id);
}

void *spawn_test_thread(void *arg)
{
	struct thread_s *thread_info = (struct thread_s *)arg;

	enospc_test(thread_info->id);
	return NULL;
}

void _run_test(int threads)
{
	pthread_t tid[threads];

	pthread_barrier_init(&bar, NULL, threads+1);
	for (int i = 0; i < threads; i++) {
		struct thread_s *thread_info = (struct thread_s *) malloc(sizeof(struct thread_s));
		thread_info->id = i;
		assert(pthread_create(&tid[i], NULL, spawn_test_thread, thread_info) == 0);
	}

	pthread_barrier_wait(&bar);

	for (int i = 0; i <threads; i++) {
		assert(pthread_join(tid[i], NULL) == 0);
	}

	pthread_barrier_destroy(&bar);
	return;
}

static void usage(void)
{
	printf("\nUsage: t_enospc [options]\n\n"
	       " -t                    thread count\n"
	       " -s size               file size\n"
	       " -p path               set base path\n"
	       " -f fallocate          use fallocate instead of ftruncate\n"
	       " -v verbose            print debug information\n"
	       " -r ratio              ratio of file sizes, ',' separated values (def: 0.5,0.5)\n");
}

int main(int argc, char *argv[])
{
	int opt;
	int threads = 0;
	char *ratio_ptr;

	while ((opt = getopt(argc, argv, ":r:p:t:s:vf")) != -1) {
		switch(opt) {
		case 't':
			threads = atoi(optarg);
			pr_debug("Testing with (%d) threads\n", threads);
			break;
		case 's':
			fsize = atoi(optarg);
			pr_debug("size: %lu Bytes\n", fsize);
			break;
		case 'p':
			base_dir = optarg;
			break;
		case 'r':
			ratio = optarg;
			ratio_ptr = strtok(ratio, ",");
			if (ratio_ptr[0] == '1') {
				file_ratio[0] = 1;
				file_ratio[1] = 1;
				break;
			}
			if (ratio_ptr != NULL)
				file_ratio[0] = strtod(ratio_ptr, NULL);
			ratio_ptr = strtok(NULL, ",");
			if (ratio_ptr != NULL)
				file_ratio[1] = strtod(ratio_ptr, NULL);
			break;
		case 'f':
			use_fallocate = true;
			break;
		case 'v':
			verbose = true;
			break;
		case '?':
			usage();
			exit(1);
		}
	}

	/* Sanity test of parameters */
	assert(threads);
	assert(fsize);
	assert(file_ratio[0]);
	assert(file_ratio[1]);

	pr_debug("Testing with (%d) threads\n", threads);
	pr_debug("size: %lu Bytes\n", fsize);

	_run_test(threads);
	return 0;
}
