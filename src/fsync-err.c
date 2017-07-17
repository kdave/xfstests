/*
 * fsync-err.c: test whether writeback errors are reported to all open fds
 * 		and properly cleared as expected after being seen once on each
 *
 * Copyright (c) 2017: Jeff Layton <jlayton@redhat.com>
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>

/*
 * btrfs has a fixed stripewidth of 64k, so we need to write enough data to
 * ensure that we hit both stripes by default.
 */
#define DEFAULT_BUFSIZE (65 * 1024)

/* default number of fds to open */
#define DEFAULT_NUM_FDS	10

static void usage()
{
	printf("Usage: fsync-err [ -b bufsize ] [ -n num_fds ] [ -s ] -d dmerror path <filename>\n");
}

int main(int argc, char **argv)
{
	int *fd, ret, i, numfds = DEFAULT_NUM_FDS;
	char *fname, *buf;
	char *dmerror_path = NULL;
	char *cmdbuf;
	size_t cmdsize, bufsize = DEFAULT_BUFSIZE;
	bool simple_mode = false;

	while ((i = getopt(argc, argv, "b:d:n:s")) != -1) {
		switch (i) {
		case 'b':
			bufsize = strtol(optarg, &buf, 0);
			if (*buf != '\0') {
				printf("bad string conversion: %s\n", optarg);
				return 1;
			}
			break;
		case 'd':
			dmerror_path = optarg;
			break;
		case 'n':
			numfds = strtol(optarg, &buf, 0);
			if (*buf != '\0') {
				printf("bad string conversion: %s\n", optarg);
				return 1;
			}
			break;
		case 's':
			/*
			 * Many filesystems will continue to throw errors after
			 * fsync has already advanced to the current error,
			 * due to metadata writeback failures or other
			 * issues. Allow those fs' to opt out of more thorough
			 * testing.
			 */
			simple_mode = true;
		}
	}

	if (argc < 1) {
		usage();
		return 1;
	}

	if (!dmerror_path) {
		printf("Must specify dmerror path with -d option!\n");
		return 1;
	}

	/* Remaining argument is filename */
	fname = argv[optind];

	fd = calloc(numfds, sizeof(*fd));
	if (!fd) {
		printf("malloc failed: %m\n");
		return 1;
	}

	for (i = 0; i < numfds; ++i) {
		fd[i] = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd[i] < 0) {
			printf("open of fd[%d] failed: %m\n", i);
			return 1;
		}
	}

	buf = malloc(bufsize);
	if (!buf) {
		printf("malloc failed: %m\n");
		return 1;
	}

	/* fill it with some junk */
	memset(buf, 0x7c, bufsize);

	for (i = 0; i < numfds; ++i) {
		ret = write(fd[i], buf, bufsize);
		if (ret < 0) {
			printf("First write on fd[%d] failed: %m\n", i);
			return 1;
		}
	}

	for (i = 0; i < numfds; ++i) {
		ret = fsync(fd[i]);
		if (ret < 0) {
			printf("First fsync on fd[%d] failed: %m\n", i);
			return 1;
		}
	}

	/* enough for path + dmerror command string  (and then some) */
	cmdsize = strlen(dmerror_path) + 64;

	cmdbuf = malloc(cmdsize);
	if (!cmdbuf) {
		printf("malloc failed: %m\n");
		return 1;
	}

	ret = snprintf(cmdbuf, cmdsize, "%s load_error_table", dmerror_path);
	if (ret < 0 || ret >= cmdsize) {
		printf("sprintf failure: %d\n", ret);
		return 1;
	}

	/* flip the device to non-working mode */
	ret = system(cmdbuf);
	if (ret) {
		if (WIFEXITED(ret))
			printf("system: program exited: %d\n",
					WEXITSTATUS(ret));
		else
			printf("system: 0x%x\n", (int)ret);

		return 1;
	}

	for (i = 0; i < numfds; ++i) {
		ret = write(fd[i], buf, bufsize);
		if (ret < 0) {
			printf("Second write on fd[%d] failed: %m\n", i);
			return 1;
		}
	}

	for (i = 0; i < numfds; ++i) {
		ret = fsync(fd[i]);
		/* Now, we EXPECT the error! */
		if (ret >= 0) {
			printf("Success on second fsync on fd[%d]!\n", i);
			return 1;
		}
	}

	if (!simple_mode) {
		for (i = 0; i < numfds; ++i) {
			ret = fsync(fd[i]);
			if (ret < 0) {
				/*
				 * We did a failed write and fsync on each fd before.
				 * Now the error should be clear since we've not done
				 * any writes since then.
				 */
				printf("Third fsync on fd[%d] failed: %m\n", i);
				return 1;
			}
		}
	}

	/* flip the device to working mode */
	ret = snprintf(cmdbuf, cmdsize, "%s load_working_table", dmerror_path);
	if (ret < 0 || ret >= cmdsize) {
		printf("sprintf failure: %d\n", ret);
		return 1;
	}

	ret = system(cmdbuf);
	if (ret) {
		if (WIFEXITED(ret))
			printf("system: program exited: %d\n",
					WEXITSTATUS(ret));
		else
			printf("system: 0x%x\n", (int)ret);

		return 1;
	}

	if (!simple_mode) {
		for (i = 0; i < numfds; ++i) {
			ret = fsync(fd[i]);
			if (ret < 0) {
				/* The error should still be clear */
				printf("fsync after healing device on fd[%d] failed: %m\n", i);
				return 1;
			}
		}
	}

	/*
	 * reopen each file one at a time to ensure the same inode stays
	 * in core. fsync each one to make sure we see no errors on a fresh
	 * open of the inode.
	 */
	for (i = 0; i < numfds; ++i) {
		ret = close(fd[i]);
		if (ret < 0) {
			printf("Close of fd[%d] returned unexpected error: %m\n", i);
			return 1;
		}
		fd[i] = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd[i] < 0) {
			printf("Second open of fd[%d] failed: %m\n", i);
			return 1;
		}
		ret = fsync(fd[i]);
		if (ret < 0) {
			/* New opens should not return an error */
			printf("First fsync after reopen of fd[%d] failed: %m\n", i);
			return 1;
		}
	}

	printf("Test passed!\n");
	return 0;
}
