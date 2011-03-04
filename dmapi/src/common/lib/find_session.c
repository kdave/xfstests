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

/*******************************************************************************
*
* NAME
*	find_test_session - find or create a test DMAPI session.
*
* DESCRIPTION
*	find_test_session() is used to find a test DMAPI session to
*	use.  There is only one test session per host; all test processes
*	share the same session.  If the session does not already exist,
*	then the first process to call find_test_session() causes
*	the session to be created.
*
*	Note: It is possible for N different programs to call this routine
*	at the same time.  Each would find that a test session does not
*	exist, and each one would then create a new test session.  Since
*	excess test sessions are not automatically released on death of
*	process, we need to make sure that we don't leave such excess
*	sessions around.  So, after creating a session we go back and find
*	the test session with the lowest session number.  If it is ours,
*	great; we are done.  If not, then we must destroy our session
*	and use the one with the lower session ID.  There is still a risk
*	here of creating a session and crashing before it can be removed
*	again.  To deal with this, the daemon will periodically remove all
*	test sessions except for the one with the lowest ID value.
*
* RETURN VALUE
*	None.
*
*******************************************************************************/

#define	TEST_MSG	"DMAPI test session"

static int
session_compare(
const	void	*a,
const	void	*b)
{
	return(*((dm_sessid_t *)a) - *((dm_sessid_t *)b));
}

extern void
find_test_session(
	dm_sessid_t	*session)
{
	char		buffer[DM_SESSION_INFO_LEN];
	dm_sessid_t	*sidbuf = NULL;
	dm_sessid_t	new_session;
	u_int		allocelem = 0;
	u_int		nelem;
	size_t		rlen;
	int		error;
	u_int		i;

	/* Retrieve the list of all active sessions on the host. */

	nelem = 100;		/* a reasonable first guess */
	do {
		if (allocelem < nelem) {
			allocelem = nelem;
			sidbuf = realloc(sidbuf, nelem * sizeof(*sidbuf));
			if (sidbuf == NULL) {
				fprintf(stderr, "realloc of %lu bytes failed\n",
					(unsigned long) nelem * sizeof(*sidbuf));
				exit(1);
			}
		}
		error = dm_getall_sessions(allocelem, sidbuf, &nelem);
	} while (error < 0 && errno == E2BIG);

	/* If an error occurred, translate it into something meaningful. */

	if (error < 0) {
		fprintf(stderr, "unexpected dm_getall_sessions failure, %s\n",
			strerror(errno));
		free(sidbuf);
		exit(1);
	}

	/* We have the list of all active sessions.  Scan the list looking
	   for an existing "test" session that we can use.  The list must
	   first be sorted in case other processes happen to be creating test
	   sessions at the same time; we need to make sure that we pick the one
	   with the lowest session ID.
	*/

	qsort(sidbuf, nelem, sizeof(sidbuf[0]), session_compare);

	for (i = 0; i < nelem; i++) {
		error = dm_query_session(sidbuf[i], sizeof(buffer),
				buffer, &rlen);
		if (error < 0) {
			fprintf(stderr, "unexpected dm_query_session "
				"failure, %s\n", strerror(errno));
			free(sidbuf);
			exit(1);
		}

		if (!strncmp(buffer, TEST_MSG, strlen(TEST_MSG)))
			break;
	}
	if (i < nelem) {
		*session = (dm_sessid_t)sidbuf[i];
		free(sidbuf);
		return;
	}

	/* No test session exists, so we have to create one ourselves. */

	if (dm_create_session(DM_NO_SESSION, TEST_MSG, &new_session) != 0) {
		fprintf(stderr, "dm_create_session failed, %s\n",
			strerror(errno));
		free(sidbuf);
		exit(1);
	}

	/* Now re-retrieve the list of active sessions. */

	do {
		if (allocelem < nelem) {
			allocelem = nelem;
			sidbuf = realloc(sidbuf, nelem * sizeof(*sidbuf));
			if (sidbuf == NULL) {
				fprintf(stderr, "realloc of %lu bytes failed\n",
					(unsigned long) nelem * sizeof(*sidbuf));
				exit(1);
			}
		}
		error = dm_getall_sessions(allocelem, sidbuf, &nelem);
	} while (error < 0 && errno == E2BIG);

	if (error < 0) {
		fprintf(stderr, "dm_getall_sessions failed, %s\n",
			strerror(errno));
		free(sidbuf);
		exit(1);
	}

	/* Sort the session ID list into ascending ID order, then find the
	   test session with the lowest session ID.  We better find at
	   least one since we created one!
	*/

	qsort(sidbuf, nelem, sizeof(sidbuf[0]), session_compare);

	for (i = 0; i < nelem; i++) {
		error = dm_query_session(sidbuf[i], sizeof(buffer),
				buffer, &rlen);
		if (error < 0) {
			fprintf(stderr, "dm_query_session failed, %s\n",
				strerror(errno));
			free(sidbuf);
			exit(1);
		}
		if (!strncmp(buffer, TEST_MSG, strlen(TEST_MSG)))
			break;
	}
	if (i == nelem) {
		fprintf(stderr, "can't find the session we created\n");
		free(sidbuf);
		exit(1);
	}
	*session = (dm_sessid_t)sidbuf[i];
	free(sidbuf);

	/* If the session we are going to use is not the one we created,
	   then we need to get rid of the created one.
	*/

	if (*session != new_session) {
		if ((new_session) != 0) {
			fprintf(stderr, "dm_destroy_session failed, %s\n",
				strerror(errno));
			exit(1);
		}
	}
}
