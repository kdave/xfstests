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


/*
 * fill2:
 *   Derived from fill.c, adds command line options, block boundary marking
 *   and allows me to tweak to suit fill2fs without breaking other qa tests.
 *   This version marks block boundaries (512, 1k, 4k and 16k) within the file
 *   using characters guaranteed not to appear anywhere else, this may allow
 *   simple checks in future which can inspect these offsets and ensure one of
 *   of the four characters is present. Also fixes off-by-one error in fill.c
 *   and is more careful about checking when write operations fail - this is 
 *   needed by fill2fs to ensure that the number of bytes written is accurately
 *   determined.
 */

#include "global.h"

#define constpp char * const *

#define N(x) (sizeof(x)/sizeof(x[0]))

/* function prototypes */
static void illegal(char *, char *);
static void reqval(char, char * [], int);
static void respec(char, char * [], int);
static void unknown(char, char *);
static void usage(void);
char *progname;

char *dopts[] =		{ "nbytes", "linelength", "seed", "file", NULL };
enum			{ D_NBYTES, D_LINELENGTH, D_SEED, D_FILE, D_ISSET, D_NUM };
int dflags[D_NUM] =	{ 0 };
char *bopts[] =		{ "512", "1k", "4k", "16k" };
enum			{ B_512, B_1K, B_4K, B_16K, B_ISSET, B_NUM };
int bflags[B_NUM] =	{ 0 };


/*
 * block boundaries
 *
 */

/* block boundaries which should be flagged in the output file */
/* flag is the character that should be used to indicate each type */
/* of block boundary */


struct s_block_type {
    int mark;
    int size;
    char flag;
};

static struct s_block_type btypes[] = {
    { 0, 512, '!' },	/* 512 */
    { 0, 1024, '"' },	/* 1k */
    { 0, 4096, '#' },	/* 4k */
    { 0, 16384, '$' },	/* 16k */
};

/*
 * main
 *
 */

int
main(int argc, char **argv)
{
    int			status = 0;	/* return status */
    int			c;		/* current option character */
    char		*p;		/* for getsubopt calls */
    long		nbytes;		/* total number of bytes to write */
    int			dlen;		/* length of normal output data line */
    const char		*dseed = NULL;	/* input string for seeding rand */
    unsigned int	seed;		/* seed for output data */    
    char		*dfile = NULL;	/* where to write output */

    FILE		*f;		/* output file */
    char		*dbuf;		/* output line buffer */
    char		bbuf[50];	/* block boundary string */
    char		*active = NULL;	/* active buffer to copy out of */
    size_t		hlen;		/* header length (bytes+key) in output */
					/* lines */
    char		*hptr;		/* pointer to end of header */
    char		*ptr;		/* current position to copy from */
    int			blktype = 0;	/* current block boundary type */
    int			boundary;	/* set if current output char lies on */
					/* block boundary */
    long		i;
    int			j;
    int			l = 0;


    /* defaults */

    progname = basename(argv[0]);
    for (p = progname; *p; p++) {
	    if (*p == '/') {
		    progname = p + 1;
	    }
    }
    nbytes = 1024 * 1024;
    dlen = 73;	/* includes the trailing newline */

    while ((c = getopt(argc, argv, "d:b:")) != EOF) {
	switch (c) {
	case 'd':
	    if (dflags[D_ISSET])
		respec('d', NULL, 0);
	    dflags[D_ISSET] = 1;

	    p = optarg;
	    while (*p != '\0') {
		char *value;
		switch (getsubopt(&p, (constpp)dopts, &value)) {
		case D_NBYTES:
		    if (! value) reqval('d', dopts, D_NBYTES);
		    if (dflags[D_NBYTES]) respec('d', dopts, D_NBYTES);
		    dflags[D_NBYTES] = 1;
		    nbytes = strtol(value, &ptr, 10);
		    if (ptr == value) illegal(value, "d nbytes");
		    break;

		case D_LINELENGTH:
		    if (! value) reqval('d', dopts, D_LINELENGTH);
		    if (dflags[D_LINELENGTH]) respec('d', dopts, D_LINELENGTH);
		    dflags[D_LINELENGTH] = 1;
		    dlen = (int) strtol(value, &ptr, 10) + 1;
		    if (ptr == value) illegal(value, "d linelength");
		    break;

		case D_SEED:
		    if (! value) reqval('d', dopts, D_SEED);
		    if (dflags[D_SEED]) respec('D', dopts, D_SEED);
		    dflags[D_SEED] = 1;
		    dseed = value;
		    break;

		case D_FILE:
		    if (! value) reqval('d', dopts, D_FILE);
		    if (dflags[D_FILE]) respec('d', dopts, D_FILE);
		    dflags[D_FILE] = 1;
		    dfile = value;
		    break;
		    
		default:
		    unknown('d', value);
		}
	    }
	    break;

	case 'b':
	    if (bflags[B_ISSET])
		respec('b', NULL, 0);
	    bflags[B_ISSET] = 1;

	    p = optarg;
	    while (*p != '\0') {
		char *value;
		switch (getsubopt(&p, (constpp)bopts, &value)) {
		case B_512:
		    if (value) illegal(value, "b 512");
		    if (bflags[B_512]) respec('b', bopts, B_512);
		    bflags[B_512] = 1;
		    btypes[0].mark = 1;
		    break;

		case B_1K:
		    if (value) illegal(value, "b 1k");
		    if (bflags[B_1K]) respec('b', bopts, B_1K);
		    bflags[B_1K] = 1;
		    btypes[1].mark = 1;
		    break;

		case B_4K:
		    if (value) illegal(value, "b 4k");
		    if (bflags[B_4K]) respec('b', bopts, B_4K);
		    bflags[B_4K] = 1;
		    btypes[2].mark = 1;
		    break;

		case B_16K:
		    if (value) illegal(value, "b 16k");
		    if (bflags[B_16K]) respec('b', bopts, B_16K);
		    bflags[B_16K] = 1;
		    btypes[3].mark = 1;
		    break;

		default:
		    unknown('b', value);
		    break;
		}
	    }
	    break;

	case '?':
	    usage();
	}
    }

    if (dflags[D_FILE] && optind != argc)
	usage();

    if (! dflags[D_FILE] && argc - optind != 1)
	usage();

    if (! dflags[D_FILE])
	dfile = argv[optind];

    if ((f = fopen(dfile, "w")) == NULL) {
	fprintf(stderr, "fill2: cannot create \"%s\": %s\n",
		dfile, strerror(errno));
	status = 1;
	goto cleanup;
    }

    if (! dflags[D_SEED]) 
	dseed = dfile;

    /* seed is an ascii string */
    seed = 0;
    i = 0;
    while (dseed[i]) {
	seed <<= 8;
	seed |= dseed[i];
	i++;
    }
    srand(seed);


    /* normal data line format: XXXXXXXXXXXX CCCC...CCC CCCCCCCCCCCC...CCC */
    /* block boundary format: CXXXXXXX */

    dbuf = (char *) malloc(dlen + 1);
    hlen = 12+1+strlen(dseed)+1;
    assert(hlen < dlen);
    hptr = dbuf + hlen;

    for (i = 0; i < nbytes; i++) {

	
	/* is this a block boundary? check largest to smallest */
	boundary = 0;
	for (j = N(btypes) - 1; j >= 0; j--) {
	    if (btypes[j].mark) {
		/* first time through or just before a block boundary? */
		if (i == 0 || i % btypes[j].size == btypes[j].size - 1) {
		    boundary = 1;
		    blktype = j;
		    break;
		}
	    }
	}


	/* are there block boundaries to check? */
	/* is this the first time through? */
	/* block boundaries take priority over other output */
	if (bflags[B_ISSET] && (i == 0 || boundary)) {
	    sprintf(bbuf, "%s%c%07ld\n",
		    i ? "\n" : "",
		    btypes[blktype].flag,
		    /* remember i is one less than block boundary */
		    i ? (i+1) / btypes[blktype].size : 0);
	    active = bbuf;
	    ptr = bbuf;
	}
	/* are we at the start of a new line? */
	else if (i == 0
		 || (active == bbuf && *ptr == '\0')
		 || (active == dbuf && l == dlen))
	{
	    sprintf(dbuf, "%012ld %s ", i, dseed);
	    assert(*hptr == '\0');
	    for (ptr = hptr; ptr != dbuf + dlen - 1; ptr++) {
		/* characters upto 126 '~' are used */
		/* do not use !"#$ - leave free for use as block markers */
		*ptr = 37 + (rand() % (127 - 37)); 
	    }
	    *ptr++ = '\n';
	    *ptr = '\0';
	    assert(ptr == dbuf + dlen);
	    active = dbuf;
	    ptr = dbuf;
	    l = 0;
	}
	else {
	    /* continue copying from the active buffer */
	}

	/* output one new character from current buffer */
	if (fputc(*ptr++, f) == EOF) {
	    fprintf(stderr, "fill2: could not write character to \"%s\": %s\n",
		    dfile, strerror(errno));
	    status = 1;
	    goto cleanup;
	}
	if (active == dbuf) l++;

    }

 cleanup:

    /* close file and flush buffers - check if this fails */
    if (fclose(f) != 0) {
	fprintf(stderr, "fill2: fclose() on \"%s\" failed: %s\n",
		dfile, strerror(errno));
	status = 1;
    }
    return status;
}



/*
 * suboption checking functions
 *
 */

static void
illegal(char *value, char *opt)
{
    fprintf(stderr, "%s: Error: Illegal value \"%s\" for -%s option\n",
	    progname, value, opt);
    usage();
}
static void
reqval(char opt, char *tab[], int idx)
{
    fprintf(stderr, "%s: Error: -%c %s option requires a value\n",
	    progname, opt, tab[idx]);
    usage();
}
static void
respec(char opt, char *tab[], int idx)
{
    fprintf(stderr, "%s: Error: ", progname);
    fprintf(stderr, "-%c ", opt);
    if (tab) fprintf(stderr, "%s ", tab[idx]);
    fprintf(stderr, "option respecified\n");
    usage();
}
static void
unknown(char opt, char *s)
{
    fprintf(stderr, "%s: Error: Unknown option -%c %s\n", progname, opt, s);
    usage();
}
static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options] filename\n"
"Options:\n"
"  -d [nbytes=num,linelength=num,   output data properties\n"
"      seed=string,file=name]\n"
"  -b [512,1k,4k,16k]               where to mark block boundaries\n", progname);
    exit(2);
}
