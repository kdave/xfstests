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

Test program used to test the DMAPI function dm_get_eventlist().  The
command line is:

	get_eventlist [-F] [-n nelem] [-s sid] [-t token] {pathname|handle}

where:
{pathname|handle}
	is the pathname of a file or filesystem or a handle.
-n nelem
	is the value to use for the nelem parameter to dm_get_eventlist().
-s sid
	is the dm_sessid_t to use in place of the default test session.
-t token
	is the dm_token_t to use in place of DM_NO_TOKEN.
-F
	is used when a pathname is specified to indicate that you want its
	filesystem handle, not its file handle.

----------------------------------------------------------------------------*/

extern  int     optind;
extern  char    *optarg;


char	*Progname;



static void
usage(void)
{
	fprintf(stderr, "usage:\t%s [-F] [-n nelem] [-s sid] [-t token] "
		"{pathname|handle}\n",
		Progname);
	exit(1);
}


int
main(
	int	argc, 
	char	**argv)
{
	dm_sessid_t	 sid = DM_NO_SESSION;
	dm_token_t	token = DM_NO_TOKEN;
	u_int		nelem = DM_EVENT_MAX;
	char		*object;
	dm_eventset_t	eventset;
	void		*hanp;
	size_t	 	 hlen;
	u_int		nelemp;
	char		*name;
	int		Fflag = 0;
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

	while ((opt = getopt(argc, argv, "Fn:s:t:")) != EOF) {
		switch (opt) {
		case 'F':
			Fflag++;
			break;
		case 'n':
			nelem = atol(optarg);
			break;
		case 's':
			sid = atol(optarg);
			break;
		case 't':
			token = atol(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (optind + 1 != argc)
		usage();
	object = argv[optind];

	if (dm_init_service(&name) == -1)  {
		fprintf(stderr, "Can't initialize the DMAPI\n");
		exit(1);
	}

	if ((error = opaque_to_handle(object, &hanp, &hlen)) != 0) {
		fprintf(stderr, "can't get a handle from %s, %s\n",
			object, strerror(error));
		return(1);
	}

	if (Fflag) {
		void	*fshanp;
		size_t	fshlen;

		if (dm_handle_to_fshandle(hanp, hlen, &fshanp, &fshlen)) {
			fprintf(stderr, "can't get filesystem handle from %s\n",
				object);
			exit(1);
		}
		dm_handle_free(hanp, hlen);
		hanp = fshanp;
		hlen = fshlen;
	}

	if (sid == DM_NO_SESSION)
		find_test_session(&sid);

	DMEV_ZERO(eventset);

	if (dm_get_eventlist(sid, hanp, hlen, token, nelem,
	    &eventset, &nelemp)) {
		fprintf(stderr, "dm_get_eventlist failed, %s\n",
			strerror(errno));
		return(1);
	}

	fprintf(stdout, "Events on object %s (0x%llx), nelemp %d:\n",
		object, (unsigned long long) eventset, nelemp);

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
