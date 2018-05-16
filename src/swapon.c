/* swapon(8) without any sanity checks; simply calls swapon(2) directly. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/swap.h>

int main(int argc, char **argv)
{
	int ret;

	if (argc != 2) {
		fprintf(stderr, "usage: %s PATH\n", argv[0]);
		return EXIT_FAILURE;
	}

	ret = swapon(argv[1], 0);
	if (ret) {
		perror("swapon");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
