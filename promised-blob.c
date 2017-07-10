#include "cache.h"
#include "promised-blob.h"
#include "sha1-lookup.h"
#include "strbuf.h"
#include "run-command.h"
#include "sha1-array.h"
#include "config.h"

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
