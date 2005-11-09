/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.
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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

typedef	void	*(*fpi_t)(void);
typedef	void	(*fpt_t)(int, void *);
typedef	void	(*fpd_t)(void *);
typedef struct	tdesc
{
	char	*name;
	fpi_t	init;
	fpt_t	test;
	fpd_t	done;
} tdesc_t;

static void	d_readdir(void *);
static void	*i_readdir(void);
static void	t_readdir(int, void *);
static void	crfiles(char **, int, char *);
static void	d_chown(void *);
static void	d_create(void *);
static void	d_linkun(void *);
static void	d_open(void *);
static void	d_rename(void *);
static void	d_stat(void *);
static void	delflist(char **);
static void	dotest(tdesc_t *);
static void	*i_chown(void);
static void	*i_create(void);
static void	*i_linkun(void);
static void	*i_open(void);
static void	*i_rename(void);
static void	*i_stat(void);
static char	**mkflist(int, int, char);
static double	now(void);
static void	prtime(char *, int, double);
static void	rmfiles(char **);
static void	t_chown(int, void *);
static void	t_create(int, void *);
static void	t_crunlink(int, void *);
static void	t_linkun(int, void *);
static void	t_open(int, void *);
static void	t_rename(int, void *);
static void	t_stat(int, void *);
static void	usage(void);

tdesc_t	tests[] = {
	{ "chown",	i_chown, t_chown, d_chown },
	{ "create",	i_create, t_create, d_create },
	{ "crunlink",	(fpi_t)0, t_crunlink, (fpd_t)0 },
	{ "readdir",	i_readdir, t_readdir, d_readdir },
	{ "linkun",	i_linkun, t_linkun, d_linkun },
	{ "open",	i_open, t_open, d_open },
	{ "rename",	i_rename, t_rename, d_rename },
	{ "stat",	i_stat, t_stat, d_stat },
	{ NULL }
};

char		*buffer;
int		compact = 0;
int		files_bg = 0;
int		files_op = 1;
char		**flist_bg;
char		**flist_op;
int		fnlen_bg = 5;
int		fnlen_op = 5;
int		fsize = 0;
int		iters = 0;
double		time_end;
double		time_start;
int		totsec = 0;
int		verbose = 0;

int
main(int argc, char **argv)
{
	int		c;
	char		*testdir;
	tdesc_t		*tp;

	testdir = getenv("TMPDIR");
	if (testdir == NULL)
		testdir = ".";
	while ((c = getopt(argc, argv, "cd:i:l:L:n:N:s:t:v")) != -1) {
		switch (c) {
		case 'c':
			compact = 1;
			break;
		case 'd':
			testdir = optarg;
			break;
		case 'i':
			iters = atoi(optarg);	
			break;
		case 'l':
			fnlen_op = atoi(optarg);
			break;
		case 'L':
			fnlen_bg = atoi(optarg);
			break;
		case 'n':
			files_op = atoi(optarg);
			break;
		case 'N':
			files_bg = atoi(optarg);
			break;
		case 's':
			fsize = atoi(optarg);
			break;
		case 't':
			totsec = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
			fprintf(stderr, "bad option\n");
			usage();
		}
	}
	if (!iters && !totsec)
		iters = 1;
	if (chdir(testdir) < 0) {
		perror(testdir);
		return 1;
	}
	if (mkdir("metaperf", 0777) < 0 || chdir("metaperf") < 0) {
		perror("metaperf");
		return 1;
	}
	for (; optind < argc; optind++) {
		for (tp = tests; tp->name; tp++) {
			if (strcmp(argv[optind], tp->name) == 0) {
				dotest(tp);
				break;
			}
		}
	}
	chdir("..");
	rmdir("metaperf");
	return 0;
}

static void
crfiles(char **flist, int fsize, char *buf)
{
	int	fd;
	char	**fnp;

	for (fnp = flist; *fnp; fnp++) {
		fd = creat(*fnp, 0666);
		if (fsize)
			write(fd, buf, fsize);
		close(fd);
	}
}

/* ARGSUSED */
static void
d_chown(void *v)
{
	rmfiles(flist_op);
}

/* ARGSUSED */
static void
d_create(void *v)
{
	rmfiles(flist_op);
}

static void
d_readdir(void *v)
{
	rmfiles(flist_op);
	closedir((DIR *)v);
}

/* ARGSUSED */
static void
d_linkun(void *v)
{
	unlink("a");
}

/* ARGSUSED */
static void
d_open(void *v)
{
	rmfiles(flist_op);
}

static void
d_rename(void *v)
{
	rmfiles(flist_op);
	rmfiles((char **)v);
	delflist((char **)v);
}

/* ARGSUSED */
static void
d_stat(void *v)
{
	rmfiles(flist_op);
}

static void
delflist(char **flist)
{
	char	**fnp;

	for (fnp = flist; *fnp; fnp++)
		free(*fnp);
	free(flist);
}

static void
dotest(tdesc_t *tp)
{
	double	dn;
	double	gotsec;
	int	n;
	void	*v;

	flist_bg = mkflist(files_bg, fnlen_bg, 'b');
	flist_op = mkflist(files_op, fnlen_op, 'o');
	if (fsize)
		buffer = calloc(fsize, 1);
	else
		buffer = NULL;
	crfiles(flist_bg, 0, (char *)0);
	n = iters ? iters : 1;
	v = (void *)0;
	for (;;) {
		if (tp->init)
			v = (tp->init)();
		sync();
		sleep(1);
		time_start = now();
		(tp->test)(n, v);
		time_end = now();
		if (tp->done)
			(tp->done)(v);
		gotsec = time_end - time_start;
		if (!totsec || gotsec >= 0.9 * totsec)
			break;
		if (verbose)
			prtime(tp->name, n, gotsec);
		if (!gotsec)
			gotsec = 1.0 / (2 * HZ);
		if (gotsec < 0.001 * totsec)
			dn = n * (0.01 * totsec / gotsec);
		else if (gotsec < 0.01 * totsec)
			dn = n * (0.1 * totsec / gotsec);
		else
			dn = n * (totsec / gotsec);
		if ((int)dn <= n)
			n++;
		else
			n = (int)dn;
	}
	prtime(tp->name, n, gotsec);
	rmfiles(flist_bg);
	delflist(flist_bg);
	delflist(flist_op);
	if (fsize)
		free(buffer);
}

static void *
i_chown(void)
{
	char	**fnp;

	crfiles(flist_op, 0, (char *)0);
	for (fnp = flist_op; *fnp; fnp++)
		chown(*fnp, 1, -1);
	return (void *)0;
}

static void *
i_create(void)
{
	crfiles(flist_op, fsize, buffer);
	return (void *)0;
}

static void *
i_readdir(void)
{
	crfiles(flist_op, 0, (char *)0);
	return opendir(".");
}

static void *
i_linkun(void)
{
	close(creat("a", 0666));
	return (void *)0;
}

static void *
i_open(void)
{
	crfiles(flist_op, 0, (char *)0);
	return (void *)0;
}

static void *
i_rename(void)
{
	crfiles(flist_op, 0, (char *)0);
	return (void *)mkflist(files_op, fnlen_op, 'r');
}

static void *
i_stat(void)
{
	crfiles(flist_op, 0, (char *)0);
	return (void *)0;
}

static char **
mkflist(int files, int fnlen, char start)
{
	int	i;
	char	**rval;

	rval = calloc(files + 1, sizeof(char *));
	for (i = 0; i < files; i++) {
		rval[i] = malloc(fnlen + 1);
		sprintf(rval[i], "%0*d%c", fnlen - 1, i, start);
	}
	return rval;
}

static double
now(void)
{
	struct timeval	t;

	gettimeofday(&t, (void *)0);
	return (double)t.tv_sec + 1.0e-6 * (double)t.tv_usec;
}

static void
prtime(char *name, int n, double t)
{
	double	ops_per_sec;
	double	usec_per_op;

	ops_per_sec = (double)n * (double)files_op / t;
	usec_per_op = t * 1.0e6 / ((double)n * (double)files_op);
	if (compact)
		printf("%s %d %d %d %d %d %d %f %f %f\n",
			name, n, files_op, fnlen_op, fsize, files_bg, fnlen_bg,
			t, ops_per_sec, usec_per_op);
	else {
		printf("%s: %d times, %d file(s) namelen %d",
			name, n, files_op, fnlen_op);
		if (fsize)
			printf(" size %d", fsize);
		if (files_bg)
			printf(", bg %d file(s) namelen %d",
				files_bg, fnlen_bg);
		printf(", time = %f sec, ops/sec=%f, usec/op = %f\n",
			t, ops_per_sec, usec_per_op);
	}
}

static void
rmfiles(char **flist)
{
	char	**fnp;

	for (fnp = flist; *fnp; fnp++)
		unlink(*fnp);
}

/* ARGSUSED */
static void
t_chown(int n, void *v)
{
	char	**fnp;
	int	i;

	for (i = 0; i < n; i++) {
		for (fnp = flist_op; *fnp; fnp++) {
			if ((i & 1) == 0)
				chown(*fnp, 2, -1);
			else
				chown(*fnp, 1, -1);
		}
	}
}

/* ARGSUSED */
static void
t_create(int n, void *v)
{
	int	i;

	for (i = 0; i < n; i++)
		crfiles(flist_op, fsize, buffer);
}

/* ARGSUSED */
static void
t_crunlink(int n, void *v)
{
	int	i;

	for (i = 0; i < n; i++) {
		crfiles(flist_op, fsize, buffer);
		rmfiles(flist_op);
	}
}

static void
t_readdir(int n, void *v)
{
	DIR	*dir;
	int	i;

	for (dir = (DIR *)v, i = 0; i < n; i++) {
		rewinddir(dir);
		while ((readdir(dir)) != NULL);
	}
}

/* ARGSUSED */
static void
t_linkun(int n, void *v)
{
	char	**fnp;
	int	i;

	for (i = 0; i < n; i++) {
		for (fnp = flist_op; *fnp; fnp++)
			link("a", *fnp);
		rmfiles(flist_op);
	}
}

/* ARGSUSED */
static void
t_open(int n, void *v)
{
	char		**fnp;
	int		i;

	for (i = 0; i < n; i++) {
		for (fnp = flist_op; *fnp; fnp++)
			close(open(*fnp, O_RDWR));
	}
}

static void
t_rename(int n, void *v)
{
	char	**fnp;
	int	i;
	char	**rflist;
	char	**rfp;

	for (rflist = (char **)v, i = 0; i < n; i++) {
		for (fnp = flist_op, rfp = rflist; *fnp; fnp++, rfp++) {
			if ((i & 1) == 0)
				rename(*fnp, *rfp);
			else
				rename(*rfp, *fnp);
		}
	}
}

/* ARGSUSED */
static void
t_stat(int n, void *v)
{
	char		**fnp;
	int		i;
	struct stat	stb;

	for (i = 0; i < n; i++) {
		for (fnp = flist_op; *fnp; fnp++)
			stat(*fnp, &stb);
	}
}

static void
usage(void)
{
	fprintf(stderr,
		"Usage: metaperf [-d dname] [-i iters|-t seconds] [-s fsize]\n"
		"\t[-l opfnamelen] [-L bgfnamelen]\n"
		"\t[-n opfcount] [-N bgfcount] test...\n");
	fprintf(stderr,
		"Tests: chown create crunlink linkun open rename stat readdir\n");
	exit(1);
}
