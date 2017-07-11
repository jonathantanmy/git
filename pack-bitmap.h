#ifndef PACK_BITMAP_H
#define PACK_BITMAP_H

#include "ewah/ewok.h"
#include "khash.h"
#include "pack-objects.h"

struct bitmap_disk_header {
	char magic[4];
	uint16_t version;
	uint16_t options;
	uint32_t entry_count;
	unsigned char checksum[20];
};

static const char BITMAP_IDX_SIGNATURE[] = {'B', 'I', 'T', 'M'};

#define NEEDS_BITMAP (1u<<22)

enum pack_bitmap_opts {
	BITMAP_OPT_FULL_DAG = 1,
	BITMAP_OPT_HASH_CACHE = 4,
};

enum pack_bitmap_flags {
	BITMAP_FLAG_REUSE = 0x1
};

typedef int (*show_reachable_fn)(
	const unsigned char *sha1,
	enum object_type type,
	int flags,
	uint32_t hash,
	struct packed_git *found_pack,
	off_t found_offset);

int prepare_bitmap_git(void);
void count_bitmap_commit_list(uint32_t *commits, uint32_t *trees, uint32_t *blobs, uint32_t *tags);
/*
 * Iterates over every marked object in the bitmap. If an invocation of
 * show_reachable returns non-zero, terminates the iteration and returns that
 * return value. Otherwise, returns 0.
 */
int traverse_bitmap_commit_list(show_reachable_fn show_reachable);
void test_bitmap_walk(struct rev_info *revs);
int prepare_bitmap_walk(struct rev_info *revs);
int reuse_partial_packfile_from_bitmap(struct packed_git **packfile, uint32_t *entries, off_t *up_to);
int rebuild_existing_bitmaps(struct packing_data *mapping, khash_sha1 *reused_bitmaps, int show_progress);

void bitmap_writer_show_progress(int show);
void bitmap_writer_set_checksum(unsigned char *sha1);
void bitmap_writer_build_type_index(struct pack_idx_entry **index, uint32_t index_nr);
void bitmap_writer_reuse_bitmaps(struct packing_data *to_pack);
void bitmap_writer_select_commits(struct commit **indexed_commits,
		unsigned int indexed_commits_nr, int max_bitmaps);
void bitmap_writer_build(struct packing_data *to_pack);
void bitmap_writer_finish(struct pack_idx_entry **index,
			  uint32_t index_nr,
			  const char *filename,
			  uint16_t options);

#endif
