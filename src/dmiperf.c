/*
 * Copyright (c) 2006 Silicon Graphics, Inc.
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

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <attr/attributes.h>

typedef unsigned int uint_t;

/*
 * Loop over directory sizes:
 *	make m directories
 *	touch n files in each directory
 *	write y bytes to all files in each directory
 *	set DMF attribute on x files in each directory
 * Change directory sizes by multiplication or addition.
 * Allow control of starting & stopping sizes, name length, target directory.
 * Print size and wallclock time (ms per file).
 * Output can be used to make graphs (gnuplot)
 */

static uint_t	addval;
static uint_t	dirchars;
static char	*directory;
static uint_t	firstsize;
static uint_t	lastsize;
static uint_t	minchars;
static double	mulval;
static uint_t	nchars;
static uint_t	ndirs;
static uint_t	pfxchars;
static off64_t	fsize;
static char	*buffer;
static size_t	bsize;

static int	mkfile(char *, char *);
static void	filename(int, int, char *);
static int	hexchars(uint_t);
static uint_t	nextsize(uint_t);
static double	now(void);
static void	usage(void);

/*
 * Maximum size allowed, this is pretty nuts.
 * The largest one we've ever built has been about 2 million.
 */
#define	MAX_DIR_SIZE	(16 * 1024 * 1024)
#define	DFL_FIRST_SIZE	1
#define	DFL_LAST_SIZE	(1024 * 1024)
#define	MAX_DIR_COUNT	1024
#define	MIN_DIR_COUNT	1

#define DMFATTRLEN	22
#define DMFATTRNAME	"SGI_DMI_DMFATTR"

int
main(int argc, char **argv)
{
	int		c;
	uint_t		cursize;
	int		i;
	int		j;
	char		name[NAME_MAX + 1];
	char		attr[DMFATTRLEN];
	double		stime;

	while ((c = getopt(argc, argv, "a:b:c:d:f:l:m:n:s:")) != -1) {
		switch (c) {
		case 'a':
			addval = (uint_t)atoi(optarg);
			break;
		case 'b':
			bsize = (size_t)atol(optarg);
			break;
		case 'c':
			nchars = (uint_t)atoi(optarg);
			break;
		case 'd':
			directory = optarg;
			break;
		case 'f':
			firstsize = (uint_t)atoi(optarg);
			break;
		case 'l':
			lastsize = (uint_t)atoi(optarg);
			break;
		case 'm':
			mulval = atof(optarg);
			break;
		case 'n':
			ndirs = (uint_t)atoi(optarg);
			break;
		case 's':
			fsize = (off64_t)atol(optarg);
			break;
		case '?':
		default:
			usage();
			exit(1);
		}
	}
	if (!addval && !mulval)
		mulval = 2.0;
	else if ((addval && mulval) || mulval < 0.0) {
		usage();
		exit(1);
	}
	if (!bsize)
		bsize = 1024 * 1024;
	buffer = memalign(getpagesize(), bsize);
	memset(buffer, 0xfeed, bsize);
	memset(attr, 0xaaaaaaa, sizeof(attr));

	if (!directory)
		directory = ".";
	else {
		if (mkdir(directory, 0777) < 0 && errno != EEXIST) {
			perror(directory);
			exit(1);
		}
		if (chdir(directory) < 0) {
			perror(directory);
			exit(1);
		}
	}
	if (firstsize == 0)
		firstsize = DFL_FIRST_SIZE;
	else if (firstsize > MAX_DIR_SIZE)
		firstsize = MAX_DIR_SIZE;
	if (lastsize == 0)
		lastsize = DFL_LAST_SIZE;
	else if (lastsize > MAX_DIR_SIZE)
		lastsize = MAX_DIR_SIZE;
	if (lastsize < firstsize)
		lastsize = firstsize;
	minchars = hexchars(lastsize - 1);
	if (nchars < minchars)
		nchars = minchars;
	else if (nchars >= NAME_MAX + 1)
		nchars = NAME_MAX;
	if (ndirs > MAX_DIR_COUNT)
		ndirs = MAX_DIR_COUNT;
	if (ndirs < MIN_DIR_COUNT)
		ndirs = MIN_DIR_COUNT;
	dirchars = hexchars(ndirs);
	pfxchars = nchars - minchars;
	if (pfxchars)
		memset(&name[dirchars + 1], 'a', pfxchars);

	cursize = firstsize;
	for (j = 0; j < ndirs; j++) {
		filename(0, j, name);
		name[dirchars] = '\0';
		mkdir(name, 0777);
		stime = now();
		for (i = 0; i < cursize; i++) {
			filename((i + j) % cursize, j, name);
			close(mkfile(name, attr));
		}
		printf("%d %.3f\n", cursize,
			(now() - stime) * 1.0e3 / (cursize * ndirs));
		cursize = nextsize(cursize);
	}
	return 0;
}

static int
mkfile(char *name, char *attr)
{
	int		fd;
	ssize_t		wrote, wsize;
	off64_t		bytes = fsize;

	if ((fd = open(name, O_WRONLY | O_CREAT | O_EXCL | O_DIRECT, 0666)) < 0) {
		perror("open");
		exit(1);
	}
	if (attr_setf(fd, DMFATTRNAME, attr, DMFATTRLEN, ATTR_ROOT) < 0) {
		perror("attr_setf");
		exit(1);
	}
	while (bytes > 0) {
		wsize = (bsize < bytes) ? bsize : bytes;
		if ((wrote = write(fd, buffer, wsize)) < 0) {
			perror("write");
			exit(1);
		}
		bytes -= wrote;
	}
	return fd;
}

static void
filename(int idx, int dir, char *name)
{
	static char	hexc[16] = "0123456789abcdef";
	int		i;

	for (i = dirchars - 1; i >= 0; i--)
		*name++ = hexc[(dir >> (4 * i)) & 0xf];
	*name++ = '/';
	name += pfxchars;		/* skip pfx a's */
	for (i = minchars - 1; i >= 0; i--)
		*name++ = hexc[(idx >> (4 * i)) & 0xf];
	*name = '\0';
}

static int
hexchars(uint_t maxval)
{
	if (maxval < 16)
		return 1;
	if (maxval < 16 * 16)
		return 2;
	if (maxval < 16 * 16 * 16)
		return 3;
	if (maxval < 16 * 16 * 16 * 16)
		return 4;
	if (maxval < 16 * 16 * 16 * 16 * 16)
		return 5;
	if (maxval < 16 * 16 * 16 * 16 * 16 * 16)
		return 6;
	if (maxval < 16 * 16 * 16 * 16 * 16 * 16 * 16)
		return 7;
	return 8;
}

static uint_t
nextsize(uint_t cursize)
{
	double	n;

	n = cursize;
	if (addval)
		n += addval;
	else
		n *= mulval;
	if (n > (double)lastsize + 0.5)
		return lastsize + 1;	/* i.e. out of bounds */
	else if ((uint_t)n == cursize)
		return cursize + 1;
	else
		return (uint_t)n;
}

static double
now(void)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + 1.0e-6 * (double)tv.tv_usec;
}

static void
usage(void)
{
	fprintf(stderr,
		"usage: dirperf [-d dir] [-a addstep | -m mulstep] [-f first] "
		"[-l last] [-c nchars] [-n ndirs] [-s size]\n");
}
