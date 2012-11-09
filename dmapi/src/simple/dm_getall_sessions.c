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
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <string.h>
#include <malloc.h>
#include <getopt.h>
#ifdef linux
#include <dmapi.h>
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

