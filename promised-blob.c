#include "cache.h"
#include "promised-blob.h"
#include "sha1-lookup.h"
#include "strbuf.h"

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
