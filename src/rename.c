/*
 * A trivial shell command wrapping rename(2).
 */

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: rename <from> <to>\n");
		exit(EXIT_FAILURE);
	}

	if (rename(argv[1], argv[2]) == -1) {
		perror("rename");
		exit(EXIT_FAILURE);
	}

	exit(0);
}
