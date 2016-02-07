#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int test_fd;
static char *buf;
static char *fname;
static int openflags = O_RDWR;

/*
 * Just creates a random file, overwriting the file in a random number of loops
 * and fsyncing between each loop.
 */
static int test_one(int *max_blocks)
{
	int loops = (random() % 20) + 5;

	lseek(test_fd, 0, SEEK_SET);
	while (loops--) {
		int character = (random() % 126) + 33; /* printable character */
		int blocks = (random() % 100) + 1;

		if (blocks > *max_blocks)
			*max_blocks = blocks;
		lseek(test_fd, 0, SEEK_SET);
		memset(buf, character, 4096);
		if (fsync(test_fd)) {
			fprintf(stderr, "Fsync failed, test results will be "
				"invalid: %d\n", errno);
			return 1;
		}

		while (blocks--) {
			if (write(test_fd, buf, 4096) < 4096) {
				fprintf(stderr, "Short write %d\n", errno);
				return 1;
			}
		}
	}

	return 0;
}

/*
 * Preallocate a randomly sized file and then overwrite the entire thing and
 * then fsync.
 */
static int test_two(int *max_blocks)
{
	int blocks = (random() % 1024) + 1;
	int character = (random() % 126) + 33;

	*max_blocks = blocks;

	if (fallocate(test_fd, 0, 0, blocks * 4096)) {
		fprintf(stderr, "Error fallocating %d (%s)\n", errno,
			strerror(errno));
		return 1;
	}

	lseek(test_fd, 0, SEEK_SET);
	memset(buf, character, 4096);
	while (blocks--) {
		if (write(test_fd, buf, 4096) < 4096) {
			fprintf(stderr, "Short write %d\n", errno);
			return 1;
		}
	}

	return 0;
}

static void drop_all_caches()
{
	char value[] = "3\n";
	int fd;

	if ((fd = open("/proc/sys/vm/drop_caches", O_WRONLY)) < 0) {
		fprintf(stderr, "Error opening drop caches: %d\n", errno);
		return;
	}

	write(fd, value, sizeof(value)-1);
	close(fd);
}

/*
 * Randomly write inside of a file, either creating a sparse file or prealloc
 * the file and randomly write within it, depending on the prealloc flag
 */
static int test_three(int *max_blocks, int prealloc, int rand_fsync,
		      int do_sync, int drop_caches)
{
	int size = (random() % 2048) + 4;
	int blocks = size / 2;
	int sync_block = blocks / 2;
	int rand_sync_interval = (random() % blocks) + 1;
	int character = (random() % 126) + 33;

	if (prealloc && fallocate(test_fd, 0, 0, size * 4096)) {
		fprintf(stderr, "Error fallocating %d (%s)\n", errno,
			strerror(errno));
		return 1;
	}

	if (prealloc)
		*max_blocks = size;

	memset(buf, character, 4096);
	while (blocks--) {
		int block = (random() % size);

		if ((block + 1) > *max_blocks)
			*max_blocks = block + 1;

		if (rand_fsync && !(blocks % rand_sync_interval)) {
			if (fsync(test_fd)) {
				fprintf(stderr, "Fsync failed, test results "
					"will be invalid: %d\n", errno);
				return 1;
			}
		}

		/* Force a transaction commit in between just for fun */
		if (blocks == sync_block && (do_sync || drop_caches)) {
			if (do_sync)
				sync();
			else
				sync_file_range(test_fd, 0, 0,
						SYNC_FILE_RANGE_WRITE|
						SYNC_FILE_RANGE_WAIT_AFTER);

			if (drop_caches) {
				close(test_fd);
				drop_all_caches();
				test_fd = open(fname, openflags);
				if (test_fd < 0) {
					test_fd = 0;
					fprintf(stderr, "Error re-opening file: %d\n",
						errno);
					return 1;
				}
			}
		}

		if (pwrite(test_fd, buf, 4096, block * 4096) < 4096) {
			fprintf(stderr, "Short write %d\n", errno);
			return 1;
		}
	}

	return 0;
}

static void timeval_subtract(struct timeval *result,struct timeval *x,
			     struct timeval *y)
{
	if (x->tv_usec < y->tv_usec) {
		int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}

	if (x->tv_usec - y->tv_usec > 1000000) {
		int nsec = (x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}

	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;
}

static int test_four(int *max_blocks)
{
	size_t size = 2621440;	/* 10 gigabytes */
	size_t blocks = size / 2;
	size_t sync_block = blocks / 8;	/* fsync 8 times */
	int character = (random() % 126) + 33;
	struct timeval start, end, diff;

	memset(buf, character, 4096);
	while (blocks--) {
		off_t block = (random() % size);

		if ((block + 1) > *max_blocks)
			*max_blocks = block + 1;

		if ((blocks % sync_block) == 0) {
			if (gettimeofday(&start, NULL)) {
				fprintf(stderr, "Error getting time: %d\n",
					errno);
				return 1;
			}
			if (fsync(test_fd)) {
				fprintf(stderr, "Fsync failed, test results "
					"will be invalid: %d\n", errno);
				return 1;
			}
			if (gettimeofday(&end, NULL)) {
				fprintf(stderr, "Error getting time: %d\n",
					errno);
				return 1;
			}
			timeval_subtract(&diff, &end, &start);
			printf("Fsync time was %ds and %dus\n",
			       (int)diff.tv_sec, (int)diff.tv_usec);
		}

		if (pwrite(test_fd, buf, 4096, block * 4096) < 4096) {
			fprintf(stderr, "Short write %d\n", errno);
			return 1;
		}
	}

	return 0;
}

static int test_five()
{
	int character = (random() % 126) + 33;
	int runs = (random() % 100) + 1;
	int i;

	memset(buf, character, 3072);
	for (i = 0; i < runs; i++) {
		ssize_t write_size = (random() % 3072) + 1;

		if (pwrite(test_fd, buf, write_size, 0) < write_size) {
			fprintf(stderr, "Short write %d\n", errno);
			return 1;
		}

		if ((i % 8) == 0) {
			if (fsync(test_fd)) {
				fprintf(stderr, "Fsync failed, test results "
					"will be invalid: %d\n", errno);
				return 1;
			}
		}
	}

	return 0;
}

/*
 * Reproducer for something like this
 *
 * [data][prealloc][data]
 *
 * and then in the [prealloc] section we have
 *
 * [ pre ][pre][     pre     ]
 * [d][pp][dd][ppp][d][ppp][d]
 *
 * where each letter represents on block of either data or prealloc.
 *
 * This explains all the weirdly specific numbers.
 */
static int test_six()
{
	int character = (random() % 126) + 33;
	int i;

	memset(buf, character, 4096);

	/* Write on either side of the file, leaving a hole in the middle */
	for (i = 0; i < 10; i++) {
		if (pwrite(test_fd, buf, 4096, i * 4096) < 4096) {
			fprintf(stderr, "Short write %d\n", errno);
			return 1;
		}
	}

	if (fsync(test_fd)) {
		fprintf(stderr, "Fsync failed %d\n", errno);
		return 1;
	}

	/*
	 * The test fs I had the prealloc extent was 13 4k blocks long so I'm
	 * just using that to give myself the best chances of reproducing.
	 */
	for (i = 23; i < 33; i++) {
		if (pwrite(test_fd, buf, 4096, i * 4096) < 4096) {
			fprintf(stderr, "Short write %d\n", errno);
			return 1;
		}
	}

	if (fsync(test_fd)) {
		fprintf(stderr, "Fsync failed %d\n", errno);
		return 1;
	}

	if (fallocate(test_fd, 0, 10 * 4096, 4 * 4096)) {
		fprintf(stderr, "Error fallocating %d\n", errno);
		return 1;
	}

	if (fallocate(test_fd, 0, 14 * 4096, 5 * 4096)) {
		fprintf(stderr, "Error fallocating %d\n", errno);
		return 1;
	}

	if (fallocate(test_fd, 0, 19 * 4096, 4 * 4096)) {
		fprintf(stderr, "Error fallocating %d\n", errno);
		return 1;
	}

	if (pwrite(test_fd, buf, 4096, 10 * 4096) < 4096) {
		fprintf(stderr, "Short write %d\n", errno);
		return 1;
	}

	if (fsync(test_fd)) {
		fprintf(stderr, "Fsync failed %d\n", errno);
		return 1;
	}

	for (i = 13; i < 15; i++) {
		if (pwrite(test_fd, buf, 4096, i * 4096) < 4096) {
			fprintf(stderr, "Short write %d\n", errno);
			return 1;
		}
	}

	if (fsync(test_fd)) {
		fprintf(stderr, "Fsync failed %d\n", errno);
		return 1;
	}

	if (pwrite(test_fd, buf, 4096, 18 * 4096) < 4096) {
		fprintf(stderr, "Short write %d\n", errno);
		return 1;
	}

	if (fsync(test_fd)) {
		fprintf(stderr, "Fsync failed %d\n", errno);
		return 1;
	}

	if (pwrite(test_fd, buf, 4096, 22 * 4096) < 4096) {
		fprintf(stderr, "Short write %d\n", errno);
		return 1;
	}

	if (fsync(test_fd)) {
		fprintf(stderr, "Fsync failed %d\n", errno);
		return 1;
	}

	return 0;
}

static void usage()
{
	printf("Usage fsync-tester [-s <seed>] [-r] [-d] -t <test-num> <filename>\n");
	printf("   -s seed   : seed for teh random map generator (defaults to reading /dev/urandom)\n");
	printf("   -r        : don't reboot the box immediately\n");
	printf("   -d        : use O_DIRECT\n");
	printf("   -t test   : test nr to run, required\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int opt;
	int fd;
	int max_blocks = 0;
	char *endptr;
	unsigned int seed = 123;
	int reboot = 0;
	int direct_io = 0;
	long int test = 1;
	long int tmp;
	int ret = 0;

	if (argc < 2)
		usage();

	fd = open("/dev/urandom", O_RDONLY);
	if (fd >= 0) {
		read(fd, &seed, sizeof(seed));
		close(fd);
	}

	while ((opt = getopt(argc, argv, "s:rdt:")) != -1) {
		switch (opt) {
		case 's':
			tmp = strtol(optarg, &endptr, 10);
			if (tmp == LONG_MAX || endptr == optarg)
				usage();
			seed = tmp;
			break;
		case 'r':
			reboot = 1;
			break;
		case 'd':
			direct_io = 1;
			break;
		case 't':
			test = strtol(optarg, &endptr, 10);
			if (test == LONG_MAX || endptr == optarg)
				usage();
			break;
		default:
			usage();
		}
	}

	if (optind >= argc)
		usage();

	/*
	 * test 19 is for smaller than blocksize writes to test btrfs's inline
	 * extent fsyncing, so direct_io doesn't make sense and in fact doesn't
	 * work for other file systems, so just disable direct io for this test.
	 */
	if (test == 19)
		direct_io = 0;

	fname = argv[optind];
	if (!fname)
		usage();

	printf("Random seed is %u\n", seed);
	srandom(seed);

	if (direct_io) {
		openflags |= O_DIRECT;
		ret = posix_memalign((void **)&buf, getpagesize(), 4096);
		if (ret) {
			fprintf(stderr, "Error allocating buf: %d\n", ret);
			return 1;
		}
	} else {
		buf = malloc(4096);
		if (!buf) {
			fprintf(stderr, "Error allocating buf: %d\n", errno);
			return 1;
		}
	}

	test_fd = open(fname, openflags | O_CREAT | O_TRUNC, 0644);
	if (test_fd < 0) {
		fprintf(stderr, "Error opening file %d (%s)\n", errno,
			strerror(errno));
		return 1;
	}

	switch (test) {
	case 1:
		ret = test_one(&max_blocks);
		break;
	case 2:
		ret = test_two(&max_blocks);
		break;
	case 3:
		ret = test_three(&max_blocks, 0, 0, 0, 0);
		break;
	case 4:
		ret = test_three(&max_blocks, 1, 0, 0, 0);
		break;
	case 5:
		ret = test_three(&max_blocks, 0, 1, 0, 0);
		break;
	case 6:
		ret = test_three(&max_blocks, 1, 1, 0, 0);
		break;
	case 7:
		ret = test_three(&max_blocks, 0, 0, 1, 0);
		break;
	case 8:
		ret = test_three(&max_blocks, 1, 0, 1, 0);
		break;
	case 9:
		ret = test_three(&max_blocks, 0, 1, 1, 0);
		break;
	case 10:
		ret = test_three(&max_blocks, 1, 1, 1, 0);
		break;
	case 11:
		ret = test_three(&max_blocks, 0, 0, 0, 1);
		break;
	case 12:
		ret = test_three(&max_blocks, 0, 1, 0, 1);
		break;
	case 13:
		ret = test_three(&max_blocks, 0, 0, 1, 1);
		break;
	case 14:
		ret = test_three(&max_blocks, 0, 1, 1, 1);
		break;
	case 15:
		ret = test_three(&max_blocks, 1, 0, 0, 1);
		break;
	case 16:
		ret = test_three(&max_blocks, 1, 1, 0, 1);
		break;
	case 17:
		ret = test_three(&max_blocks, 1, 0, 1, 1);
		break;
	case 18:
		ret = test_three(&max_blocks, 1, 1, 1, 1);
		break;
	case 19:
		ret = test_five();
		break;
	case 20:
		ret = test_six();
		break;
	case 21:
		/*
		 * This is just a perf test, keep moving it down so it's always
		 * the last test option.
		 */
		reboot = 0;
		ret = test_four(&max_blocks);
		goto out;
	default:
		usage();
	}

	if (ret)
		goto out;

	if (fsync(test_fd)) {
		fprintf(stderr, "Fsync failed, test results will be invalid: "
			"%d\n", errno);
		return 1;
	}
	if (reboot)
		system("reboot -fn");
out:
	free(buf);
	close(test_fd);
	return ret;
}
