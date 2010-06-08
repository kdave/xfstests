#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * Simple wrapper around realpath(3) to get absolute path
 * to a device name; many xfstests scripts don't cope well
 * with symlinked devices due to differences in /proc/mounts,
 * /etc/mtab, mount output, etc.
 */

int main(int argc, char *argv[])
{
	char path[PATH_MAX];
	char resolved_path[PATH_MAX];

	if (argc != 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	strncpy(path, argv[1], PATH_MAX-1);

	if (!realpath(path, resolved_path)) {
		perror("Failed to resolve path for %s");
		return 1;
	}

	printf("%s\n", resolved_path);
	return 0;
}
