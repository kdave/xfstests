#include <stdio.h>
#include <errno.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	int ret;

	ret = truncate(argv[0], 4096);
	if (ret != -1) {
		fprintf(stderr, "truncate(argv[0]) should have failed\n");
		return 1;
	}
	if (errno != ETXTBSY) {
		perror("truncate(argv[0])");
		return 1;
	}

	return 0;
}
