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
		printf("%d ", (int)*(tokenbuf+i));
	}
	printf("\"\n");
	exit(0);
}
