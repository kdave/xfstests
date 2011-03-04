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

#include <lib/hsm.h>

#include <string.h>
#include <getopt.h>

/*---------------------------------------------------------------------------

Test program used to test the DMAPI function dm_get_config_events().  The
command line is:

	get_config_events [-n nelem] handle

where handle is the handle of a file or filesystem, and nelem is the value
to use for the nelem parameter to dm_get_eventlist().

----------------------------------------------------------------------------*/

extern  int     optind;
extern  char    *optarg;


char	*Progname;



static void
usage(void)
{
	fprintf(stderr, "usage:\t%s [-n nelem] handle\n", Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	u_int		nelem = DM_EVENT_MAX;
	char		*han_str;
	dm_eventset_t	eventset;
	void		*hanp;
	size_t	 	 hlen;
	u_int		nelemp;
	char		*name;
	int		error;
	int		opt;
	int		i;

	Progname = strrchr(argv[0], '/');
	if (Progname) {
		Progname++;
	} else {
		Progname = argv[0];
	}

	/* Crack and validate the command line options. */

	while ((opt = getopt(argc, argv, "n:")) != EOF) {
		switch (opt) {
		case 'n':
			nelem = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (optind + 1 != argc)
		usage();
	han_str = argv[optind];
	if ((error = atohan(han_str, &hanp, &hlen)) != 0) {
		fprintf(stderr, "atohan() failed, %s\n", strerror(error));
		return(1);
	}

	if (dm_init_service(&name))  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		return(1);
	}

	DMEV_ZERO(eventset);

	if (dm_get_config_events(hanp, hlen, nelem, &eventset, &nelemp)) {
		fprintf(stderr, "dm_get_config_events failed, %s\n",
			strerror(errno));
		return(1);
	}

	fprintf(stdout, "Events supported (0x%llx), nelemp %d:\n",
		(unsigned long long) eventset, nelemp);

	for (i = 0; i < nelemp; i++) {
		if (!DMEV_ISSET(i, eventset))
			continue;
		switch (i) {
		case DM_EVENT_CANCEL:
			fprintf(stdout, "DM_EVENT_CANCEL");
			break;
		case DM_EVENT_MOUNT:
			fprintf(stdout, "DM_EVENT_MOUNT");
			break;
		case DM_EVENT_PREUNMOUNT:
			fprintf(stdout, "DM_EVENT_PREUNMOUNT");
			break;
		case DM_EVENT_UNMOUNT:
			fprintf(stdout, "DM_EVENT_UNMOUNT");
			break;
		case DM_EVENT_DEBUT:
			fprintf(stdout, "DM_EVENT_DEBUT");
			break;
		case DM_EVENT_CREATE:
			fprintf(stdout, "DM_EVENT_CREATE");
			break;
		case DM_EVENT_CLOSE:
			fprintf(stdout, "DM_EVENT_CLOSE");
			break;
		case DM_EVENT_POSTCREATE:
			fprintf(stdout, "DM_EVENT_POSTCREATE");
			break;
		case DM_EVENT_REMOVE:
			fprintf(stdout, "DM_EVENT_REMOVE");
			break;
		case DM_EVENT_POSTREMOVE:
			fprintf(stdout, "DM_EVENT_POSTREMOVE");
			break;
		case DM_EVENT_RENAME:
			fprintf(stdout, "DM_EVENT_RENAME");
			break;
		case DM_EVENT_POSTRENAME:
			fprintf(stdout, "DM_EVENT_POSTRENAME");
			break;
		case DM_EVENT_LINK:
			fprintf(stdout, "DM_EVENT_LINK");
			break;
		case DM_EVENT_POSTLINK:
			fprintf(stdout, "DM_EVENT_POSTLINK");
			break;
		case DM_EVENT_SYMLINK:
			fprintf(stdout, "DM_EVENT_SYMLINK");
			break;
		case DM_EVENT_POSTSYMLINK:
			fprintf(stdout, "DM_EVENT_POSTSYMLINK");
			break;
		case DM_EVENT_READ:
			fprintf(stdout, "DM_EVENT_READ");
			break;
		case DM_EVENT_WRITE:
			fprintf(stdout, "DM_EVENT_WRITE");
			break;
		case DM_EVENT_TRUNCATE:
			fprintf(stdout, "DM_EVENT_TRUNCATE");
			break;
		case DM_EVENT_ATTRIBUTE:
			fprintf(stdout, "DM_EVENT_ATTRIBUTE");
			break;
		case DM_EVENT_DESTROY:
			fprintf(stdout, "DM_EVENT_DESTROY");
			break;
		case DM_EVENT_NOSPACE:
			fprintf(stdout, "DM_EVENT_NOSPACE");
			break;
		case DM_EVENT_USER:
			fprintf(stdout, "DM_EVENT_USER");
			break;
		case DM_EVENT_MAX:
			fprintf(stdout, "DM_EVENT_23");
			break;
		}
		fprintf(stdout, " (%d)\n", i);
	}

	dm_handle_free(hanp, hlen);
	return(0);
}
