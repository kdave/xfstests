/*
 * Copyright (c) 2001 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This prog is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This prog is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this prog with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this prog; if not, write the Free Software Foundation, Inc., 59
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

/*
 * Get an access or default acl on a file
 * using IRIX semantics or Linux semantics
 */
 
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/acl.h>
#include <acl/libacl.h>

char *prog;

void usage(void)
{
    fprintf(stderr, "usage: %s [-a] [-d] [-f] [-i] path\n"
           "flags:\n"
           "    -a - get access ACL\n"
           "    -d - get default ACL\n" 
           "    -f - get access ACL using file descriptor\n" 
           ,prog);
           
}



int
main(int argc, char **argv)
{
	int c;
        char *file;
	char *acl_text;
	int getaccess = 0;
	int getdefault = 0;
	int usefd = 0;
	int fd = -1;
	acl_t acl;

        prog = basename(argv[0]);

	while ((c = getopt(argc, argv, "adf")) != -1) {
		switch (c) {
		case 'a':
			getaccess = 1;
			break;
		case 'd':
			getdefault = 1;
			break;
		case 'f':
			usefd = 1;
			break;
		case '?':
                        usage();
			return 1;
		}
	}

	if (getdefault && usefd) {
	    fprintf(stderr, "%s: -f and -d are not compatible\n", prog);
	    return 1;
	}

        /* need path */
        if (optind == argc) {
            usage();
            exit(1);
        }
	else {
	    file = argv[optind];
	} 

	if (usefd) {
	    fd = open(file, O_RDONLY);
	    if (fd < 0) {
		fprintf (stderr, "%s: error opening \"%s\": %s\n",
			     prog, file, strerror(errno));
	 	usage(); 
		return 1;

	    }	
	}

        if (getaccess) {
	    if (usefd) { 
		acl = acl_get_fd(fd);
	    }
	    else {
		acl = acl_get_file(file, ACL_TYPE_ACCESS);
	    }
	    if (acl == NULL) {
		fprintf(stderr, "%s: error getting access ACL on \"%s\": %s\n",
			     prog, file, strerror(errno));
		return 0;
	    }
	    acl_text = acl_to_any_text(acl, NULL, ',', TEXT_ABBREVIATE);
	    if (acl_text == NULL) {
	    	fprintf(stderr, "%s: cannot get access ACL text on '%s': %s\n",
			prog, file, strerror(errno));
	    	return 0;
	    }
	    printf("%s: access %s", file, acl_text);
	    acl_free(acl_text);
	    acl_free(acl);
	}

        if (getdefault) {
	    acl = acl_get_file(file, ACL_TYPE_DEFAULT);
	    if (acl == NULL) {
		fprintf(stderr, "%s: error getting default ACL on \"%s\": %s\n",
			     prog, file, strerror(errno));
		return 0;
	    }
	    acl_text = acl_to_any_text(acl, NULL, ',', TEXT_ABBREVIATE);
	    if (acl_text == NULL) {
	    	fprintf(stderr, "%s: cannot get default ACL text on '%s': %s\n",
			prog, file, strerror(errno));
	    	return 0;
	    }
	    printf("%s: default %s", file, acl_text);
	    acl_free(acl_text);
	    acl_free(acl);
	}

	return 0;
}
