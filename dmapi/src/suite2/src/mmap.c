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
/*
 *	This routine simulates
 *	
 *	cp	file1 file2
 *
 *
 *	It is a demo program which does the copy by memory mapping each of the
 *	files and then doing a byte at a time memory copy.
 *
 */
#ifdef linux
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>


char * Progname;
off_t	len;			/* length of file 1 */
off_t	offset = 0;
int	print_flags_set = 1;

typedef	struct	fflag {
	int	arg;		/* != 0 if ars specified */
	int	value;		/* flags value */
} fflag_t;


typedef enum ftype {	/* flag bit types */
	FL_MAP, FL_PROT, FL_OPEN, FL_MAX
} ftype_t;

typedef struct	mfile	{
	fflag_t	flags[FL_MAX];
	char	*path;
	int	fd;
	struct	stat	st;
#ifdef linux
	void *p;
#else
	addr_t	p;
#endif
} mfile_t;


#define	FLAG(symbol,type) { # symbol , symbol, type }
#define	MAP_NONE	0

static	struct	{
	char	*name;
	int	value;
	ftype_t	type;
} Flags[] = {
	FLAG(O_RDONLY, FL_OPEN),
	FLAG(O_WRONLY, FL_OPEN),
	FLAG(O_RDWR, FL_OPEN),
	FLAG(O_NDELAY, FL_OPEN),
	FLAG(O_NONBLOCK, FL_OPEN),
	FLAG(O_APPEND, FL_OPEN),
	FLAG(O_SYNC, FL_OPEN),
	FLAG(O_TRUNC, FL_OPEN),
	FLAG(O_CREAT, FL_OPEN),
	FLAG(O_DIRECT, FL_OPEN),
	FLAG(PROT_NONE, FL_PROT),
	FLAG(PROT_READ, FL_PROT),
	FLAG(PROT_WRITE, FL_PROT),
	FLAG(PROT_EXEC, FL_PROT),
	FLAG(MAP_SHARED, FL_MAP),
	FLAG(MAP_PRIVATE, FL_MAP),
	FLAG(MAP_FIXED, FL_MAP),
	FLAG(MAP_NONE, FL_MAP),
};

int	num_Flags = sizeof(Flags)/sizeof(Flags[0]);


mfile_t	*ifile, *ofile;
mfile_t	*hfile;	/* Hack job */
static int	hack = 0;
	
static	mfile_t	*new_mfile(void);
static int mfile_opt(char * s, mfile_t * f);
static	void print_flags(char *s, mfile_t *f);
static void Usage(void);

int
main(int argc, char * argv[])
{
	int	opt;

	Progname = strrchr(argv[0], '/');
	if (Progname == NULL)
		Progname = argv[0];
	else
		Progname++;

	ifile = new_mfile();
	ofile = new_mfile();
	hfile = new_mfile();
	if (ifile == NULL || ofile == NULL || hfile == NULL) {
		fprintf(stderr,"%s: malloc failure.\n", Progname);
		exit (1);
	}

	/* Set default flags */
	ifile->flags[FL_MAP].value = MAP_PRIVATE;
	ifile->flags[FL_PROT].value = PROT_READ;
	ifile->flags[FL_OPEN].value = O_RDONLY;
	ofile->flags[FL_MAP].value = MAP_SHARED;
	ofile->flags[FL_PROT].value = PROT_WRITE;
	ofile->flags[FL_OPEN].value = O_RDWR|O_CREAT;

	while ((opt = getopt(argc, argv, "i:o:h:d")) != EOF) {
		switch(opt) {
		case 'i':
			if (mfile_opt(optarg, ifile) != 0) {
				fprintf(stderr, "%s: Invalid -i option %s\n",
					Progname, optarg);
				Usage();
			}
			break;

		case 'o':
			if (mfile_opt(optarg, ofile) != 0) {
				fprintf(stderr, "%s: Invalid -o option %s\n",
					Progname, optarg);
				Usage();
			}
			break;

		case 'h':
			if (mfile_opt(optarg, hfile) != 0) {
				fprintf(stderr, "%s: Invalid -h option %s\n",
					Progname, optarg);
				Usage();
			}
			hack = 1;
			break;

		case 'd':
			print_flags_set ^= 1;
			break;
		case '?':
			Usage();
		}
	}

	if (optind+1 > argc)
		Usage();

	ifile->path = argv[optind++];
	ofile->path = argv[optind++];

	if (optind != argc) 	/* Extra args on command line */
		Usage();

	if (stat(ifile->path, &(ifile->st)) < 0) {
		fprintf(stderr,"%s: stat of %s failed.\n",
						Progname, ifile->path);
		perror(ifile->path);
		exit(2);
	}

	len = ifile->st.st_size;

	ifile->fd = open(ifile->path, ifile->flags[FL_OPEN].value);
	if (ifile->fd < 0) {
		fprintf(stderr,"%s: cannot open %s\n", Progname, ifile->path);
                perror(ifile->path);
                exit(2);
	}


	ofile->fd = open(ofile->path, ofile->flags[FL_OPEN].value, 0644);
        if (ofile->fd < 0) {
                fprintf(stderr,"%s: cannot open %s\n", Progname, ofile->path);
                perror(ofile->path);
                exit(3);
        }

	if (print_flags_set) {
		print_flags("Input ", ifile);
		print_flags("Output", ofile);
		if (hack)
			print_flags("Hack  ", hfile);
	}


	ifile->p = mmap(NULL, len, ifile->flags[FL_PROT].value,
				ifile->flags[FL_MAP].value, ifile->fd, 0);
	if (ifile->p == MAP_FAILED) {
		fprintf(stderr,"%s: cannot mmap %s\n", Progname, ifile->path);
                perror(ifile->path);
                exit(2);
        }

	ofile->p = mmap(NULL, len, ofile->flags[FL_PROT].value,
				ofile->flags[FL_MAP].value , ofile->fd, 0);
		if (ofile->p == MAP_FAILED) {
                fprintf(stderr,"%s: cannot mmap %s\n", Progname, ofile->path);
                perror(ofile->path);
                exit(3);
        }

	if (hack) {
		int	error;

		error = mprotect(ofile->p, len, hfile->flags[FL_PROT].value);
		if (error) {
			fprintf(stderr,"%s: mprotect call failed.\n", Progname);
			perror("mprotect");
			exit(3);
		}
	}
	
	bcopy(ifile->p, ofile->p, len);

	printf("%s complete.\n", Progname);
	return 0;
}

static mfile_t *
new_mfile(void)
{
	mfile_t	*ptr = (mfile_t *)malloc(sizeof(*ptr));
	if (ptr)
		bzero(ptr, sizeof *ptr);

	return	ptr;
}


static	int
mfile_opt(char * s, mfile_t *f)
{
	int	i;
	ftype_t	type;

	for (i = 0; i < num_Flags; i++) {
		if(!strcasecmp(Flags[i].name, s)) {

			/* Zero value if this is 1st arg of this type */

			type = Flags[i].type;
			if (f->flags[type].arg++ == 0) 
				f->flags[type].value = 0;
			f->flags[type].value |= Flags[i].value;
			return 0;
		}
	}
	return -1;	/* error - string not found */
}

static void
Usage(void)
{
	int	i;

	fprintf(stderr, 
		"Usage: %s [-d] [-i flag] [-i flag] [-o flag] ... file1 file2\n",
		Progname);
	fprintf(stderr, "Valid flag values are:\n");

	for (i = 0; i < num_Flags; i++) {
		fprintf(stderr,"%15s",Flags[i].name);
		if ((i+1)%4 == 0 || i == num_Flags-1)
			fprintf(stderr,"\n");
		else
			fprintf(stderr,",");
	}
	exit(1);
}

static	void
print_flags(char *s, mfile_t *f)
{
	int		i;
	ftype_t		type;

	printf("DEBUG - %s flags:\n", s);
	for (i = 0; i < num_Flags; i++) {
		type = Flags[i].type;
		if (type == FL_OPEN && Flags[i].value == O_RDONLY && 
			((f->flags[type].value) & 3) == 0) 
				/* Hack to print out O_RDONLY */
				printf("\t%s\n", Flags[i].name);
		else if ((Flags[i].value & (f->flags[type].value)) != 0)
			printf("\t%s\n", Flags[i].name);
	}
}
