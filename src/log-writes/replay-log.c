// SPDX-License-Identifier: GPL-2.0
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include "log-writes.h"

enum option_indexes {
	NEXT_FLUSH,
	NEXT_FUA,
	START_ENTRY,
	END_MARK,
	LOG,
	REPLAY,
	LIMIT,
	VERBOSE,
	FIND,
	NUM_ENTRIES,
	NO_DISCARD,
	FSCK,
	CHECK,
	START_MARK,
	START_SECTOR,
	END_SECTOR,
};

static struct option long_options[] = {
	{"next-flush", no_argument, NULL, 0},
	{"next-fua", no_argument, NULL, 0},
	{"start-entry", required_argument, NULL, 0},
	{"end-mark", required_argument, NULL, 0},
	{"log", required_argument, NULL, 0},
	{"replay", required_argument, NULL, 0},
	{"limit", required_argument, NULL, 0},
	{"verbose", no_argument, NULL, 'v'},
	{"find", no_argument, NULL, 0},
	{"num-entries", no_argument, NULL, 0},
	{"no-discard", no_argument, NULL, 0},
	{"fsck", required_argument, NULL, 0},
	{"check", required_argument, NULL, 0},
	{"start-mark", required_argument, NULL, 0},
	{"start-sector", required_argument, NULL, 0},
	{"end-sector", required_argument, NULL, 0},
	{ NULL, 0, NULL, 0 },
};

static void usage(void)
{
	fprintf(stderr, "Usage: replay-log --log <logfile> [options]\n");
	fprintf(stderr, "\t--replay <device> - replay onto a specific "
		"device\n");
	fprintf(stderr, "\t--limit <number> - number of entries to replay\n");
	fprintf(stderr, "\t--next-flush - replay to/find the next flush\n");
	fprintf(stderr, "\t--next-fua - replay to/find the next fua\n");
	fprintf(stderr, "\t--start-entry <entry> - start at the given "
		"entry #\n");
	fprintf(stderr, "\t--start-mark <mark> - mark to start from\n");
	fprintf(stderr, "\t--end-mark <mark> - replay to/find the given mark\n");
	fprintf(stderr, "\t--find - put replay-log in find mode, will search "
		"based on the other options\n");
	fprintf(stderr, "\t--number-entries - print the number of entries in "
		"the log\n");
	fprintf(stderr, "\t--no-discard - don't process discard entries\n");
	fprintf(stderr, "\t--fsck - the fsck command to run, must specify "
		"--check\n");
	fprintf(stderr, "\t--check [<number>|flush|fua] when to check the "
		"file system, mush specify --fsck\n");
	fprintf(stderr, "\t--start-sector <sector> - replay ops on region "
		"from <sector> onto <device>\n");
	fprintf(stderr, "\t--end-sector <sector> - replay ops on region "
		"to <sector> onto <device>\n");
	fprintf(stderr, "\t-v or --verbose - print replayed ops\n");
	fprintf(stderr, "\t-vv - print also skipped ops\n");
	exit(1);
}

/*
 * Check if the log entry flag matches one of the stop_flags.
 * If stop_flag has LOG_MARK, then looking also for match of
 * the mark label.
 */
static int should_stop(struct log_write_entry *entry, u64 stop_flags,
		       char *mark)
{
	u64 flags = le64_to_cpu(entry->flags);
	int check_mark = (stop_flags & LOG_MARK_FLAG);
	/* mark data begins after entry header */
	char *buf = entry->data;
	/* entry buffer is padded with at least 1 zero after data_len */
	u64 buflen = le64_to_cpu(entry->data_len) + 1;

	if (flags & stop_flags) {
		if (!check_mark)
			return 1;
		if ((flags & LOG_MARK_FLAG) &&
		    !strncmp(mark, buf, buflen))
			return 1;
	}
	return 0;
}

static int run_fsck(struct log *log, char *fsck_command)
{
	int ret = fsync(log->replayfd);
	if (ret)
		return ret;
	ret = system(fsck_command);
	if (ret >= 0)
		ret = WEXITSTATUS(ret);
	return ret ? -1 : 0;
}

enum log_replay_check_mode {
	CHECK_NUMBER = 1,
	CHECK_FUA = 2,
	CHECK_FLUSH = 3,
};

static int seek_to_mark(struct log *log, struct log_write_entry *entry,
			char *mark)
{
	int ret;

	while ((ret = log_seek_next_entry(log, entry, 1)) == 0) {
		if (should_stop(entry, LOG_MARK_FLAG, mark))
			break;
	}
	if (ret == 1) {
		fprintf(stderr, "Couldn't find starting mark\n");
		ret = -1;
	}

	return ret;
}

int main(int argc, char **argv)
{
	char *logfile = NULL, *replayfile = NULL, *fsck_command = NULL;
	struct log_write_entry *entry;
	u64 stop_flags = 0;
	u64 start_entry = 0;
	u64 start_sector = 0;
	u64 end_sector = -1ULL;
	u64 run_limit = 0;
	u64 num_entries = 0;
	u64 check_number = 0;
	char *end_mark = NULL, *start_mark = NULL;
	char *tmp = NULL;
	struct log *log;
	int find_mode = 0;
	int c;
	int opt_index;
	int ret;
	int print_num_entries = 0;
	int discard = 1;
	enum log_replay_check_mode check_mode = 0;

	while ((c = getopt_long(argc, argv, "v", long_options,
				&opt_index)) >= 0) {
		switch(c) {
		case 'v':
			log_writes_verbose++;
			continue;
		case '?':
			usage();
		default:
			break;
		}

		switch(opt_index) {
		case NEXT_FLUSH:
			stop_flags |= LOG_FLUSH_FLAG;
			break;
		case NEXT_FUA:
			stop_flags |= LOG_FUA_FLAG;
			break;
		case START_ENTRY:
			start_entry = strtoull(optarg, &tmp, 0);
			if (tmp && *tmp != '\0') {
				fprintf(stderr, "Invalid entry number\n");
				exit(1);
			}
			tmp = NULL;
			break;
		case START_MARK:
			/*
			 * Biggest sectorsize is 4k atm, so limit the mark to 4k
			 * minus the size of the entry.  Say 4097 since we want
			 * an extra slot for \0.
			 */
			start_mark = strndup(optarg, 4097 -
					     sizeof(struct log_write_entry));
			if (!start_mark) {
				fprintf(stderr, "Couldn't allocate memory\n");
				exit(1);
			}
			break;
		case END_MARK:
			/*
			 * Biggest sectorsize is 4k atm, so limit the mark to 4k
			 * minus the size of the entry.  Say 4097 since we want
			 * an extra slot for \0.
			 */
			end_mark = strndup(optarg, 4097 -
					   sizeof(struct log_write_entry));
			if (!end_mark) {
				fprintf(stderr, "Couldn't allocate memory\n");
				exit(1);
			}
			stop_flags |= LOG_MARK_FLAG;
			break;
		case LOG:
			logfile = strdup(optarg);
			if (!logfile) {
				fprintf(stderr, "Couldn't allocate memory\n");
				exit(1);
			}
			break;
		case REPLAY:
			replayfile = strdup(optarg);
			if (!replayfile) {
				fprintf(stderr, "Couldn't allocate memory\n");
				exit(1);
			}
			break;
		case LIMIT:
			run_limit = strtoull(optarg, &tmp, 0);
			if (tmp && *tmp != '\0') {
				fprintf(stderr, "Invalid entry number\n");
				exit(1);
			}
			tmp = NULL;
			break;
		case FIND:
			find_mode = 1;
			break;
		case NUM_ENTRIES:
			print_num_entries = 1;
			break;
		case NO_DISCARD:
			discard = 0;
			break;
		case FSCK:
			fsck_command = strdup(optarg);
			if (!fsck_command) {
				fprintf(stderr, "Couldn't allocate memory\n");
				exit(1);
			}
			break;
		case CHECK:
			if (!strcmp(optarg, "flush")) {
				check_mode = CHECK_FLUSH;
			} else if (!strcmp(optarg, "fua")) {
				check_mode = CHECK_FUA;
			} else {
				check_mode = CHECK_NUMBER;
				check_number = strtoull(optarg, &tmp, 0);
				if (!check_number || (tmp && *tmp != '\0')) {
					fprintf(stderr,
						"Invalid entry number\n");
					exit(1);
				}
				tmp = NULL;
			}
			break;
		case START_SECTOR:
			start_sector = strtoull(optarg, &tmp, 0);
			if (tmp && *tmp != '\0') {
				fprintf(stderr, "Invalid sector number\n");
				exit(1);
			}
			tmp = NULL;
			break;
		case END_SECTOR:
			end_sector = strtoull(optarg, &tmp, 0);
			if (tmp && *tmp != '\0') {
				fprintf(stderr, "Invalid sector number\n");
				exit(1);
			}
			tmp = NULL;
			break;
		default:
			usage();
		}
	}

	if (!logfile)
		usage();

	log = log_open(logfile, replayfile);
	if (!log)
		exit(1);
	free(logfile);
	free(replayfile);

	if (!discard)
		log->flags |= LOG_IGNORE_DISCARD;

	log->start_sector = start_sector;
	log->end_sector = end_sector;

	entry = malloc(log->sectorsize);
	if (!entry) {
		fprintf(stderr, "Couldn't allocate buffer\n");
		log_free(log);
		exit(1);
	}

	if (start_mark) {
		ret = seek_to_mark(log, entry, start_mark);
		if (ret)
			exit(1);
		free(start_mark);
	} else {
		ret = log_seek_entry(log, start_entry);
		if (ret)
			exit(1);
	}

	if ((fsck_command && !check_mode) || (!fsck_command && check_mode))
		usage();

	/* We just want to find a given entry */
	if (find_mode) {
		while ((ret = log_seek_next_entry(log, entry, 1)) == 0) {
			num_entries++;
			if ((run_limit && num_entries == run_limit) ||
			    should_stop(entry, stop_flags, end_mark)) {
				printf("%llu@%llu\n",
				       (unsigned long long)log->cur_entry - 1,
				       log->cur_pos / log->sectorsize);
				log_free(log);
				return 0;
			}
		}
		log_free(log);
		if (ret < 0)
			return ret;
		fprintf(stderr, "Couldn't find entry\n");
		return 1;
	}

	/* Used for scripts, just print the number of entries in the log */
	if (print_num_entries) {
		printf("%llu\n", (unsigned long long)log->nr_entries);
		log_free(log);
		return 0;
	}

	/* No replay, just spit out the log info. */
	if (!replayfile) {
		printf("Log version=%d, sectorsize=%lu, entries=%llu\n",
		       WRITE_LOG_VERSION, (unsigned long)log->sectorsize,
		       (unsigned long long)log->nr_entries);
		log_free(log);
		return 0;
	}

	while ((ret = log_replay_next_entry(log, entry, 1)) == 0) {
		num_entries++;
		if (fsck_command) {
			if ((check_mode == CHECK_NUMBER) &&
			    !(num_entries % check_number))
				ret = run_fsck(log, fsck_command);
			else if ((check_mode == CHECK_FUA) &&
				 should_stop(entry, LOG_FUA_FLAG, NULL))
				ret = run_fsck(log, fsck_command);
			else if ((check_mode == CHECK_FLUSH) &&
				 should_stop(entry, LOG_FLUSH_FLAG, NULL))
				ret = run_fsck(log, fsck_command);
			else
				ret = 0;
			if (ret) {
				fprintf(stderr, "Fsck errored out on entry "
					"%llu\n",
					(unsigned long long)log->cur_entry - 1);
				break;
			}
		}

		if ((run_limit && num_entries == run_limit) ||
		    should_stop(entry, stop_flags, end_mark))
			break;
	}
	fsync(log->replayfd);
	log_free(log);
	free(end_mark);
	if (ret < 0)
		exit(1);
	return 0;
}
