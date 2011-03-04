/*
 * Simple util to print out DMAPI info about a file
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

#define MAX_RGNS	8	/* Arbitrary for this release */
#define NUM_EXTENTS	4

extern char	*optarg;
extern int	 optind, optopt, opterr;
char		*Progname;

void		 usage(char *);
int		 mr_info(dm_sessid_t, void *, size_t);
int		 alloc_info(dm_sessid_t, void *, size_t);
int		 event_info(dm_sessid_t, void *, size_t);
int		 handle_info(dm_sessid_t, void *, size_t);

extern int	 setup_dmapi(dm_sessid_t *);
extern void	 errno_msg(char *, ...);
extern void	 print_handle(void *, size_t);


void
usage(
	char *prog)
{
	fprintf(stderr, "Usage: %s <options> filename \n ", prog);
	fprintf(stderr, "\t-m	managed region info\n");
	fprintf(stderr, "\t-a	allocation info\n");
	fprintf(stderr, "\t-e	event info\n");
	fprintf(stderr, "\t-h	handle\n");
}


int
main(
	int	argc, 
	char	*argv[])
{
	int		 c;
	int	 	 error;
	int		 mr_flag, alloc_flag, handle_flag, event_flag;
	void		*hanp;
	size_t	 	 hlen;
	char		*filename;
	dm_sessid_t	 sid;


	Progname = argv[0];
	mr_flag  = alloc_flag =  handle_flag = event_flag = 0;
	
	while ((c = getopt(argc, argv, "maeh")) != EOF) {
		switch (c) {
		case 'm':
			mr_flag = 1;
			break;
		case 'a':
			alloc_flag = 1;
			break;
		case 'e':
			event_flag = 1;
			break;
		case 'h':
			handle_flag = 1;
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
	filename = argv[optind];
	if (filename == NULL) {
		usage(Progname);
		exit(1);
	}
	

	/*
	 * Set up our link to the DMAPI, and get a handle for
	 * the file we want to query
	 */
	error = setup_dmapi(&sid);
	if (error)
		exit(1);

	if (dm_path_to_handle(filename, &hanp, &hlen) == -1) {
		printf("Can't get handle for path %s", filename);
		error = 1;
		goto out;
	}

	printf("File %s:\n", filename);
	if (mr_flag) {
		error = mr_info(sid, hanp, hlen);
		if (error) {
			error = 1;
			goto out;
		}
	}
	if (alloc_flag) {
		error = alloc_info(sid, hanp, hlen);
		if (error) {
			error = 1;
			goto out;
		}
	}
	if (event_flag) {
		error = event_info(sid, hanp, hlen);
		if (error) {
			error = 1;
			goto out;
		}
	}
	if (handle_flag) {
		error = handle_info(sid, hanp, hlen);
		if (error) {
			error = 1;
			goto out;
		}
	}

out:
	if (dm_destroy_session(sid)) {
		errno_msg("Can't shut down session");
		error = 1;
	}

	return(error);
}

/*
 * Get the complete list of all managed regions for a file. For now,
 * we know that most implementations only support a small number of
 * regions, so we don't handle the E2BIG error here.
 */
int
mr_info(
	dm_sessid_t	 sid,
	void		*hanp,
	size_t		 hlen)
{
	u_int		i;
	u_int		ret;
	dm_region_t	rgn[MAX_RGNS];

	memset((char *)&rgn, 0, (sizeof(dm_region_t) * MAX_RGNS));
	if (dm_get_region(sid, hanp, hlen, DM_NO_TOKEN, MAX_RGNS, rgn, &ret)) {
		errno_msg("Can't get list of managed regions");
		return(1);
	}
	printf("\n");
	for (i=0; i<ret; i++) {
		printf("\tRegion %d:\n", i);
		printf("\t\toffset %lld, ", (long long) rgn[i].rg_offset);
		printf("size %llu, ", (unsigned long long) rgn[i].rg_size);
		printf("flags 0x%x", rgn[i].rg_flags);
		printf(" ( ");
		if (rgn[i].rg_flags & DM_REGION_NOEVENT)
			printf("noevent ");
		if (rgn[i].rg_flags & DM_REGION_READ)
			printf("read ");
		if (rgn[i].rg_flags & DM_REGION_WRITE)
			printf("write ");
		if (rgn[i].rg_flags & DM_REGION_TRUNCATE)
			printf("trunc ");
		printf(" )\n");
	}
	return(0);
}

/*
 * Get the complete list of events for a file.
 */
int
event_info(
	dm_sessid_t	 sid,
	void		*hanp,
	size_t		 hlen)
{
	u_int		ret;
	dm_eventset_t	eventlist;

	DMEV_ZERO(eventlist);
	if (dm_get_eventlist(sid, hanp, hlen, DM_NO_TOKEN, DM_EVENT_MAX, 
				&eventlist, &ret)) {
		errno_msg("Can't get list of events");
		return(1);
	}
	printf("\n\tEvent list: \n\t\t");
	if (DMEV_ISSET(DM_EVENT_MOUNT, eventlist)) 
		printf("mount ");
	if (DMEV_ISSET(DM_EVENT_PREUNMOUNT, eventlist)) 
		printf("preunmount ");
	if (DMEV_ISSET(DM_EVENT_UNMOUNT, eventlist)) 
		printf("unmount ");
	if (DMEV_ISSET(DM_EVENT_DEBUT, eventlist)) 
		printf("debut ");
	if (DMEV_ISSET(DM_EVENT_CREATE, eventlist)) 
		printf("create ");
	if (DMEV_ISSET(DM_EVENT_POSTCREATE, eventlist)) 
		printf("postcreate ");
	if (DMEV_ISSET(DM_EVENT_REMOVE, eventlist)) 
		printf("remove ");
	if (DMEV_ISSET(DM_EVENT_POSTREMOVE, eventlist)) 
		printf("postmount ");
	if (DMEV_ISSET(DM_EVENT_RENAME, eventlist)) 
		printf("rename ");
	if (DMEV_ISSET(DM_EVENT_POSTRENAME, eventlist)) 
		printf("postrename ");
	if (DMEV_ISSET(DM_EVENT_LINK, eventlist)) 
		printf("link ");
	if (DMEV_ISSET(DM_EVENT_POSTLINK, eventlist)) 
		printf("postlink ");
	if (DMEV_ISSET(DM_EVENT_SYMLINK, eventlist)) 
		printf("symlink ");
	if (DMEV_ISSET(DM_EVENT_POSTSYMLINK, eventlist)) 
		printf("postsymlink ");
	if (DMEV_ISSET(DM_EVENT_READ, eventlist)) 
		printf("read ");
	if (DMEV_ISSET(DM_EVENT_WRITE, eventlist)) 
		printf("write ");
	if (DMEV_ISSET(DM_EVENT_TRUNCATE, eventlist)) 
		printf("truncate ");
	if (DMEV_ISSET(DM_EVENT_ATTRIBUTE, eventlist)) 
		printf("attribute ");
	if (DMEV_ISSET(DM_EVENT_DESTROY, eventlist)) 
		printf("destroy ");
	if (DMEV_ISSET(DM_EVENT_NOSPACE, eventlist)) 
		printf("nospace ");
	if (DMEV_ISSET(DM_EVENT_USER, eventlist)) 
		printf("user ");

	printf("\n");
	return(0);
}

/*
 * Print out the handle for a file
 */
int
handle_info(
	dm_sessid_t	 sid,
	void		*hanp,
	size_t		 hlen)
{
	printf("\n\tHandle (handle length, value) \n\t\t");
	print_handle(hanp, hlen);
	return(0);
}

/*
 * Get the allocation info for a file. We pick some small number
 * of extents to get the residency info on at one time
 */
int
alloc_info(
	dm_sessid_t	 sid,
	void		*hanp,
	size_t		 hlen)
{
	int		i, more;
	dm_off_t	offset;
	u_int		nextents, nret;
	dm_extent_t	ext[NUM_EXTENTS];


	nextents = NUM_EXTENTS;
	offset   = 0;
	printf("\n\tAllocation info \n");
	do {
		more = dm_get_allocinfo(sid, hanp, hlen, DM_NO_TOKEN, &offset, 
					nextents, ext, &nret);
		if (more == -1) {
			errno_msg("Can't get alloc info for file");
			return(1);
		}
		for (i=0; i<nret; i++) {
			printf("\t\tExtent %d ", i);
			if (ext[i].ex_type == DM_EXTENT_RES)
				printf("(resident): ");
			if (ext[i].ex_type == DM_EXTENT_HOLE)
				printf("(hole): ");
			printf("offset %lld, ", (long long) ext[i].ex_offset);
			printf("len %llu\n", (unsigned long long) ext[i].ex_length);
		}
	} while (more == 1);
	return(0);
}
