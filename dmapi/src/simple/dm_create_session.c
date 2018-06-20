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
	dm_sessid_t oldsid = DM_NO_SESSION;
	dm_sessid_t newsid = 0;
	char *sessinfo = "test1";
	char *versionstr;

	while( (c = getopt(argc, argv, "hs:i:")) != -1 ) {
		switch(c){
		case 's':
			oldsid = atoi( optarg );
			break;
		case 'i':
			sessinfo = optarg;
			break;
		case 'h':
			fprintf(stderr, "Usage: %s [-s oldsid] [-i sessinfo_txt]\n", argv[0]);
			exit(2);
		}
	}

	if( dm_init_service( &versionstr ) < 0 )
		exit(1);

	ret = dm_create_session( oldsid, sessinfo, &newsid);
	printf( "ret=%d\n", ret );
	printf( "newsid=%d\n", newsid );
	exit(0);
}

