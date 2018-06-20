// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.
 * All Rights Reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
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
	dm_sessid_t sid = 0;
	char *versionstr;

	while( (c = getopt(argc, argv, "hs:")) != -1 ) {
		switch(c){
		case 's':
			sid = atoi( optarg );
			break;
		case 'h':
			fprintf(stderr, "Usage: %s <-s sid>\n", argv[0] );
			exit(2);
		}
	}

	if( sid == 0 ){
		fprintf(stderr, "%s: must specify -s\n", argv[0] );
		exit(1);
	}

	if( dm_init_service( &versionstr ) < 0 )
		exit(1);

	ret = dm_destroy_session( sid );
	printf( "ret=%d\n", ret );
	exit(0);
}
