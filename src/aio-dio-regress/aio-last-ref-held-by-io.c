/* Copyright (C) 2010, Matthew E. Cross <matt.cross@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Code to reproduce the aio lockup.
 *
 * Make a test file that is at least 4MB long.  Something like this:
 * 'dd if=/dev/zero of=/tmp/testfile bs=1M count=10'
 *
 * Run this test as './aio_test 0 100 /tmp/testfile' to induce the
 * failure.
 *
 * Run this test as './aio_test 1 100 /tmp/testfile' to demonstrate an
 * incomplete workaround (close fd, then wait for all io to complete
 * on an io context before calling io_destroy()).  This still induces
 * the failure.
 *
 * This test was written several years ago by Matt Cross, and he has
 * graciously allowed me to post it for inclusion in xfstests.
 *
 * Changelog
 * - reduce output and make it consistent for integration into xfstests (JEM)
 * - run for fixed amount of time instead of indefinitely (JEM)
 * - change coding style to meet xfstests standards (JEM)
 * - get rid of unused code (workaround 2 documented above) (JEM)
 * - use posix_memalign (JEM)
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* to get definition of O_DIRECT flag. */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <pthread.h>
#include <unistd.h>
#include <libaio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sched.h>

#undef DEBUG
#ifdef DEBUG
#define dprintf(fmt, args...) printf(fmt, ##args)
#else
#define dprintf(fmt, args...)
#endif

char *filename;
int wait_for_events = 0;

pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned long total_loop_count = 0;

#define NUM_IOS 16
#define IOSIZE (1024 * 64)

pid_t
gettid(void)
{
	return (pid_t)syscall(SYS_gettid);
}

void *
aio_test_thread(void *data)
{
	int fd = -1;
	io_context_t ioctx;
	int ioctx_initted;
	int ios_submitted;
	struct iocb iocbs[NUM_IOS];
	int i;
	static unsigned char *buffer;
	int ret;
	long mycpu = (long)data;
	pid_t mytid = gettid();
	cpu_set_t cpuset;

	dprintf("setting thread %d to run on cpu %ld\n", mytid, mycpu);

	/*
	 * Problems have been easier to trigger when spreading the
	 * workload over the available CPUs.
	 */
	CPU_ZERO(&cpuset);
	CPU_SET(mycpu, &cpuset);
	if (sched_setaffinity(mytid, sizeof(cpuset), &cpuset)) {
		printf("FAILED to set thread %d to run on cpu %ld\n",
		       mytid, mycpu);
	}

	ioctx_initted = 0;
	ios_submitted = 0;

	ret = posix_memalign((void **)&buffer, getpagesize(), IOSIZE * NUM_IOS);
	if (ret != 0) {
		printf("%lu: Failed to allocate buffer for IO: %d\n",
		       pthread_self(), ret);
		goto done;
	}

	while (1) {
		fd = open(filename, O_RDONLY | O_DIRECT);
		if (fd < 0) {
			printf("%lu: Failed to open file '%s'\n",
			       pthread_self(), filename);
			goto done;
		}

		memset(&ioctx, 0, sizeof(ioctx));
		if (io_setup(NUM_IOS, &ioctx)) {
			printf("%lu: Failed to setup io context\n",
			       pthread_self());
			goto done;
		}
		ioctx_initted = 1;

		if (mycpu != 0) {
			for (i = 0; i < NUM_IOS; i++) {
				struct iocb *iocb = &iocbs[i];

				memset(iocb, 0, sizeof(*iocb));
				io_prep_pread(iocb, fd, (buffer + i * IOSIZE),
					      IOSIZE, i * IOSIZE);
				if (io_submit(ioctx, 1, &iocb) != 1) {
					printf("%lu: failed to submit io #%d\n",
						pthread_self(), i+1);
				}
			}
			ios_submitted = 1;
		}

done:
		if (fd >= 0)
			close(fd);

		if (wait_for_events && ios_submitted) {
			struct io_event io_events[NUM_IOS];

			if (io_getevents(ioctx, NUM_IOS, NUM_IOS,
					 io_events, NULL) != NUM_IOS)
				printf("io_getevents failed to wait for all IO\n");
		}

		if (ioctx_initted) {
			io_destroy(ioctx);
			ioctx_initted = 0;
		}

		if (ios_submitted) {
			pthread_mutex_lock(&count_mutex);
			total_loop_count++;
			pthread_mutex_unlock(&count_mutex);

			ios_submitted = 0;
		}
	}
}

int
main(int argc, char **argv)
{
	unsigned num_threads;
	unsigned i;
	int fd;
	pthread_t *threads;
	long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	struct timeval start, now, delta = { 0, 0 };

	if (argc != 4) {
		printf("Usage: aio_test [wait for events?] [# of threads] "
		       "[filename]\n");
		return -1;
	}

	wait_for_events = strtoul(argv[1], NULL, 0);
	num_threads = strtoul(argv[2], NULL, 0);
	filename = argv[3];

	printf("wait_for_events: %d\n", wait_for_events);
	printf("num_threads: %u\n", num_threads);
	printf("filename: '%s'\n", basename(filename));

	if (num_threads < 1) {
		printf("Number of threads is invalid, must be at least 1\n");
		return -1;
	}

	fd = open(filename, O_RDONLY|O_DIRECT);
	if (fd < 0) {
		printf("Failed to open filename '%s' for reading\n", filename);
		return -1;
	}
	close(fd);

	threads = malloc(sizeof(pthread_t) * num_threads);
	if (threads == NULL) {
		printf("Failed to allocate thread id storage\n");
		return -1;
	}

	for (i = 0; i < num_threads; i++) {
		if (pthread_create(&threads[i], NULL,
				   aio_test_thread, (void *)(i % ncpus))) {
			printf("Failed to create thread #%u\n", i+1);
			threads[i] = (pthread_t)-1;
		}
	}

	printf("All threads spawned\n");
	gettimeofday(&start, NULL);

	while (delta.tv_sec < 60) {
		sleep(1);
		gettimeofday(&now, NULL);
		timersub(&now, &start, &delta);
		dprintf("%lu loops completed in %ld seconds\n",
			total_loop_count, delta.tv_sec);
	}

	return 0;
}
