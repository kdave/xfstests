/*
 * Simple Mr. Mean that can manipulate and torch sessions
 *
 * This code was written by Peter Lawthers, and placed in the public
 * domain for use by DMAPI implementors and app writers.
 *
 * Standard disclaimer:
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/errno.h>

#include <lib/dmport.h>

#include <string.h>
#include <getopt.h>

extern char	*optarg;
extern int	 optind, opterr, optopt;
extern int	 errno;

int		 Verbose;
char		*Progname;

extern void	err_msg(char *, ...);
extern void	errno_msg(char *, ...);

int 		get_sessions(dm_sessid_t **, u_int *);
int		get_tokens(dm_sessid_t, dm_token_t **, u_int *);
void		print_session(dm_sessid_t);
void		print_tokens(dm_sessid_t);
void		kill_session(dm_sessid_t);

void
usage(char *s)
{
	fprintf(stderr, "Usage: %s <options>\n", s);
	fprintf(stderr, "\t-t	list tokens\n");
	fprintf(stderr, "\t-l	list sessions\n");
	fprintf(stderr, "\t-k	kill sessions\n");
	fprintf(stderr, "\t-s	<specific_sid>\n");
	fprintf(stderr, "\t-v	verbose (for kill)\n");
}

int
main(
	int	 argc,
	char	*argv[])
{
	int		 c;
	int		 error;
	u_int		 i, nsids;
	int		 list_flag, kill_flag, token_flag, sid_flag;
	dm_sessid_t	*sidbuf, *sidp, onesid;
	char		*cp;


	Progname = argv[0];
	list_flag = sid_flag = kill_flag = token_flag = 0;

	while ((c = getopt(argc, argv, "vlkts:")) != EOF) {
		switch (c) {
		case 'l':
			list_flag = 1;
			break;
		case 'k':
			kill_flag = 1;
			break;
		case 't':
			token_flag = 1;
			break;
		case 's':
			if (sscanf(optarg, "%d", &onesid) <=0 ) {
				err_msg("Can't convert sid %s", optarg);
				exit(2);
			}
			sid_flag = 1;
			break;
		case 'v':
			Verbose = 1;
			break;
		case '?':
		default:
			usage(Progname);
			exit(1);
		}
	}
	if (!list_flag && !sid_flag && !kill_flag && !token_flag) {
		usage(Progname);
		exit(1);
	}

        if (dm_init_service(&cp) == -1)  {
                err_msg("Can't init dmapi");
                return(1);
        }
        if (strcmp(cp, DM_VER_STR_CONTENTS)) {
                err_msg("Compiled for a different version");
                return(1);
        }


	/*
	 * Get the list of all sessions in the system
	 */
	error = get_sessions(&sidbuf, &nsids);
	if (error)
		exit(1);

	/*
	 * Walk through the list of sessions do what is right
	 */
	sidp = sidbuf;
	for (i=0; i<nsids; i++, sidp++) {
		if (sid_flag) {
			/*
			 * If we're only looking for one sid, then
			 * we can skip this one if there's no match
			 */
			if (onesid != *sidp)
				continue;
		}
		if (list_flag)
			print_session(*sidp);
		if (token_flag)
			print_tokens(*sidp);
		if (kill_flag)
			kill_session(*sidp);
	}
	return(0);
}

/*
 * Print out info about a sessions
 */
void
print_session(dm_sessid_t sid)
{
	char	buf[DM_SESSION_INFO_LEN];
	size_t	ret;

	if (dm_query_session(sid,DM_SESSION_INFO_LEN,(void *)buf, &ret) == -1) {
		errno_msg("Can't get session info");
		return;
	}

	printf("Session (%d) name: %s\n", sid, buf);
	return;
}

/*
 * Get all the tokens for a session
 */
void
print_tokens(dm_sessid_t sid)
{
	dm_token_t	*tbuf;
	int		 error;
	u_int		 i, ntokens;

	error = get_tokens(sid, &tbuf, &ntokens);
	if (error)
		return;

	printf("\tTokens (%d): ", ntokens);
	for (i=0; i<ntokens; i++) 
		printf("%d ", *tbuf++);

	printf("\n");

	free(tbuf);
	return;
}

/*
 * Try and kill a session
 */
void
kill_session(dm_sessid_t sid)
{
	dm_token_t	*tbuf;
	int		 error;
	u_int		 i, ntokens;

	/*
	 * Get all the tokens in the system so we can respond to them
	 */
	error = get_tokens(sid, &tbuf, &ntokens);
	if (error)
		return;

	if (ntokens && Verbose) 
		printf("\tResponding to events for sid %d, tokens: \n", sid);

	for (i=0; i<ntokens; i++) {
		if (Verbose)
			printf("\t\t%d ", *tbuf);

		if (dm_respond_event(sid, *tbuf, DM_RESP_ABORT, EIO, 0, NULL) == -1) 
			errno_msg("Can't respond to event, sid %d token %d", 
				   sid, *tbuf);
		tbuf++;
	}

	if (Verbose)
		printf("\tDestroying session %d\n", sid);
	if (dm_destroy_session(sid) == -1) 
		errno_msg("Can't shut down session %d", sid);
	return;
}

int
get_sessions(
	dm_sessid_t **sidpp,
	u_int	     *nsidp)
{
	dm_sessid_t	*sidbuf;
	int		 error;
	u_int	 	 nsids, nret;

	/*
	 * Pick an arbitrary number of sessions to get info for.
	 * If it's not enough, then we can always resize our buffer
	 */
	error = 0;
	nsids = 32;
	sidbuf = malloc(nsids * sizeof(dm_sessid_t));
	if (sidbuf == NULL) {
		err_msg("Can't malloc memory");
		error = 1;
		goto out;
	}
		
	if (dm_getall_sessions(nsids, sidbuf, &nret) == -1) {
		if (errno != E2BIG) {
			errno_msg("Can't get list of sessions");
			error = 1;
			goto out;
		}
		free(sidbuf);
		nsids = nret;
		sidbuf = malloc(nsids * sizeof(dm_sessid_t));
		if (sidbuf == NULL) {
			err_msg("Can't malloc memory");
			error = 1;
			goto out;
		}
		if (dm_getall_sessions(nsids, sidbuf, &nret) == -1) {
			if (error == -1) {
				errno_msg("Can't get sessions with new buf");
				error = 1;
				goto out;
			}
		}
	}
out:
	if (error && (sidbuf != NULL) )
		free(sidbuf);
	else {
		*sidpp = sidbuf;
		*nsidp = nret;
	}
	
	return(error);
}


/*
 * Get all tokens in the session
 */
int
get_tokens(
	dm_sessid_t	  sid,
	dm_token_t	**bufpp,
	u_int		 *nretp)
{
	dm_token_t	*tbuf;
	u_int		 ntokens, nret;
	int		 error;

	error   = 0;
	ntokens = 1024;
	tbuf = (dm_token_t *)malloc(ntokens * sizeof(dm_token_t));
	if (tbuf == NULL)  
		goto out;
	
	if (dm_getall_tokens(sid, ntokens, tbuf, &nret) == -1) {
		if (errno != E2BIG) {
			errno_msg("Can't get all tokens");
			goto out;
		}
		free(tbuf);
		ntokens = nret;
		tbuf = (dm_token_t *)malloc(ntokens * sizeof(dm_token_t));
		if (tbuf == NULL)  
			goto out;
		
		if (dm_getall_tokens(sid, ntokens, tbuf, &nret) == -1) {
			errno_msg("Can't get all tokens");
			goto out;
		}
	}
out:
	if (error && (tbuf != NULL))
		free(tbuf);
	else {
		*bufpp = tbuf;
		*nretp = nret;
	}

	return(error);
}
