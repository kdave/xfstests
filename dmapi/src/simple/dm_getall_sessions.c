/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <string.h>
#include <malloc.h>
#include <getopt.h>
#ifdef linux
#include <dmapi.h>
#endif
#ifdef __sgi
#include <sys/dmi.h>
#endif

int
main( int argc, char **argv )
{
	extern char *optarg;
	int c;
	int ret;
	dm_sessid_t *sidbuf;
	u_int nelem = 100;
	u_int rnelem = 0;
	int i;
	char *versionstr;
	int anyway = 0; /* attempt to show this many elements anyway,
			 * even if it looks like nothing was returned.
			 */

	while( (c = getopt(argc, argv, "hn:x:")) != -1 ) {
		switch(c){
		case 'n':
			nelem = atoi( optarg );
			break;
		case 'x':
			anyway = atoi( optarg );
			break;
		case 'h':
			fprintf(stderr, "Usage: %s [-n nelem]\n", argv[0]);
			exit(2);
		}
	}
	
	if( (sidbuf = malloc( sizeof(*sidbuf) * nelem )) == NULL ){
		fprintf(stderr, "%s: malloc failed\n", argv[0] );
		exit(1);
	}

	memset( sidbuf, 0, sizeof(*sidbuf) * nelem );

	if( dm_init_service( &versionstr ) < 0 )
		exit(1);

	ret = dm_getall_sessions( nelem, sidbuf, &rnelem );
	printf( "ret=%d\n", ret );
	printf( "rnelem=%d\n", rnelem );

	/* user wants us to try to show a specific number of elements */
	if( anyway > 0 )
		rnelem = anyway;

	printf("sids=\"");
	for( i = 0; i < rnelem; i++ ){
		printf("%d ", sidbuf[i]);
	}
	printf("\"\n");
	exit(0);
}

