/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <malloc.h>
#ifdef linux
#include <dmapi.h>
#include <getopt.h>
#else
#include <sys/dmi.h>
#endif

int
main( int argc, char **argv )
{
	extern char *optarg;
	int c;
	int ret;
	dm_token_t *tokenbuf;
	u_int nelem = 100;
	u_int rnelem = 0;
	dm_sessid_t sid = 0;
	int i;
	char *versionstr;

	while( (c = getopt(argc, argv, "hs:n:")) != -1 ) {
		switch(c){
		case 's':
			sid = atoi( optarg );
			break;
		case 'n':
			nelem = atoi( optarg );
			break;
		case 'h':
			fprintf(stderr, "Usage: %s <-s sid> [-n nelem]\n", argv[0]);
			exit(2);
		}
	}

	if( sid == 0 ){
		fprintf(stderr, "%s: must specify -s\n", argv[0] );
		exit(1);
	}

	if( (tokenbuf = malloc( sizeof(dm_token_t) * nelem )) == NULL ){
		fprintf(stderr, "%s: malloc failed\n", argv[0] );
		exit(1);
	}

	if( dm_init_service( &versionstr ) < 0 )
		exit(1);

	ret = dm_getall_tokens( sid, nelem, tokenbuf, &rnelem );
	printf( "ret=%d\n", ret );
	printf( "rnelem=%d\n", rnelem );

	printf("tokens=\"");
	for( i = 0; i < rnelem; i++ ){
		printf("%d ", (int)(tokenbuf+i));
	}
	printf("\"\n");
	exit(0);
}
