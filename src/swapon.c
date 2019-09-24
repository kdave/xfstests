/* swapon(8) without any sanity checks; simply calls swapon(2) directly. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/swap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>

static void usage(const char *prog)
{
	fprintf(stderr, "usage: %s [-v verb] PATH\n", prog);
	exit(EXIT_FAILURE);
}

enum verbs {
	TEST_SWAPON = 0,
	TEST_WRITE,
	TEST_MWRITE_AFTER,
	TEST_MWRITE_BEFORE_AND_MWRITE_AFTER,
	TEST_MWRITE_BEFORE,
	MAX_TEST_VERBS,
};

#define BUF_SIZE 262144
static char buf[BUF_SIZE];

static void handle_signal(int signal)
{
	fprintf(stderr, "Caught signal %d, terminating...\n", signal);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	struct sigaction act = {
		.sa_handler	= handle_signal,
	};
	enum verbs verb = TEST_SWAPON;
	void *p;
	ssize_t sz;
	int fd = -1;
	int ret, c;

	memset(buf, 0x58, BUF_SIZE);

	while ((c = getopt(argc, argv, "v:")) != -1) {
		switch (c) {
		case 'v':
			verb = atoi(optarg);
			if (verb < TEST_SWAPON || verb >= MAX_TEST_VERBS) {
				fprintf(stderr, "Verbs must be 0-%d.\n",
						MAX_TEST_VERBS - 1);
				usage(argv[0]);
			}
			break;
		default:
			usage(argv[0]);
			break;
		}
	}

	ret = sigaction(SIGSEGV, &act, NULL);
	if (ret) {
		perror("sigsegv action");
		return EXIT_FAILURE;
	}

	ret = sigaction(SIGBUS, &act, NULL);
	if (ret) {
		perror("sigbus action");
		return EXIT_FAILURE;
	}

	switch (verb) {
	case TEST_WRITE:
	case TEST_MWRITE_AFTER:
	case TEST_MWRITE_BEFORE_AND_MWRITE_AFTER:
	case TEST_MWRITE_BEFORE:
		fd = open(argv[optind], O_RDWR);
		if (fd < 0) {
			perror(argv[optind]);
			return EXIT_FAILURE;
		}
		break;
	default:
		break;
	}

	switch (verb) {
	case TEST_MWRITE_BEFORE_AND_MWRITE_AFTER:
	case TEST_MWRITE_BEFORE:
		p = mmap(NULL, BUF_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED,
				fd, 65536);
		if (p == MAP_FAILED) {
			perror("mmap");
			return EXIT_FAILURE;
		}
		memcpy(p, buf, BUF_SIZE);
		break;
	default:
		break;
	}

	if (optind != argc - 1)
		usage(argv[0]);

	ret = swapon(argv[optind], 0);
	if (ret) {
		perror("swapon");
		return EXIT_FAILURE;
	}

	switch (verb) {
	case TEST_WRITE:
		sz = pwrite(fd, buf, BUF_SIZE, 65536);
		if (sz < 0) {
			perror("pwrite");
			return EXIT_FAILURE;
		}
		break;
	case TEST_MWRITE_AFTER:
		p = mmap(NULL, BUF_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED,
				fd, 65536);
		if (p == MAP_FAILED) {
			perror("mmap");
			return EXIT_FAILURE;
		}
		/* fall through */
	case TEST_MWRITE_BEFORE_AND_MWRITE_AFTER:
		memcpy(p, buf, BUF_SIZE);
		break;
	default:
		break;
	}

	if (fd >= 0) {
		ret = fsync(fd);
		if (ret)
			perror("fsync");
		ret = close(fd);
		if (ret)
			perror("close");
	}

	return EXIT_SUCCESS;
}
