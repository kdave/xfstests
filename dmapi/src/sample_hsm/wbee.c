/*
 * Worker bees.
 *
 * The bees perform the grunt work of handling a file event
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <lib/hsm.h>

extern char	*optarg;
extern int	 optind, optopt, opterr;
extern int	 errno;
char		*Progname;
int		 Verbose;

extern int	 restore_filedata(dm_sessid_t, void *, size_t, dm_token_t, 
				void *, size_t, dm_off_t);
extern int	 get_dmchange(dm_sessid_t, void *, size_t, dm_token_t, u_int *);
extern int	 setup_dmapi(dm_sessid_t *);
extern void	 err_msg(char *, ...);
extern void	 errno_msg(char *, ...);

int	stagein_file(dm_sessid_t, dm_token_t, dm_eventmsg_t *);
int	inval_file(dm_sessid_t, dm_token_t, dm_eventmsg_t *);
int	check_lockstate(dm_sessid_t, void *, size_t, dm_token_t);
int	clear_mrgns(dm_sessid_t, void *, size_t, dm_token_t);
int	find_msg(dm_sessid_t, dm_token_t, dm_eventmsg_t	**);
int	get_stghandle(dm_sessid_t, void *, size_t, dm_token_t, void **, 
			size_t *);
void    usage(char *);



void
usage(
	char *prog)
{
	fprintf(stderr, "Usage: %s ", prog);
	fprintf(stderr, " <-i invalidate file> ");
	fprintf(stderr, " <-r restore file> ");
	fprintf(stderr, "[-s sid] [-t token] \n");
}


int
main(
	int	argc, 
	char	*argv[])
{
	
	dm_eventmsg_t	*msgheader;
	char		*sid_str, *token_str;
	dm_sessid_t	 sid;
	dm_token_t	 token;
	int 	 	 c;
	int 	 	 error;
	int	 	 restore_flag, inval_flag;
	char		*cp;

	Progname  = argv[0];
	sid_str   = NULL;
	token_str = NULL;
	restore_flag = 0;
	inval_flag   = 0;

	while ((c = getopt(argc, argv, "s:t:ri")) != EOF) {
		switch (c) {
		case 'r':
			restore_flag++;
			break;

		case 'i':
			inval_flag++;
			break;

		case 's':
			sid_str = optarg;
			break;

		case 't':
			token_str = optarg;
			break;

		case '?':
		default:
			usage(Progname);
			exit(1);
		}
	}
	if (optind < argc) {
		usage(Progname);
		exit(1);
	}
	if (sid_str == NULL || token_str == NULL) {
		usage(Progname);
		exit(1);
	}
	if ((restore_flag > 0) && (inval_flag > 0)) {
		usage(Progname);
		exit(1);
	}

	if (sscanf(sid_str, "%d", &sid) <= 0) {
		err_msg("Can't convert sid");
		exit(1);
	}
	if (sscanf(token_str, "%d", &token) <= 0) {
		err_msg("Can't convert token");
		exit(1);
	}

	/*
	 * Now we have our session and token. We just need to
	 * let the DMAPI know we exist so we can use the interface.
	 * We don't need to create a session since we'll be using
	 * the session of our parent.
	 */
	error = dm_init_service(&cp);
	if (error == -1) {
		errno_msg("Can't init DMAPI");
		exit(1);
	}
	if (strcmp(cp, DM_VER_STR_CONTENTS)) {
		err_msg("Compiled for a different version");
		exit(1);
	}
	
	/*
	 * Find the message our caller wants us to handle
	 */
	error = find_msg(sid, token, &msgheader);
	if (error) 
		exit(1);

	/*
	 * Now service the particular event type
	 */
	if (restore_flag) 
		error = stagein_file(sid, token, msgheader);
	else
		error = inval_file(sid, token, msgheader);

	return(error);
}


/*
 * Find the data event message that correponds to the token.
 *
 * RETURNS:
 *	A pointer to malloc'd memory that contains the message
 * 	we're supposed to handle in the 'msgheader' param.
 */
int
find_msg(
	dm_sessid_t	  sid,
	dm_token_t	  token,
	dm_eventmsg_t	**msgheader)
{
	void	*buf;
	size_t	 buflen, rlen;
	int	 error;

	/*
	 * Malloc a buffer that we think is large enough for
	 * the common message header and the event specific part. 
	 * If it's not large enough, we can always resize it.
	 */
	buflen = sizeof(dm_eventmsg_t) + sizeof(dm_data_event_t) + HANDLE_LEN;
	buf = (void *)malloc(buflen);
	if (buf == NULL) {
		err_msg("Can't alloc memory for event buffer");
		return(1);
	}

	error = dm_find_eventmsg(sid, token, buflen, buf, &rlen);
	if (error == -1) {
		if (errno != E2BIG) {
			free(buf);
			errno_msg("Can't obtain message from token");
			return(1);
		}
		free(buf);
		buflen = rlen;
		buf = (void *)malloc(buflen);
		if (buf == NULL) {
			err_msg("Can't resize event buffer");
			return(1);
		}
		error = dm_find_eventmsg(sid, token, buflen, buf, &rlen);
		if (error == -1) {
			errno_msg("Can't get message with resized buffer");
			return(1);
		}
	}
	
	*msgheader = (dm_eventmsg_t *)buf;
	return(0);
}


/*
 * Check the lock state associated with the file. If the token
 * does not reference exclusive access, try to upgrade our lock.
 * If we can't upgrade, drop the lock and start over
 */
int
check_lockstate(
	dm_sessid_t	 sid, 
	void		*hanp, 
	size_t		 hlen, 
	dm_token_t	 token)
{
	int		error;
	dm_right_t	right;
	u_int		change_start, change_end;

	error = dm_query_right(sid, hanp, hlen, token, &right);
	if (error == -1) {
		errno_msg("Can't query file access rights");
		return(1);
	}
#if defined(linux)
	/*
 	 * There are no access rights on the SGI. 1 means it's
	 * there.
	 */
	if (right == DM_RIGHT_SHARED)
		return 0;
#endif

	if (right != DM_RIGHT_EXCL) {
		error = dm_request_right(sid, hanp, hlen, token, 0,
					 DM_RIGHT_EXCL);
		if (error == -1) {
			if (errno != EAGAIN) {
				errno_msg("Can't upgrade lock");
				return(1);
			}
			error = get_dmchange(sid, hanp, hlen, token, 
						&change_start);
			if (error) 
				return(1);
				
			
			error = dm_release_right(sid, hanp, hlen, token);
			if (error == -1) {
				errno_msg("Can't release file access rights");
				return(1);
			}
			error = dm_request_right(sid, hanp, hlen, token, 
						DM_RR_WAIT, DM_RIGHT_EXCL);
			if (error == -1)  {
				errno_msg("Can't get exclusive right to file");
				return(1);
			}

			/*
			 * If the file changed while we slept, then someone
			 * must have modified the file
			 */
			error = get_dmchange(sid, hanp, hlen, token, 
						&change_end);
			if (error == -1) 
				return(1);
			
			if (change_start != change_end) {
				err_msg("File changed while waiting for lock");
				return(1);
			}
		}
	}
	return(0);
}


/*
 * Stage in the data for a file
 */
int
stagein_file(
	dm_sessid_t	 sid, 
	dm_token_t	 token,
	dm_eventmsg_t	*msgheader)
{

	void		*stg_hanp, *hanp;
	size_t	 	 stg_hlen, hlen;
	int	 	 error, ret_errno;
	dm_response_t	 reply;
	dm_data_event_t	*msg;

	/*
	 * Extract the event-specific info from the message header,
	 * then get the file handle.
	 */
	msg  = DM_GET_VALUE(msgheader, ev_data, dm_data_event_t *);
	hanp = DM_GET_VALUE(msg, de_handle, void *);
	hlen = DM_GET_LEN(msg, de_handle);

	/*
	 * Check our permissions. We need exclusive access to the 
	 * file to stage it back in.
	 */
	error = check_lockstate(sid, hanp, hlen, token);
	if (error) 
		goto out;

	/*
	 * get the staging file handle from it's DM attributes
	 */
	stg_hanp = NULL;
	error = get_stghandle(sid, hanp, hlen, token, &stg_hanp, &stg_hlen);
	if (error) 
		goto out;

	/*
	 * We keep the exclusive lock held for the *entire* duration
	 * of the stagein. This is not required, but just quick and
	 * [sl]easy. For a large file, it is typically better to release
	 * the lock, and have a sliding window of managed regions to allow
	 * people to consume the data as it is being read in.
	 */
	error = restore_filedata(sid, hanp, hlen, token, stg_hanp, stg_hlen,
			   msg->de_offset);
	if (error) 
		goto out;

	/*
	 * Now that the data is restored, and while we still have exclusive
	 * access to the file, clear the managed regions.
	 */
	error = clear_mrgns(sid, hanp, hlen, token);
	if (error) 
		goto out;

out:
	if (stg_hanp)
		free((char *)stg_hanp);

	/*
	 * Figure out what our response to the event will be. Once
	 * we've responded to the event, the token is no longer valid.
	 * On error, we pick the (less than helpful) errno EIO to signal
	 * to the user that something went wrong.
	 */
	if (error) {
		reply = DM_RESP_ABORT;
		ret_errno = EIO;
	} else {
		reply = DM_RESP_CONTINUE;
		ret_errno = 0;
	}
	(void)dm_respond_event(sid, token, reply, ret_errno, 0, 0);

	return(ret_errno);

}

/*
 * Turn off event monitoring for a file. In a real HSM, we would
 * probably want to either invalidate the file's data on 
 * tertiary storage, or start some aging process so that it will
 * eventually go away.
 *
 * The assumption is that for write and truncate events, the file
 * data is about to be invalidated.
 */
int
inval_file(
	dm_sessid_t	 sid, 
	dm_token_t	 token,
	dm_eventmsg_t	*msgheader)
{
	dm_data_event_t	*msg;
	void		*hanp;
	size_t	 	 hlen;
	int	 	 error, ret_errno;
	dm_response_t	 reply;

	/*
	 * Extract the event-specific info from the message header,
	 * then get the file handle.
	 */
	msg  = DM_GET_VALUE(msgheader, ev_data, dm_data_event_t *);
	hanp = DM_GET_VALUE(msg, de_handle, void *);
	hlen = DM_GET_LEN(msg, de_handle);

	/*
	 * Check our permissions. We need exclusive access to the 
	 * file to clear our managed regions.
	 */
	error = check_lockstate(sid, hanp, hlen, token);
	if (error) 
		goto out;

	/*
	 * Clear all the managed regions for the file.
	 */
	error = clear_mrgns(sid, hanp, hlen, token);

out:
	/*
	 * Figure out what our response to the event will be. Once
	 * we've responded to the event, the token is no longer valid.
	 * On error, we pick the (less than helpful) errno EIO to signal
	 * to the user that something went wrong.
	 */
	if (error) {
		reply = DM_RESP_ABORT;
		ret_errno = EIO;
	} else {
		reply = DM_RESP_CONTINUE;
		ret_errno = 0;
	}
	(void)dm_respond_event(sid, token, reply, ret_errno, 0, 0);
	
	return(ret_errno);
}

/*
 * Clear all of the managed regions for a file. 
 */
int
clear_mrgns(
	dm_sessid_t	 sid, 
	void		*hanp, 
	size_t		 hlen, 
	dm_token_t	 token)
{
	dm_region_t	*rgn, *sv_rgn;
	u_int		 nregions, nret;
	u_int		 exact_flag;
	int		 i;
	int		 error, retval;

	/*
	 * We take a guess first and assume there is only one managed
	 * region per file. There should'nt be more than this, but
	 * it never hurts to check, since we want to make sure that
	 * all regions are turned off.
	 *
	 * The main purpose of this is to demonstrate the use of the 
	 * E2BIG paradigm.
	 */
	retval = 1;
	nregions = 1;
	rgn = (dm_region_t *)malloc(nregions * sizeof(dm_region_t));
	if (rgn == NULL) {
		err_msg("Can't allocate memory for region buffers");
		goto out;
	}

	error = dm_get_region(sid, hanp, hlen, token, nregions, rgn, &nret);
	if (error == -1) {
		if (errno != E2BIG) {
			errno_msg("Can't get list of managed regions for file");
			goto out;
		}

		/*
		 * Now we know how many managed regions there are, so we can
		 * resize our buffer
		 */
		nregions = nret;
		free(rgn);
		rgn = (dm_region_t *)malloc(nregions * sizeof(dm_region_t));
		if (rgn == NULL) {
			err_msg("Can't resize region buffers");
			goto out;
		}
		error = dm_get_region(sid, hanp, hlen, token, nregions, rgn, 
					&nret);
		if (error == -1) {
			errno_msg("Can't get list of managed regions for file");
			goto out;
		}
	}

	sv_rgn = rgn;

	/*
	 * Clear all the managed regions
	 */
	for (i=0; i<nregions; i++) {
		rgn->rg_offset = 0;
		rgn->rg_size   = 0;
		rgn->rg_flags  = DM_REGION_NOEVENT;
		rgn++;
	}
	rgn = sv_rgn;

	error = dm_set_region(sid, hanp, hlen, token, nregions, rgn, 
				&exact_flag);
	if (error == -1) {
		errno_msg("Can't clear list of managed regions for file");
	}
	retval = 0;

out:
	if (rgn != NULL) 
		free(rgn);

	return(retval);
}


/*
 * Extract the staging file handle from a file's DM attributes
 */
int
get_stghandle(
	dm_sessid_t	  sid, 
	void		 *hanp, 
	size_t		  hlen, 
	dm_token_t	  token,
	void		**stg_hanp,
	size_t		 *stg_hlen)
{
	void		*han_buf;
	size_t		 han_len;
	int		 error;
	size_t		 rlen;
	dm_attrname_t	 hanp_attrname;
	dm_attrname_t	 hlen_attrname;

	/*
	 * First get the length of the file handle, so we
	 * can size our buffer correctly
	 */
	memcpy((void *)&hlen_attrname.an_chars[0], DLOC_HANLEN, DM_ATTR_NAME_SIZE);
	error = dm_get_dmattr(sid, hanp, hlen, token, &hlen_attrname,
				sizeof(size_t), &han_len, &rlen);
	if (error == -1) {
		/*
		 * On any error, even E2BIG, we bail since the size of
		 * the file handle should be a constant
		 */
		errno_msg("Can't get size of staging file handle");
		return(1);
	}
	if (rlen != sizeof(size_t)) {
		err_msg("File handle length component incorrect");
		return(1);
	}

	/*
	 * Malloc space for our staging file handle, and 
	 * extract it from our DM attributes
	 */
	han_buf = (void *)malloc(han_len);
	if (han_buf == NULL) {
		err_msg("Can't alloc memory for file handle");
		return(1);
	}

	memcpy((void *)&hanp_attrname.an_chars[0], DLOC_HAN, DM_ATTR_NAME_SIZE);
	error = dm_get_dmattr(sid, hanp, hlen, token, &hanp_attrname, 
				han_len, han_buf, &rlen);
	if (error == -1) {
		errno_msg("Can't get staging file handle");
		free(han_buf);
		return(1);
	}
	if (rlen != han_len) {
		err_msg("File handle is incorrect length");
		free(han_buf);
		return(1);
	}
	*stg_hanp = han_buf;
	*stg_hlen = han_len;
	return(0);
}

