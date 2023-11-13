// SPDX-License-Identifier: GPL-2.0
/*
 * Race reads with reflink to see if reads continue while we're cloning.
 * Copyright 2023 Oracle.  All rights reserved.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

static pid_t mypid;

static FILE *outcome_fp;

/* Significant timestamps.  Initialized to zero */
static double program_start, clone_start, clone_end;

/* Coordinates access to timestamps */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct readinfo {
	int fd;
	unsigned int blocksize;
	char *descr;
};

struct cloneinfo {
	int src_fd, dst_fd;
};

#define MSEC_PER_SEC	1000
#define NSEC_PER_MSEC	1000000

/*
 * Assume that it shouldn't take longer than 100ms for the FICLONE ioctl to
 * enter the kernel and take locks on an uncontended file.  This is also the
 * required CLOCK_MONOTONIC granularity.
 */
#define EARLIEST_READ_MS	(MSEC_PER_SEC / 10)

/*
 * We want to be reasonably sure that the FICLONE takes long enough that any
 * read initiated after clone operation locked the source file could have
 * completed a disk IO before the clone finishes.  Therefore, we require that
 * the clone operation must take at least 4x the maximum setup time.
 */
#define MINIMUM_CLONE_MS	(EARLIEST_READ_MS * 5)

static double timespec_to_msec(const struct timespec *ts)
{
	return (ts->tv_sec * (double)MSEC_PER_SEC) +
	       (ts->tv_nsec / (double)NSEC_PER_MSEC);
}

static void sleep_ms(unsigned int len)
{
	struct timespec time = {
		.tv_nsec = len * NSEC_PER_MSEC,
	};

	nanosleep(&time, NULL);
}

static void outcome(const char *str)
{
	fprintf(outcome_fp, "%s\n", str);
	fflush(outcome_fp);
}

static void *reader_fn(void *arg)
{
	struct timespec now;
	struct readinfo *ri = arg;
	char *buf = malloc(ri->blocksize);
	loff_t pos = 0;
	ssize_t copied;
	int ret;

	if (!buf) {
		perror("alloc buffer");
		goto kill_error;
	}

	/* Wait for the FICLONE to start */
	pthread_mutex_lock(&lock);
	while (clone_start < program_start) {
		pthread_mutex_unlock(&lock);
#ifdef DEBUG
		printf("%s read waiting for clone to start; cs=%.2f ps=%.2f\n",
				ri->descr, clone_start, program_start);
		fflush(stdout);
#endif
		sleep_ms(1);
		pthread_mutex_lock(&lock);
	}
	pthread_mutex_unlock(&lock);
	sleep_ms(EARLIEST_READ_MS);

	/* Read from the file... */
	while ((copied = read(ri->fd, buf, ri->blocksize)) > 0) {
		double read_completion;

		ret = clock_gettime(CLOCK_MONOTONIC, &now);
		if (ret) {
			perror("clock_gettime");
			goto kill_error;
		}
		read_completion = timespec_to_msec(&now);

		/*
		 * If clone_end is still zero, the FICLONE is still running.
		 * If the read completion occurred a safe duration after the
		 * start of the ioctl, then report that as an early read
		 * completion.
		 */
		pthread_mutex_lock(&lock);
		if (clone_end < program_start &&
		    read_completion > clone_start + EARLIEST_READ_MS) {
			pthread_mutex_unlock(&lock);

			/* clone still running... */
			printf("finished %s read early at %.2fms\n",
					ri->descr,
					read_completion - program_start);
			fflush(stdout);
			outcome("finished read early");
			goto kill_done;
		}
		pthread_mutex_unlock(&lock);

		sleep_ms(1);
		pos += copied;
	}
	if (copied < 0) {
		perror("read");
		goto kill_error;
	}

	return NULL;
kill_error:
	outcome("reader error");
kill_done:
	kill(mypid, SIGTERM);
	return NULL;
}

static void *clone_fn(void *arg)
{
	struct timespec now;
	struct cloneinfo *ci = arg;
	int ret;

	/* Record the approximate start time of this thread */
	ret = clock_gettime(CLOCK_MONOTONIC, &now);
	if (ret) {
		perror("clock_gettime clone start");
		goto kill_error;
	}
	pthread_mutex_lock(&lock);
	clone_start = timespec_to_msec(&now);
	pthread_mutex_unlock(&lock);

	printf("started clone at %.2fms\n", clone_start - program_start);
	fflush(stdout);

	/* Kernel call, only killable with a fatal signal */
	ret = ioctl(ci->dst_fd, FICLONE, ci->src_fd);
	if (ret) {
		perror("FICLONE");
		goto kill_error;
	}

	/* If the ioctl completes, note the completion time */
	ret = clock_gettime(CLOCK_MONOTONIC, &now);
	if (ret) {
		perror("clock_gettime clone end");
		goto kill_error;
	}

	pthread_mutex_lock(&lock);
	clone_end = timespec_to_msec(&now);
	pthread_mutex_unlock(&lock);

	printf("finished clone at %.2fms\n",
			clone_end - program_start);
	fflush(stdout);

	/* Complain if we didn't take long enough to clone. */
	if (clone_end < clone_start + MINIMUM_CLONE_MS) {
		printf("clone did not take enough time (%.2fms)\n",
				clone_end - clone_start);
		fflush(stdout);
		outcome("clone too fast");
		goto kill_done;
	}

	outcome("clone finished before any reads");
	goto kill_done;

kill_error:
	outcome("clone error");
kill_done:
	kill(mypid, SIGTERM);
	return NULL;
}

int main(int argc, char *argv[])
{
	struct cloneinfo ci;
	struct readinfo bufio = { .descr = "buffered" };
	struct readinfo directio = { .descr = "directio" };
	struct timespec now;
	pthread_t clone_thread, bufio_thread, directio_thread;
	double clockres;
	int ret;

	if (argc != 4) {
		printf("Usage: %s src_file dst_file outcome_file\n", argv[0]);
		return 1;
	}

	mypid = getpid();

	/* Check for sufficient clock precision. */
	ret = clock_getres(CLOCK_MONOTONIC, &now);
	if (ret) {
		perror("clock_getres MONOTONIC");
		return 2;
	}
	printf("CLOCK_MONOTONIC resolution: %llus %luns\n",
			(unsigned long long)now.tv_sec,
			(unsigned long)now.tv_nsec);
	fflush(stdout);

	clockres = timespec_to_msec(&now);
	if (clockres > EARLIEST_READ_MS) {
		fprintf(stderr, "insufficient CLOCK_MONOTONIC resolution\n");
		return 2;
	}

	/*
	 * Measure program start time since the MONOTONIC time base is not
	 * all that well defined.
	 */
	ret = clock_gettime(CLOCK_MONOTONIC, &now);
	if (ret) {
		perror("clock_gettime MONOTONIC");
		return 2;
	}
	if (now.tv_sec == 0 && now.tv_nsec == 0) {
		fprintf(stderr, "Uhoh, start time is zero?!\n");
		return 2;
	}
	program_start = timespec_to_msec(&now);

	outcome_fp = fopen(argv[3], "w");
	if (!outcome_fp) {
		perror(argv[3]);
		return 2;
	}

	/* Open separate fds for each thread */
	ci.src_fd = open(argv[1], O_RDONLY);
	if (ci.src_fd < 0) {
		perror(argv[1]);
		return 2;
	}

	ci.dst_fd = open(argv[2], O_RDWR | O_CREAT, 0600);
	if (ci.dst_fd < 0) {
		perror(argv[2]);
		return 2;
	}

	bufio.fd = open(argv[1], O_RDONLY);
	if (bufio.fd < 0) {
		perror(argv[1]);
		return 2;
	}
	bufio.blocksize = 37;

	directio.fd = open(argv[1], O_RDONLY | O_DIRECT);
	if (directio.fd < 0) {
		perror(argv[1]);
		return 2;
	}
	directio.blocksize = 512;

	/* Start threads */
	ret = pthread_create(&clone_thread, NULL, clone_fn, &ci);
	if (ret) {
		fprintf(stderr, "create clone thread: %s\n", strerror(ret));
		return 2;
	}

	ret = pthread_create(&bufio_thread, NULL, reader_fn, &bufio);
	if (ret) {
		fprintf(stderr, "create bufio thread: %s\n", strerror(ret));
		return 2;
	}

	ret = pthread_create(&directio_thread, NULL, reader_fn, &directio);
	if (ret) {
		fprintf(stderr, "create directio thread: %s\n", strerror(ret));
		return 2;
	}

	/* Wait for threads */
	ret = pthread_join(clone_thread, NULL);
	if (ret) {
		fprintf(stderr, "join clone thread: %s\n", strerror(ret));
		return 2;
	}

	ret = pthread_join(bufio_thread, NULL);
	if (ret) {
		fprintf(stderr, "join bufio thread: %s\n", strerror(ret));
		return 2;
	}

	ret = pthread_join(directio_thread, NULL);
	if (ret) {
		fprintf(stderr, "join directio thread: %s\n", strerror(ret));
		return 2;
	}

	printf("Program should have killed itself?\n");
	outcome("program failed to end early");
	return 0;
}
