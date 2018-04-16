#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>

int main(int argc, char *argv[])
{
	const char *progname = basename(argv[0]);
	int ret;

	ret = truncate(argv[0], 4096);
	if (ret != -1) {
		if (argc == 2 && strcmp(argv[1], "--may-succeed") == 0)
			return 0;
		fprintf(stderr, "truncate(%s) should have failed\n",
			progname);
		return 1;
	}
	if (errno != ETXTBSY) {
		perror(progname);
		return 1;
	}

	return 0;
}
