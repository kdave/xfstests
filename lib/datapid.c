/*
 * Copyright (c) 2000 Silicon Graphics, Inc.
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
/************

64 bits in a Cray word

				12345678901234567890123456789012
1234567890123456789012345678901234567890123456789012345678901234
________________________________________________________________
<    pid       >< word-offset in file (same #) ><    pid       >

1234567890123456789012345678901234567890123456789012345678901234
________________________________________________________________
<    pid       >< offset in file of this word  ><    pid       >


8 bits to a bytes == character
 NBPW            8 
************/

#include <stdio.h>
#include <sys/param.h>
#ifdef UNIT_TEST
#include <unistd.h>
#include <stdlib.h>
#endif

static char Errmsg[80];

#define LOWER16BITS(X)	(X & 0177777)
#define LOWER32BITS(X)	(X & 0xffffffff)

/***
#define HIGHBITS(WRD, bits) ( (-1 << (64-bits)) & WRD)
#define LOWBITS(WRD, bits) ( (-1 >> (64-bits)) & WRD)
****/

#define NBPBYTE		8		/* number bits per byte */

#ifndef DEBUG
#define DEBUG	0
#endif

/***********************************************************************
 *
 * 
 * 1   2   3   4   5   6   7   8   9   10  11  12  13  14  14  15	bytes
 * 1234567890123456789012345678901234567890123456789012345678901234	bits
 * ________________________________________________________________	1 word
 * <    pid       >< offset in file of this word  ><    pid       >
 * 
 * the words are put together where offset zero is the start.
 * thus, offset 16 is the start of  the second full word
 * Thus, offset 8 is in middle of word 1
 ***********************************************************************/
int
datapidgen(pid, buffer, bsize, offset)
int pid;
char *buffer;
int bsize;
int offset;
{
	return -1;	/* not support on non-64 bits word machines  */


} 

/***********************************************************************
 *
 *
 ***********************************************************************/
int
datapidchk(pid, buffer, bsize, offset, errmsg)
int pid;
char *buffer;
int bsize;
int offset;
char **errmsg;
{
    if ( errmsg != NULL ) {
        *errmsg = Errmsg;
    }
    sprintf(Errmsg, "Not supported on this OS.");
    return 0;

}       /* end of datapidchk */

#if UNIT_TEST

/***********************************************************************
 * main for doing unit testing
 ***********************************************************************/
int
main(ac, ag)
int ac;
char **ag;
{

int size=1234;
char *buffer;
int ret;
char *errmsg;

    if ((buffer=(char *)malloc(size)) == NULL ) {
        perror("malloc");
        exit(2);
    }


    datapidgen(-1, buffer, size, 3);

/***
fwrite(buffer, size, 1, stdout);
fwrite("\n", 1, 1, stdout);
****/

    printf("datapidgen(-1, buffer, size, 3)\n");

    ret=datapidchk(-1, buffer, size, 3, &errmsg);
    printf("datapidchk(-1, buffer, %d, 3, &errmsg) returned %d %s\n",
        size, ret, errmsg);
    ret=datapidchk(-1, &buffer[1], size-1, 4, &errmsg);
    printf("datapidchk(-1, &buffer[1], %d, 4, &errmsg) returned %d %s\n",
        size-1, ret, errmsg);

    buffer[25]= 0x0;
    buffer[26]= 0x0;
    buffer[27]= 0x0;
    buffer[28]= 0x0;
    printf("changing char 25-28\n");

    ret=datapidchk(-1, &buffer[1], size-1, 4, &errmsg);
    printf("datapidchk(-1, &buffer[1], %d, 4, &errmsg) returned %d %s\n",
        size-1, ret, errmsg);

printf("------------------------------------------\n");

    datapidgen(getpid(), buffer, size, 5);

/*******
fwrite(buffer, size, 1, stdout);
fwrite("\n", 1, 1, stdout);
******/

    printf("\ndatapidgen(getpid(), buffer, size, 5)\n");

    ret=datapidchk(getpid(), buffer, size, 5, &errmsg);
    printf("datapidchk(getpid(), buffer, %d, 5, &errmsg) returned %d %s\n",
        size, ret, errmsg);

    ret=datapidchk(getpid(), &buffer[1], size-1, 6, &errmsg);
    printf("datapidchk(getpid(), &buffer[1], %d, 6, &errmsg) returned %d %s\n",
        size-1, ret, errmsg);

    buffer[25]= 0x0;
    printf("changing char 25\n");

    ret=datapidchk(getpid(), &buffer[1], size-1, 6, &errmsg);
    printf("datapidchk(getpid(), &buffer[1], %d, 6, &errmsg) returned %d %s\n",
        size-1, ret, errmsg);

    exit(0);
}

#endif

