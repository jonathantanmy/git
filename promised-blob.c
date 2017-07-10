#include "cache.h"
#include "promised-blob.h"
#include "sha1-lookup.h"
#include "strbuf.h"
#include "run-command.h"
#include "sha1-array.h"
#include "config.h"
#include "pkt-line.h"
#include "varint.h"

#define ENTRY_SIZE (GIT_SHA1_RAWSZ + 8)
/*
 * A mmap-ed byte array of size (missing_blob_nr * ENTRY_SIZE). Each
 * ENTRY_SIZE-sized entry consists of the SHA-1 of the promised blob and its
 * 64-bit size in network byte order. The entries are sorted in ascending SHA-1
 * order.
 */
static char *promised_blobs;
static int64_t promised_blob_nr = -1;

static void prepare_promised_blobs(void)
{
	char *filename;
	int fd;
	struct stat st;

	if (promised_blob_nr >= 0)
		return;

	if (getenv("GIT_IGNORE_PROMISED_BLOBS")) {
		promised_blob_nr = 0;
		return;
	}
	
	filename = xstrfmt("%s/promisedblob", get_object_directory());
	fd = git_open(filename);
	if (fd < 0) {
		if (errno == ENOENT) {
			promised_blob_nr = 0;
			goto cleanup;
		}
		perror("prepare_promised_blobs");
		die("Could not open %s", filename);
	}
	if (fstat(fd, &st)) {
		perror("prepare_promised_blobs");
		die("Could not stat %s", filename);
	}
	if (st.st_size == 0) {
		promised_blob_nr = 0;
		goto cleanup;
	}
	if (st.st_size % ENTRY_SIZE) {
		die("Size of %s is not a multiple of %d", filename, ENTRY_SIZE);
	}

	promised_blobs = xmmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	promised_blob_nr = st.st_size / ENTRY_SIZE;

cleanup:
	free(filename);
	if (fd >= 0)
		close(fd);
}

int is_promised_blob(const struct object_id *oid, unsigned long *size)
{
	int result;

	prepare_promised_blobs();
	result = sha1_entry_pos(promised_blobs, ENTRY_SIZE, 0, 0,
				promised_blob_nr, promised_blob_nr, oid->hash);
	if (result >= 0) {
		if (size) {
			uint64_t size_nbo;
			char *sizeptr = promised_blobs +
					result * ENTRY_SIZE + GIT_SHA1_RAWSZ;
			memcpy(&size_nbo, sizeptr, sizeof(size_nbo));
			*size = ntohll(size_nbo);
		}
		return 1;
	}
	return 0;
}

int for_each_promised_blob(each_promised_blob_fn cb, void *data)
{
	struct object_id oid;
	int i, r;

	prepare_promised_blobs();
	for (i = 0; i < promised_blob_nr; i++) {
		memcpy(oid.hash, &promised_blobs[i * ENTRY_SIZE],
		       GIT_SHA1_RAWSZ);
		r = cb(&oid, data);
		if (r)
			return r;
	}
	return 0;
}

static char *promised_blob_command;

static int promised_blob_config(const char *conf_key, const char *value,
				void *cb)
{
	if (!strcmp(conf_key, "core.promisedblobcommand")) {
		promised_blob_command = xstrdup(value);
	}
	return 0;
}

static void ensure_promised_blob_configured(void)
{
	static int configured;
	if (configured)
		return;
	git_config(promised_blob_config, NULL);
	configured = 1;
}

int request_promised_blobs(const struct oid_array *blobs)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	const char *argv[] = {NULL, NULL};
	const char *env[] = {"GIT_IGNORE_PROMISED_BLOBS=1", NULL};
	int blobs_requested = 0;
	int i;

	for (i = 0; i < blobs->nr; i++) {
		if (is_promised_blob(&blobs->oid[i], NULL))
			break;
	}

	if (i == blobs->nr)
		/* Nothing to fetch */
		return 0;

	ensure_promised_blob_configured();
	if (!promised_blob_command)
		die("some promised blobs need to be fetched, but\n"
		    "no core.promisedblobcommand configured");

	argv[0] = promised_blob_command;
	cp.argv = argv;
	cp.env = env;
	cp.use_shell = 1;
	cp.in = -1;

	if (start_command(&cp))
		die("failed to start <%s>", promised_blob_command);

	for (; i < blobs->nr; i++) {
		if (is_promised_blob(&blobs->oid[i], NULL)) {
			write_in_full(cp.in, oid_to_hex(&blobs->oid[i]),
				      GIT_SHA1_HEXSZ);
			write_in_full(cp.in, "\n", 1);
			blobs_requested++;
		}
	}
	close(cp.in);

	if (finish_command(&cp))
		die("failed to finish <%s>", promised_blob_command);

	/*
	 * The command above may have updated packfiles, so update our record
	 * of them.
	 */
	reprepare_packed_git();
	return blobs_requested;
}

void merge_promises(int fd)
{
	static char scratch[1];

	char *next_existing, *existing_limit;
	char *out_filename;
	int out_fd;
	char buffer[LARGE_PACKET_DATA_MAX + 1];
	char *next = buffer;
	int size;
	uintmax_t new_promise_count;
	int i;
	char *promisedblob_filename;

	prepare_promised_blobs();
	if (promised_blob_nr > 0) {
		next_existing = promised_blobs;
		existing_limit = promised_blobs +
				 (promised_blob_nr * ENTRY_SIZE);
	} else {
		next_existing = existing_limit = scratch;
	}


	out_filename = xstrfmt("%s/tmp_promisedblob_XXXXXX",
			       get_object_directory());
	out_fd = git_mkstemp_mode(out_filename, 0444);
	if (out_fd < 0)
		die("Could not create temporary file %s", out_filename);

	/* avoid buffer overruns when decoding varints */
	buffer[LARGE_PACKET_DATA_MAX] = 0;

	size = packet_read(fd, NULL, NULL, buffer, sizeof(buffer), 0);
	new_promise_count = decode_varint((const unsigned char **) &next);
	for (i = 0; i < new_promise_count; i++) {
		unsigned long promised_size_nbo;
		if (next - buffer >= size) {
			size = packet_read(fd, NULL, NULL, buffer,
					   sizeof(buffer), 0);
			next = buffer;
		}
		for (;
		     next_existing < existing_limit &&
		     memcmp(next_existing, next, GIT_SHA1_RAWSZ) < 0;
		     next_existing += ENTRY_SIZE)
			write_in_full(out_fd, next_existing, ENTRY_SIZE);
		write_in_full(out_fd, next, GIT_SHA1_RAWSZ);
		next += GIT_SHA1_RAWSZ;
		promised_size_nbo =
			htonll((unsigned long)
			       decode_varint((const unsigned char **) &next));
		write_in_full(out_fd, &promised_size_nbo,
			      sizeof(promised_size_nbo));
	}
	/* Write the remaining old entries */
	for (;
	     next_existing < existing_limit;
	     next_existing += ENTRY_SIZE)
		write_in_full(out_fd, next_existing, ENTRY_SIZE);
	close(out_fd);

	if (promised_blob_nr > 0) {
		if (munmap(promised_blobs, promised_blob_nr * ENTRY_SIZE)) {
			perror("merge_promises");
			warning("Could not munmap promised blobs");
		}
	}
	/* Reset promised blobs, because they have changed */
	promised_blobs = NULL;
	promised_blob_nr = -1;

	promisedblob_filename = xstrfmt("%s/promisedblob",
					get_object_directory());
	if (rename(out_filename, promisedblob_filename)) {
		perror("merge_promises");
		unlink_or_warn(out_filename);
		die("Could not rename %s to %s", out_filename,
		    promisedblob_filename);
	}

	free(out_filename);
	free(promisedblob_filename);
}
