// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "log-writes.h"

int log_writes_verbose = 0;

/*
 * @log: the log to free.
 *
 * This will close any open fd's the log has and free up its memory.
 */
void log_free(struct log *log)
{
	if (log->replayfd >= 0)
		close(log->replayfd);
	if (log->logfd >= 0)
		close(log->logfd);
	free(log);
}

static int discard_range(struct log *log, u64 start, u64 len)
{
	u64 range[2] = { start, len };

	if (ioctl(log->replayfd, BLKDISCARD, &range) < 0) {
		if (log_writes_verbose)
			printf("replay device doesn't support discard, "
			       "switching to writing zeros\n");
		log->flags |= LOG_DISCARD_NOT_SUPP;
	}
	return 0;
}

static int zero_range(struct log *log, u64 start, u64 len)
{
	u64 bufsize = len;
	ssize_t ret;
	char *buf = NULL;

	if (log->max_zero_size < len) {
		if (log_writes_verbose)
			printf("discard len %llu larger than max %llu\n",
			       (unsigned long long)len,
			       (unsigned long long)log->max_zero_size);
		return 0;
	}

	while (!buf) {
		buf = malloc(bufsize);
		if (!buf)
			bufsize >>= 1;
		if (!bufsize) {
			fprintf(stderr, "Couldn't allocate zero buffer");
			return -1;
		}
	}

	memset(buf, 0, bufsize);
	while (len) {
		if (len < bufsize)
			bufsize = len;

		ret = pwrite(log->replayfd, buf, bufsize, start);
		if (ret != bufsize) {
			fprintf(stderr, "Error zeroing file: %d\n", errno);
			free(buf);
			return -1;
		}
		len -= ret;
		start += ret;
	}
	free(buf);
	return 0;
}

/*
 * @log: the log we are replaying.
 * @entry: the discard entry.
 *
 * Discard the given length.  If the device supports discard we will call that
 * ioctl, otherwise we will write 0's to emulate discard.  If the discard size
 * is larger than log->max_zero_size then we will simply skip the zero'ing if
 * the drive doesn't support discard.
 */
int log_discard(struct log *log, struct log_write_entry *entry)
{
	u64 start = le64_to_cpu(entry->sector) * log->sectorsize;
	u64 size = le64_to_cpu(entry->nr_sectors) * log->sectorsize;
	u64 max_chunk = 1 * 1024 * 1024 * 1024;

	if (log->flags & LOG_IGNORE_DISCARD)
		return 0;

	while (size) {
		u64 len = size > max_chunk ? max_chunk : size;
		int ret;

		/*
		 * Do this check first in case it is our first discard, that way
		 * if we return EOPNOTSUPP we will fall back to the 0 method
		 * automatically.
		 */
		if (!(log->flags & LOG_DISCARD_NOT_SUPP))
			ret = discard_range(log, start, len);
		if (log->flags & LOG_DISCARD_NOT_SUPP)
			ret = zero_range(log, start, len);
		if (ret)
			return -1;
		size -= len;
		start += len;
	}
	return 0;
}

#define DEFINE_LOG_FLAGS_STR_ENTRY(x)	\
	{LOG_##x##_FLAG, #x}

struct flags_to_str_entry {
	u64 flags;
	const char *str;
} log_flags_table[] = {
	DEFINE_LOG_FLAGS_STR_ENTRY(FLUSH),
	DEFINE_LOG_FLAGS_STR_ENTRY(FUA),
	DEFINE_LOG_FLAGS_STR_ENTRY(DISCARD),
	DEFINE_LOG_FLAGS_STR_ENTRY(MARK),
	DEFINE_LOG_FLAGS_STR_ENTRY(METADATA)
};

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define LOG_FLAGS_BUF_SIZE	128
/*
 * Convert numeric flags to human readable flags.
 * @flags:	numeric flags
 * @buf:	output buffer for human readable string.
 * 		must have enough space (LOG_FLAGS_BUF_SIZE) to contain all
 * 		the string
 */
static void entry_flags_to_str(u64 flags, char *buf)
{
	int empty = 1;
	int left_len;
	int i;

	buf[0] = '\0';
	for (i = 0; i < ARRAY_SIZE(log_flags_table); i++) {
		if (flags & log_flags_table[i].flags) {
			if (!empty)
				strncat(buf, "|", LOG_FLAGS_BUF_SIZE);
			empty = 0;
			strncat(buf, log_flags_table[i].str, LOG_FLAGS_BUF_SIZE);
			flags &= ~log_flags_table[i].flags;
		}
	}
	if (flags) {
		if (!empty)
			strncat(buf, "|", LOG_FLAGS_BUF_SIZE);
		empty = 0;
		left_len = LOG_FLAGS_BUF_SIZE - strnlen(buf,
						        LOG_FLAGS_BUF_SIZE);
		if (left_len > 0)
			snprintf(buf + strnlen(buf, LOG_FLAGS_BUF_SIZE),
				 left_len, "UNKNOWN.0x%llx", flags);
	}
	if (empty)
		strncpy(buf, "NONE", LOG_FLAGS_BUF_SIZE);
}

/*
 * @log: the log we are replaying.
 * @entry: entry to be replayed.
 *
 * @return: 0 if we should replay the entry, > 0 if we should skip it.
 *
 * Should we skip the entry in our log or replay onto the replay device.
 */
int log_should_skip(struct log *log, struct log_write_entry *entry)
{
	u64 sector = le64_to_cpu(entry->sector);
	u64 nr_sectors = le64_to_cpu(entry->nr_sectors);

	if (!nr_sectors)
		return 0;
	if (sector + nr_sectors <= log->start_sector ||
	    sector > log->end_sector)
		return 1;
	return 0;
}

/*
 * @entry: entry to be replayed.
 *
 * @return: 1 if the entry is sane, 0 if it is invalid.
 *
 * Check if this is a sane log entry.
 */
int log_entry_valid(struct log_write_entry *entry)
{
	u64 flags = le64_to_cpu(entry->flags);

	/* Suspect all zeroes entry */
	if (!flags && !entry->nr_sectors)
		return 0;
	/* Suspect non zero padded entry */
	if (flags != LOG_MARK_FLAG && entry->data[0] != 0)
		return 0;
	return 1;
}

/*
 * @log: the log we are replaying.
 * @entry: where we put the entry.
 * @read_data: read the entry data as well, entry must be log->sectorsize sized
 * if this is set.
 *
 * @return: 0 if we replayed, 1 if we are at the end, -1 if there was an error.
 *
 * Replay the next entry in our log onto the replay device.
 */
int log_replay_next_entry(struct log *log, struct log_write_entry *entry,
			  int read_data)
{
	u64 size;
	u64 flags;
	size_t read_size = read_data ? log->sectorsize :
		sizeof(struct log_write_entry);
	char *buf;
	char flags_buf[LOG_FLAGS_BUF_SIZE];
	ssize_t ret;
	off_t offset;
	int skip = 0;

	if (log->cur_entry >= log->nr_entries)
		return 1;

	ret = read(log->logfd, entry, read_size);
	if (ret != read_size) {
		fprintf(stderr, "Error reading entry: %d\n", errno);
		return -1;
	}
	if (!log_entry_valid(entry)) {
		fprintf(stderr, "Malformed entry @%llu\n",
				log->cur_pos / log->sectorsize);
		return -1;
	}
	log->cur_entry++;

	size = le64_to_cpu(entry->nr_sectors) * log->sectorsize;
	if (read_size < log->sectorsize) {
		log->cur_pos = lseek(log->logfd,
			log->sectorsize - sizeof(struct log_write_entry), SEEK_CUR);
		if (log->cur_pos == (off_t)-1) {
			fprintf(stderr, "Error seeking in log: %d\n", errno);
			return -1;
		}
	} else {
		log->cur_pos += read_size;
	}

	flags = le64_to_cpu(entry->flags);
	entry_flags_to_str(flags, flags_buf);
	skip = log_should_skip(log, entry);
	if (log_writes_verbose > 1 || (log_writes_verbose && !skip)) {
		printf("%s %d@%llu: sector %llu, size %llu, flags 0x%llx(%s)\n",
		       skip ? "skipping" : "replaying",
		       (int)log->cur_entry - 1, log->cur_pos / log->sectorsize,
		       (unsigned long long)le64_to_cpu(entry->sector),
		       (unsigned long long)size,
		       (unsigned long long)flags, flags_buf);
	}
	if (!size)
		return 0;

	if (flags & LOG_DISCARD_FLAG)
		return log_discard(log, entry);

	if (skip) {
		log->cur_pos = lseek(log->logfd, size, SEEK_CUR);
		if (log->cur_pos == (off_t)-1) {
			fprintf(stderr, "Error seeking in log: %d\n", errno);
			return -1;
		}
		return 0;
	}

	buf = malloc(size);
	if (!buf) {
		fprintf(stderr, "Error allocating buffer %llu entry %llu\n", (unsigned long long)size, (unsigned long long)log->cur_entry - 1);
		return -1;
	}

	ret = read(log->logfd, buf, size);
	if (ret != size) {
		fprintf(stderr, "Error reading data: %d\n", errno);
		free(buf);
		return -1;
	}
	log->cur_pos += size;

	offset = le64_to_cpu(entry->sector) * log->sectorsize;
	ret = pwrite(log->replayfd, buf, size, offset);
	free(buf);
	if (ret != size) {
		fprintf(stderr, "Error writing data: %d\n", errno);
		return -1;
	}

	return 0;
}

/*
 * @log: the log we are manipulating.
 * @entry_num: the entry we want.
 *
 * Seek to the given entry in the log, starting at 0 and ending at
 * log->nr_entries - 1.
 */
int log_seek_entry(struct log *log, u64 entry_num)
{
	u64 i = 0;

	if (entry_num >= log->nr_entries) {
		fprintf(stderr, "Invalid entry number\n");
		return -1;
	}

	/* Skip the first sector containing the log super block */
	log->cur_pos = lseek(log->logfd, log->sectorsize, SEEK_SET);
	if (log->cur_pos == (off_t)-1) {
		fprintf(stderr, "Error seeking in file: %d\n", errno);
		return -1;
	}

	log->cur_entry = 0;
	for (i = 0; i < entry_num; i++) {
		struct log_write_entry entry;
		ssize_t ret;
		off_t seek_size;
		u64 flags;

		ret = read(log->logfd, &entry, sizeof(entry));
		if (ret != sizeof(entry)) {
			fprintf(stderr, "Error reading entry: %d\n", errno);
			return -1;
		}
		if (!log_entry_valid(&entry)) {
			fprintf(stderr, "Malformed entry @%llu\n",
					log->cur_pos / log->sectorsize);
			return -1;
		}
		if (log_writes_verbose > 1)
			printf("seek entry %d@%llu: %llu, size %llu, flags 0x%llx\n",
			       (int)i, log->cur_pos / log->sectorsize,
			       (unsigned long long)le64_to_cpu(entry.sector),
			       (unsigned long long)le64_to_cpu(entry.nr_sectors),
			       (unsigned long long)le64_to_cpu(entry.flags));
		flags = le64_to_cpu(entry.flags);
		seek_size = log->sectorsize - sizeof(entry);
		if (!(flags & LOG_DISCARD_FLAG))
			seek_size += le64_to_cpu(entry.nr_sectors) *
				log->sectorsize;
		log->cur_pos = lseek(log->logfd, seek_size, SEEK_CUR);
		if (log->cur_pos == (off_t)-1) {
			fprintf(stderr, "Error seeking in file: %d\n", errno);
			return -1;
		}
		log->cur_entry++;
	}

	return 0;
}

/*
 * @log: the log we are manipulating.
 * @entry: the entry we read.
 * @read_data: read the extra data for the entry, your entry must be
 * log->sectorsize large.
 *
 * @return: 1 if we hit the end of the log, 0 we got the next entry, < 0 if
 * there was an error.
 *
 * Seek to the next entry in the log.
 */
int log_seek_next_entry(struct log *log, struct log_write_entry *entry,
			int read_data)
{
	size_t read_size = read_data ? log->sectorsize :
		sizeof(struct log_write_entry);
	u64 flags;
	char flags_buf[LOG_FLAGS_BUF_SIZE];
	ssize_t ret;

	if (log->cur_entry >= log->nr_entries)
		return 1;

	ret = read(log->logfd, entry, read_size);
	if (ret != read_size) {
		fprintf(stderr, "Error reading entry: %d\n", errno);
		return -1;
	}
	if (!log_entry_valid(entry)) {
		fprintf(stderr, "Malformed entry @%llu\n",
				log->cur_pos / log->sectorsize);
		return -1;
	}
	log->cur_entry++;

	if (read_size < log->sectorsize) {
		log->cur_pos = lseek(log->logfd,
			log->sectorsize - sizeof(struct log_write_entry), SEEK_CUR);
		if (log->cur_pos == (off_t)-1) {
			fprintf(stderr, "Error seeking in log: %d\n", errno);
			return -1;
		}
	} else {
		log->cur_pos += read_size;
	}
	flags = le64_to_cpu(entry->flags);
	entry_flags_to_str(flags, flags_buf);
	if (log_writes_verbose > 1)
		printf("seek entry %d@%llu: %llu, size %llu, flags 0x%llx(%s)\n",
		       (int)log->cur_entry - 1, log->cur_pos / log->sectorsize,
		       (unsigned long long)le64_to_cpu(entry->sector),
		       (unsigned long long)le64_to_cpu(entry->nr_sectors),
		       (unsigned long long)flags, flags_buf);

	read_size = le64_to_cpu(entry->nr_sectors) * log->sectorsize;
	if (!read_size || (flags & LOG_DISCARD_FLAG))
		return 0;

	log->cur_pos = lseek(log->logfd, read_size, SEEK_CUR);
	if (log->cur_pos == (off_t)-1) {
		fprintf(stderr, "Error seeking in log: %d\n", errno);
		return -1;
	}

	return 0;
}

/*
 * @logfile: the file that contains the write log.
 * @replayfile: the file/device to replay onto, can be NULL.
 *
 * Opens a logfile and makes sure it is valid and returns a struct log.
 */
struct log *log_open(char *logfile, char *replayfile)
{
	struct log *log;
	struct log_write_super super;
	ssize_t ret;

	log = malloc(sizeof(struct log));
	if (!log) {
		fprintf(stderr, "Couldn't alloc log\n");
		return NULL;
	}

	log->replayfd = -1;

	log->logfd = open(logfile, O_RDONLY);
	if (log->logfd < 0) {
		fprintf(stderr, "Couldn't open log %s: %d\n", logfile,
			errno);
		log_free(log);
		return NULL;
	}

	if (replayfile) {
		log->replayfd = open(replayfile, O_WRONLY);
		if (log->replayfd < 0) {
			fprintf(stderr, "Couldn't open replay file %s: %d\n",
				replayfile, errno);
			log_free(log);
			return NULL;
		}
	}

	ret = read(log->logfd, &super, sizeof(struct log_write_super));
	if (ret < sizeof(struct log_write_super)) {
		fprintf(stderr, "Error reading super: %d\n", errno);
		log_free(log);
		return NULL;
	}

	if (le64_to_cpu(super.magic) != WRITE_LOG_MAGIC) {
		fprintf(stderr, "Magic doesn't match\n");
		log_free(log);
		return NULL;
	}

	if (le64_to_cpu(super.version) != WRITE_LOG_VERSION) {
		fprintf(stderr, "Version mismatch, wanted %d, have %d\n",
			WRITE_LOG_VERSION, (int)le64_to_cpu(super.version));
		log_free(log);
		return NULL;
	}

	log->sectorsize = le32_to_cpu(super.sectorsize);
	log->nr_entries = le64_to_cpu(super.nr_entries);
	log->max_zero_size = 128 * 1024 * 1024;

	log->cur_pos = lseek(log->logfd, log->sectorsize - sizeof(super), SEEK_CUR);
	if (log->cur_pos == (off_t) -1) {
		fprintf(stderr, "Error seeking to first entry: %d\n", errno);
		log_free(log);
		return NULL;
	}
	log->cur_entry = 0;

	return log;
}
