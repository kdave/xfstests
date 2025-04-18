// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000 Silicon Graphics, Inc.
 * All Rights Reserved.
 */
#include <stdio.h>
#include <sys/param.h>
#include "str_to_bytes.h"

/****************************************************************************
 * str_to_bytes(s)
 *
 * Computes the number of bytes described by string s.  s is assumed to be
 * a base 10 positive (ie. >= 0) number followed by an optional single
 * character multiplier.  The following multipliers are supported:
 *
 *              char    mult
 *              -----------------
 *              b       BSIZE  or BBSIZE
 *              k       1024 bytes
 *              K       1024 * sizeof(long)
 *              m       2^20 (1048576)
 *              M       2^20 (1048576 * sizeof(long)
 *              g       2^30 (1073741824)
 *              G       2^30 (1073741824) * sizeof(long)
 *
 * for instance, "1k" and "1024" would both cause str_to_bytes to return 1024.
 *
 * Returns -1 if mult is an invalid character, or if the integer portion of
 * s is not a positive integer.
 *
 ****************************************************************************/

#ifdef __linux__
#define B_MULT	DEV_BSIZE	/* block size */
#endif


#define K_MULT	1024		/* Kilo or 2^10 */
#define M_MULT	1048576		/* Mega or 2^20 */
#define G_MULT	1073741824	/* Giga or 2^30 */
#define T_MULT	1099511627776	/* tera or 2^40 */

int
str_to_bytes(char *s)
{
    char    mult, junk;
    int	    nconv;
    float   num;

    nconv = sscanf(s, "%f%c%c", &num, &mult, &junk);
    if (nconv == 0 || nconv == 3 )
	return -1;

    if (nconv == 1)
	return num;

    switch (mult) {
    case 'b':
		return (int)(num * (float)B_MULT);
    case 'k':
		return (int)(num * (float)K_MULT);
    case 'K':
		return (int)((num * (float)K_MULT) * sizeof(long));
    case 'm':
		return (int)(num * (float)M_MULT);
    case 'M':
		return (int)((num * (float)M_MULT) * sizeof(long));
    case 'g':
		return (int)(num * (float)G_MULT);
    case 'G':	
    		return (int)((num * (float)G_MULT) * sizeof(long));
    default:
	return -1;
    }
}

long
str_to_lbytes(char *s)
{
    char    mult, junk;
    long    nconv;
    float   num;

    nconv = sscanf(s, "%f%c%c", &num, &mult, &junk);
    if (nconv == 0 || nconv == 3 )
	return -1;

    if (nconv == 1)
	return (long)num;

    switch (mult) {
    case 'b':
		return (long)(num * (float)B_MULT);
    case 'k':
		return (long)(num * (float)K_MULT);
    case 'K':
		return (long)((num * (float)K_MULT) * sizeof(long));
    case 'm':
		return (long)(num * (float)M_MULT);
    case 'M':
		return (long)((num * (float)M_MULT) * sizeof(long));
    case 'g':
		return (long)(num * (float)G_MULT);
    case 'G':	
    		return (long)((num * (float)G_MULT) * sizeof(long));
    default:
	return -1;
    }
}

/*
 * Force 64 bits number when compiled as a 32-bit binary.
 * This allows for a number bigger than 2G.
 */

long long
str_to_llbytes(char *s)
{
    char    mult, junk;
    long    nconv;
    double  num;

    nconv = sscanf(s, "%lf%c%c", &num, &mult, &junk);
    if (nconv == 0 || nconv == 3 )
	return -1;

    if (nconv == 1)
	return (long long)num;

    switch (mult) {
    case 'b':
		return (long long)(num * (float)B_MULT);
    case 'k':
		return (long long)(num * (float)K_MULT);
    case 'K':
		return (long long)((num * (float)K_MULT) * sizeof(long long));
    case 'm':
		return (long long)(num * (float)M_MULT);
    case 'M':
		return (long long)((num * (float)M_MULT) * sizeof(long long));
    case 'g':
		return (long long)(num * (float)G_MULT);
    case 'G':	
    		return (long long)((num * (float)G_MULT) * sizeof(long long));
    default:
	return -1;
    }
}

#ifdef UNIT_TEST

main(int argc, char **argv)
{
    int ind;

    if (argc == 1 ) {
	fprintf(stderr, "missing str_to_bytes() parameteres\n");
	exit(1);
    }
   
    for (ind=1; ind<argc; ind++) {

	printf("str_to_bytes(%s) returned %d\n", 
	    argv[ind], str_to_bytes(argv[ind]));

	printf("str_to_lbytes(%s) returned %ld\n", 
	    argv[ind], str_to_lbytes(argv[ind]));

	printf("str_to_llbytes(%s) returned %lld\n", 
	    argv[ind], str_to_llbytes(argv[ind]));
    }
}

#endif
