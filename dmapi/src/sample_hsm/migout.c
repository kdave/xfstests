/*
 * Simple utility to migrate a group of specified files.
 * The unsorted input is from migfind, and is of the form:
 *	filehandle length	filehandle 	file size
 *
 * The data for each file will be stored in another file located
 * in a different directory. This 'staging directory' should be on
 * another filesystem. The staging file will be named the same as
 * the file handle. This simple-minded scheme suffices, since we're
 * not interested in showing any media management in this example.
 *
 * ASSUMPTIONS:
 *	Persistent managed regions are supported
 *	Persistent DM attributes are supported
 *
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
#include <sys/param.h>
#include <sys/stat.h>

#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>

#include <lib/hsm.h>

extern char	*optarg;
extern int	 optind, optopt, opterr;
char		*Progname;
int		 Verbose;

int	mig_files(dm_sessid_t, char *);
int	mk_nonrez(dm_sessid_t, void *, size_t, dm_token_t, dm_off_t);
int	set_mrgns(dm_sessid_t, void *, size_t, dm_token_t, dm_off_t,
		  dm_off_t *);
void	clear_mrgns(dm_sessid_t, void *, dm_size_t, dm_token_t);
int	lock_file(dm_sessid_t, void *, size_t, dm_right_t, dm_token_t *);
void	unlock_file(dm_sessid_t, dm_token_t);
int	get_dmchange(dm_sessid_t, void *, size_t, dm_token_t, u_int *);
int	create_stgfile(char *, void *, size_t, char *, int *);
int	setup_dmapi(dm_sessid_t *);
int	save_filedata(dm_sessid_t, void *, size_t, int, dm_size_t);
int	extract_fields(char *, char *, size_t *, dm_size_t *);
int	save_dataloc(dm_sessid_t, void *, size_t, dm_token_t, char *);

void	usage(char *);

void
usage(
	char *prog)
{
	fprintf(stderr, "Usage: %s ", prog);
	fprintf(stderr, " <-v verbose> ");
	fprintf(stderr, "<staging directory>\n");
}


int
main(
	int	argc,
	char	*argv[])
{

	int	 	 c;
	int	 	 error;
	char		*stage_dir;
	dm_sessid_t	 sid;


	error     = 0;
	Progname  = argv[0];
	stage_dir = NULL;

	while ((c = getopt(argc, argv, "v")) != EOF) {
		switch (c) {
		case 'v':
			Verbose++;
			break;

		case '?':
		default:
			usage(Progname);
			exit(1);
		}
	}
	if (optind >= argc) {
		usage(Progname);
		exit(1);
	}
	stage_dir = argv[optind];
	if (stage_dir == NULL) {
		usage(Progname);
		exit(1);
	}

	/*
	 * Init the dmapi, and get a session.
	 */
	error = setup_dmapi(&sid);
	if (error)
		exit(1);

	/*
 	 * Migrate all the files given to us via stdin
   	 */
	error = mig_files(sid, stage_dir);


	if (dm_destroy_session(sid))
		errno_msg("Can't shut down session, line=%d, errno=%d", __LINE__, errno);

	return(error);
}

/*
 * Migrate all the files given in stdin
 */

int
mig_files(
	dm_sessid_t	 sid,
	char		*stage_dir)
{
	void		*hanp;
	size_t	 	 hlen;
	dm_size_t	 fsize;
	int	 	 error;
	u_int		 change_start, change_end;
	int	 	 stg_fd;		/* staging file descriptor */
	dm_off_t	 off;			/* starting offset */
	dm_token_t	 token;			/* file token */
	char		 ibuf[IBUFSIZE];
	char		 handle_buf[HANDLE_LEN];
	char		 stgpath[MAXPATHLEN];

	/*
	 * Read all the lines in std input and migrate each file.
	 * This simple-minded migout does no batching, sorting, or
	 * anything else that a real HSM might do.
	 */
	while (fgets(ibuf, IBUFSIZE, stdin) != NULL) {
		error = extract_fields(ibuf, handle_buf, &hlen, &fsize);
		if (error) {
			err_msg("%s/%d: mangled input line, '%s' ", __FILE__, __LINE__, ibuf);
			continue;
		}
		hanp = (void *)handle_buf;
		if (Verbose) {
			print_handle(hanp, hlen);
		}

		/*
		 * Create and open the file in the staging directory.
		 */
		error = create_stgfile(stage_dir, hanp, hlen, stgpath, &stg_fd);
		if (error)
			continue;

		/*
		 * Get the file's DMAPI change indicator so that we
		 * can tell if it changed (either via a data mod, or
		 * a DM attribute update) while we are staging it out
		 */
		error = get_dmchange(sid, hanp, hlen, DM_NO_TOKEN,
					&change_start);
		if (error) {
			close(stg_fd);
			continue;
		}

		/*
		 * Write all the file's data to our file in the
		 * staging directory. In a real HSM, the data would
		 * be copied off to tertiary storage somewhere.
		 *
		 * The staging file descriptor will be closed for us
		 * in all cases.
		 */
		error = save_filedata(sid, hanp, hlen, stg_fd, fsize);
		if (error)
			continue;


		/*
		 * Get exclusive access to the file so we can blow
		 * away its data
		 */
		error = lock_file(sid, hanp, hlen, DM_RIGHT_EXCL, &token);
		if (error) {
			err_msg("Can't get exclusive access to file, ignoring");
			continue;
		}

		/*
		 * Make sure the file did not change
		 */
		error = get_dmchange(sid, hanp, hlen, token, &change_end);
		if (error) {
			unlock_file(sid, token);
			continue;
		}
		if (change_start != change_end) {
			unlock_file(sid, token);
			err_msg("File changed during stageout, ignoring");
			continue;
		}

		/*
		 * Save the location of the data (the staging file)
		 * in a private DM attribute so that we can get the
		 * file back in the future
		 */
		error = save_dataloc(sid, hanp, hlen, token, stgpath);
		if (error) {
			err_msg("Can't save location of file data");
			unlock_file(sid, token);
			continue;
		}


		/*
		 * Set up the managed regions on the file so that
		 * a foray past the fencepost will cause an event to
		 * be generated.
		 */
		error = set_mrgns(sid, hanp, hlen, token, fsize, &off);
		if (error) {
			unlock_file(sid, token);
			err_msg("Can't set managed regions");
			continue;
		}

		/*
		 * Now we can safely blow away the data.
		 */
		error = mk_nonrez(sid, hanp, hlen, token, off);
		if (error)  {
			clear_mrgns(sid, hanp, hlen, token);
		}

		/*
		 * Unlock the file, which releases the token
		 */
		unlock_file(sid, token);

	}

	return(0);
}


/*
 * Remove the data for a file
 */
int
mk_nonrez(
	dm_sessid_t	 sid,
	void		*hanp,
	size_t		 hlen,
	dm_token_t	 token,
	dm_off_t	 off)
{
	int	error;

	error = dm_punch_hole(sid, hanp, hlen, token, off, 0);
	if (error == -1) {
		errno_msg("Can't punch hole in file, line=%d, errno=%d", __LINE__, errno);
		return(1);
	}
	return(0);
}


/*
 * Set up the managed regions on a file. We try to leave some of the
 * file resident; the actual amount left on-disk is dependent
 * on the rounding enforced by the DMAPI implementation.
 */
int
set_mrgns(
	dm_sessid_t	 sid,
	void		*hanp,
	size_t		 hlen,
	dm_token_t	 token,
	dm_off_t	 fsize,
	dm_off_t	*start_off)
{
	dm_off_t	rroff;
	dm_size_t	rlenp;
	dm_region_t	rgn;
	u_int		exact_flag;
	int		error;

	if (fsize > FENCEPOST_SIZE) {
		error = dm_probe_hole(sid, hanp, hlen, token, FENCEPOST_SIZE, 0,
					&rroff, &rlenp);
		if (error == -1) {
			errno_msg("Can't probe hole in file, line=%d, errno=%d", __LINE__, errno);
			return(1);
		}
	} else {
		rroff = 0;
	}
	*start_off = rroff;

	/*
	 * Now we know what the DMAPI and filesystem will support with
	 * respect to rounding of holes. We try to set our managed region
	 * to begin at this offset and continue to the end of the file.
	 * We set the event mask so that we'll trap on all events that
	 * occur in the managed region.
	 *
	 * Note that some implementations may not be able to support
	 * a managed region that starts someplace other than the beginning
	 * of the file. If we really cared, we could check the exact_flag.
	 */
	rgn.rg_offset = rroff;
	rgn.rg_size   = 0;
	rgn.rg_flags  = DM_REGION_READ | DM_REGION_WRITE | DM_REGION_TRUNCATE;
	error = dm_set_region(sid, hanp, hlen, token, 1, &rgn, &exact_flag);
	if (error == -1) {
		errno_msg("Can't set managed region, line=%d, errno=%d", __LINE__, errno);
		return(1);
	}
	return(0);
}


/*
 * Clear a managed region on a file
 */
void
clear_mrgns(
	dm_sessid_t	sid,
	void		*hanp,
	dm_size_t	hlen,
	dm_token_t	token)
{
	dm_region_t	rgn;
	u_int		exact_flag;
	int		error;

	rgn.rg_offset = 0;
	rgn.rg_size = 0;
	rgn.rg_flags = DM_REGION_NOEVENT;
	error = dm_set_region(sid, hanp, hlen, token, 1, &rgn, &exact_flag);
	if (error)
		errno_msg("Can't clear managed regions from file, line=%d, errno=%d", __LINE__, errno);

	return;
}


/*
 * File rights are accessed via a token. The token must be associated
 * with a synchronous event message. So, to obtain either shared or
 * exclusive rights to a file, we first associate a token with a message,
 * and then request our desired right
 */
int
lock_file(
	dm_sessid_t	 sid,
	void		*hanp,
	size_t		 hlen,
	dm_right_t	 right,
	dm_token_t	*token)
{
	int	error;

	error = dm_create_userevent(sid,  (size_t)0, (void *)0, token);
	if (error == -1) {
		errno_msg("Can't create user event for token context, line=%d, errno=%d", __LINE__, errno);
		return(1);
	}
	error = dm_request_right(sid, hanp, hlen, *token, DM_RR_WAIT, right);
	if (error == -1) {
		errno_msg("Can't get requested right for token, line=%d, errno=%d", __LINE__, errno);
		return(1);
	}
	return(0);
}


/*
 * Release the lock on a file
 */
void
unlock_file(
	dm_sessid_t	 sid,
	dm_token_t	 token)
{
	int	error;

	error = dm_respond_event(sid, token, DM_RESP_CONTINUE, 0, 0, 0);
	if (error == -1)
		errno_msg("Can't respond to event and release token, line=%d, errno=%d", __LINE__, errno);

	return;
}


int
create_stgfile(
	char	*stage_dir,
	void	*hanp,
	size_t	 hlen,
	char	*path,
	int	*stg_fd)
{
	char	handle_str[HANDLE_STR];

	if (hlen > HANDLE_LEN) {
		err_msg("Handle length (%d) too long for file", hlen);
		return(1);
	}

	strcpy(path, stage_dir);
	strcat(path, "/");
	hantoa(hanp, hlen, handle_str);

	/*
	 * Concat the ascii representation of the file handle
	 * (which is two times longer than the binary version)
	 * onto the staging path name
	 */
	strncat(path, (char *)handle_str, hlen*2);

	if ( (*stg_fd = open(path, O_RDWR | O_CREAT, 0644)) < 0) {
		errno_msg("Can't open file %s, line=%d, errno=%d\n", path, __LINE__, errno);
		return(1);
	}

	return(0);
}


/*
 * Extract the three fields from the input line. THe input is of
 * the form
 *	filehandle length	filehandle 	file size
 *
 * The length of the file handle is expected to be less than 64 bytes.
 */
int
extract_fields(
	char		*ibuf,
	char		*handle_buf,
	size_t		*hlen,
	dm_size_t	*fsize)
{
	char	*cp, *start;
	size_t	 len;
	char *hanp;
	char *hanpp=NULL;

	/*
	 * Skip any leading white space, and check the length
	 * of the file handle
	 */
	cp = ibuf;
	while (!isalnum(*cp))
		cp++;

	start = cp;
	while (isalnum(*cp))
		cp++;
	*cp = '\0';

	len = strtol(start, 0, 0);
	if (len > HANDLE_LEN) {
		err_msg("%s/%d: Handle length %d too long in input line", __FILE__, __LINE__, len);
		return(1);
	}

	*hlen = len;

	/*
	 * Skip over white space, and extract the file handle
	 */
	while (!isalnum(*cp))
		cp++;
	hanp = cp;

	/*
 	 * Skip over the ascii length of the file handle, and
	 * then extract the file's length
	 */
	cp += len*2;
	*cp = '\0';

	atohan( hanp, (void**)&hanpp, &len );
	memcpy( handle_buf, hanpp, len);
	free( hanpp );

	/* skip over white space */
	while (!isalnum(*cp))
		cp++;

	/* read file length */
	start = cp;
	while (isalnum(*cp))
		cp++;
	*cp = '\0';

	*fsize = strtol(start, 0, 0);

	return(0);

}


/*
 * Save the location of the file's data so that we can find
 * it again when we're staging the file back in. Rather than store
 * the full pathname of the staging file, we just store the handle.
 * This means the staging dir must be on a filesystem that supports
 * the DMAPI.
 */
int
save_dataloc(
	dm_sessid_t	 sid,
	void		*hanp,
	size_t		 hlen,
	dm_token_t	 token,
	char		*stgpath)
{
	void		*stg_hanp;
	size_t		 stg_hlen;
	int		 error;
	dm_attrname_t	 hanp_attrname;
	dm_attrname_t	 hlen_attrname;

	if (dm_path_to_handle(stgpath, &stg_hanp, &stg_hlen) == -1) {
		errno_msg("Can't get handle for path %s, line=%d, errno=%d", stgpath, __LINE__, errno);
		return(1);
	}

	/*
	 * Since handles are in two parts, they become two attributes.
	 * This can be useful, since we can extract the length
	 * separately when we stage the file back in
	 */
	memcpy((void *)&hanp_attrname.an_chars[0], DLOC_HAN, DM_ATTR_NAME_SIZE);
	error = dm_set_dmattr(sid, hanp, hlen, token, &hanp_attrname,
				0, stg_hlen, stg_hanp);
	if (error == -1) {
		errno_msg("Can't set DM attr of staging filehandle, line=%d, errno=%d",__LINE__, errno);
		return(1);
	}

	memcpy((void *)&hlen_attrname.an_chars[0], DLOC_HANLEN, DM_ATTR_NAME_SIZE);
	error = dm_set_dmattr(sid, hanp, hlen, token, &hlen_attrname,
				0, sizeof(stg_hlen), (void *)&stg_hlen);
	if (error == -1) {
		errno_msg("Can't set DM attr of staging filehandle length, line=%d, errno=%d", __LINE__, errno);
		return(1);
	}
	return(0);
}
