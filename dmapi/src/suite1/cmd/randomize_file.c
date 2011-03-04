/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <sys/types.h>

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
	This program reads the file specified on the command line and
	produces a new file on stdout which contains the lines from the
	input file scrambled into a random order.  This is useful for
	'randomizing' a database to simulate the kind of I/O that will
	occur when many records have been added and deleted over time.
*/

#define	FIELD_WIDTH 21

static	char	buffer[4096];
char *Progname;

int
main(
	int	argc,
	char	**argv)
{
	FILE	*infile;
	FILE	*tmpfile;
	char	path[] = "file_XXXXXX";
	int	line_count = 0;
	int	i;
	int	j;

	Progname = argv[0];
	if (argc != 2) {
		fprintf(stderr, "Usage: %s infile\n", argv[0]);
		exit(1);
	}

	/* Read through the input file and count how many lines are present. */

	if ((infile = fopen(argv[1], "r")) == NULL) {
		fprintf(stderr, "can't open %s\n", argv[1]);
		exit(1);
	}
	for (;;) {
		if (fgets(buffer, sizeof(buffer), infile) == NULL) {
			if (!ferror(infile))
				break;
			fprintf(stderr, "Error on infile\n");
			exit(1);
		}
		line_count++;
	}
	fprintf(stderr, "%d lines in input file\n", line_count);

	/* Create a temp file.  Copy the input file to the temp file,
	   prepending a random number in the range
		0 <= X <= linecount-1
	   to each line copied.
	*/

	(void) mkstemp(path);
	if ((tmpfile = fopen(path, "w")) == NULL) {
		fprintf(stderr, "error opening temp file %s\n", path);
		exit(1);
	}
	rewind(infile);
	srand48((long)getpid());

	for (i = line_count - 1; i >= 0; i--) {
		if (fgets(buffer, sizeof(buffer), infile) == NULL) {
			if (!ferror(infile))
				break;
			fprintf(stderr, "Error on infile\n");
			exit(1);
		}
		j = (int)(drand48() * (float)i);
		fprintf(tmpfile, "%*d ", FIELD_WIDTH - 1, j);
		fputs(buffer, tmpfile);
	}
	if (fclose(infile)) {
		fprintf(stderr, "close of input file failed\n");
		exit(1);
	}
	if (fclose(tmpfile)) {
		fprintf(stderr, "close of temp file %s failed\n", path);
		exit(1);
	}
	fprintf(stderr, "random mapping complete\n");

	/* Use the system sort routine to sort the file into order on the
	   first field, effectively randomizing the lines.
	*/

	sprintf(buffer, "sort +0 -1 -o %s %s", path, path);
	if (system(buffer)) {
		fprintf(stderr, "sort call failed\n");
		exit(1);
	}
	fprintf(stderr, "sort complete\n");

	/* Copy the temp file to stdout, removing the prepended field. */

	if ((tmpfile = fopen(path, "rw")) == NULL) {
		fprintf(stderr, "error reopening temp file %s\n", path);
		exit(1);
	}

	for (i = 0; i < line_count; i++) {
		if (fgets(buffer, sizeof(buffer), tmpfile) == NULL) {
			if (!ferror(tmpfile))
				break;
			fprintf(stderr, "Error on tmpfile\n");
			exit(1);
		}
		if (fputs(buffer + FIELD_WIDTH, stdout) < 0) {
			fprintf(stderr, "Error on outfile\n");
			exit(1);
		}
	}
	if (unlink(path)) {
		fprintf(stderr, "can't unlink %s\n", path);
		exit(1);
	}
	exit(0);
}
