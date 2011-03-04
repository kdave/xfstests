/*
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
 *
 * October 2, 2003: Dean Roehrich
 *  - Adapted to test dm_get_bulkattr + dm_get_dmattr, from migfind.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include <lib/hsm.h>

#include <getopt.h>

extern char	*optarg;
extern int	 optind, optopt, opterr;
char		*Progname;

extern void	print_victim(void *, size_t, dm_off_t);
extern void	err_msg(char *, ...);
extern void	errno_msg(char *, ...);

int 		setup_dmapi(dm_sessid_t *);
int		scan_fs(dm_sessid_t, void *, size_t, dm_attrname_t*,
			size_t, int);
void		usage(char *);

void
usage(
	char *prog)
{
	fprintf(stderr, "Usage: %s ", prog);
	fprintf(stderr, " [-a attrname] [-b bufsz] [-v]");
	fprintf(stderr, " [-q] filesystem\n");
}

#define V_PRINT		0x01
#define V_VERBOSE	0x02


int
main(
	int	argc, 
	char	*argv[])
{
	
	int	 	 c;
	int	 	 error;
	char		*fsname;
	dm_sessid_t	 sid;
	void		*fs_hanp;
	size_t	 	 fs_hlen;
	dm_attrname_t	dmattr;
	size_t		bufsz = 65536;
	dm_attrname_t	*dmattrp = NULL;
	int		verbose = V_PRINT;

	Progname = argv[0];
	memset(&dmattr, 0, sizeof(dmattr));

	while ((c = getopt(argc, argv, "a:b:vq")) != EOF) {
		switch (c) {
		case 'a':
			if (strlen(optarg) > (DM_ATTR_NAME_SIZE-1)){
				printf("Arg for -a too long\n");
				exit(1);
			}
			strcpy((char*)dmattr.an_chars, optarg);
			dmattrp = &dmattr;
			break;
		case 'b':
			bufsz = atoi(optarg);
			break;
		case 'v':
			verbose |= V_VERBOSE;
			break;

		case 'q':
			verbose &= ~V_PRINT;
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

	fsname = argv[optind];

	/*
	 * Now we have our filesystem name and possibly a size threshold
	 * to look for. Init the dmapi, and get a filesystem handle so
	 * we can scan the filesystem
	 */
	error = setup_dmapi(&sid);
	if (error) 
		exit(1);
	
	if (dm_path_to_fshandle(fsname, &fs_hanp, &fs_hlen) == -1) {
		errno_msg("Can't get filesystem handle");
		exit(1);
	}



	/*
	 * Get the attributes of all files in the filesystem
	 */
	error = scan_fs(sid, fs_hanp, fs_hlen, dmattrp, bufsz, verbose);
	if (error) 
		exit(1);
	

	/*
	 * We're done, so we can shut down our session.
	 */
	if (dm_destroy_session(sid) == -1) {
		errno_msg("Can't close session");
		exit(1);
	}

	return(0);

}

void
my_print_victim(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_attrname_t	*dm_attrp,
	int		verbose)
{
	size_t	rlen;
	size_t	buflen = 10000;
	char	buf[buflen];

	if (hlen > HANDLE_LEN) {
		if (verbose & V_PRINT)
			printf("-- invalid length --\n");
	}
	else {
		char handle_str[HANDLE_STR];
		if (verbose & V_PRINT) {
			printf("%zd\t", hlen);
			hantoa(hanp, hlen, handle_str);
			printf("%s ", handle_str);
		}

		if (dm_get_dmattr(sid, hanp, hlen, DM_NO_TOKEN, dm_attrp,
				  buflen, (char*)buf, &rlen)) {
			if (verbose & V_PRINT) {
				if( errno == E2BIG ){
					printf("E2BIG\n");
				}
				else if (errno == ENOMEM){
					printf("ENOMEM\n");
				}
				else {
					printf("<none>\n");
				}
			}
		}
		else if (verbose & V_PRINT) {
			if( isalpha(buf[0]) )
				printf("(%s)\n", buf);
			else
				printf("<len=%zd>\n",rlen);
		}
	}
}

/*
 * Get the attributes for all the files in a filesystem in bulk,
 * and print out the handles and sizes of any that meet our target
 * criteria.
 *
 * We are not interested in file names; if we were, then we would 
 * have to do a dm_get_dirattrs() on each directroy, then use 
 * dm_handle_to_path() to get the pathname. 
 */
int
scan_fs(
	dm_sessid_t 	 sid, 
	void		*fs_hanp, 
	size_t 	 	 fs_hlen, 
	dm_attrname_t	*dmattrp,
	size_t		buflen,
	int		verbose)
{
	u_int		 mask;			/* attributes to scan for */
	dm_stat_t	*dm_statbuf, *sbuf;	/* attributes buffer */
	dm_attrloc_t	 locp;			/* opaque location in fs */
	size_t		 rlenp;			/* ret length of stat info */
	void		*hanp;			/* file handle */
	size_t	 	 hlen;			/* file handle */
	int		 more;			/* loop terminator */
	int		 error; 


#ifdef	VERITAS_21
	if (buflen > 65536)
		buflen = 65536;
#endif
	dm_statbuf = (dm_stat_t *)calloc(1, buflen);
	if (dm_statbuf == NULL)  {
		err_msg("Can't get memory for stat buffer");
		return(1);
	}


	/*
	 * Initialize the offset opaque offset cookie that
	 * we use in successive calls to dm_get_bulkattr()
	 */
	error = dm_init_attrloc(sid, fs_hanp, fs_hlen, DM_NO_TOKEN, &locp);
	if (error == -1) {
		errno_msg("%s/%d: Can't initialize offset cookie (%d)", __FILE__, __LINE__, errno);
		free(dm_statbuf);
		return(1);
	}

	/*
	 * Set our stat mask so that we'll only get the normal stat(2)
	 * info and the file's handle
	 */
	mask = DM_AT_HANDLE | DM_AT_STAT;
	do {
		more = dm_get_bulkattr(sid, fs_hanp, fs_hlen, DM_NO_TOKEN,
				       mask, &locp, buflen, dm_statbuf, &rlenp);
		if (verbose & V_VERBOSE)
			fprintf(stderr, "BULKATTR more=%d, rlen=%zd\n",
				more, rlenp);
		if (more == -1) {
			errno_msg("%s/%d: Can't get bulkattr for filesystem", __FILE__, __LINE__, errno);
			break;
		}

		/*
		 * Walk through the stat buffer and pull out files 
		 * that are of interest
		 *
		 * The stat buffer is variable length, so we must
		 * use the DM_STEP_TO_NEXT macro to access each individual
		 * dm_stat_t structure in the returned buffer.
		 */
		sbuf = dm_statbuf;
		while (sbuf != NULL) {
			if (S_ISREG(sbuf->dt_mode)) {
				hanp = DM_GET_VALUE(sbuf, dt_handle, void *);
				hlen = DM_GET_LEN(sbuf, dt_handle);

				if (dmattrp) {
					my_print_victim(sid, hanp, hlen,
							dmattrp, verbose);
				}
				else if (verbose & V_PRINT){
					print_victim(hanp, hlen, sbuf->dt_size);
				}
			}
			sbuf = DM_STEP_TO_NEXT(sbuf, dm_stat_t *);
		}
	} while (more == 1);

	free(dm_statbuf);
	if (more == -1) 
		return(1);

	return(0);
}

