#ifndef PACKFILE_H
#define PACKFILE_H

#include "oidset.h"

/*
 * Generate the filename to be used for a pack file with checksum "sha1" and
 * extension "ext". The result is written into the strbuf "buf", overwriting
 * any existing contents. A pointer to buf->buf is returned as a convenience.
 *
 * Example: odb_pack_name(out, sha1, "idx") => ".git/objects/pack/pack-1234..idx"
 */
extern char *odb_pack_name(struct strbuf *buf, const unsigned char *sha1, const char *ext);

/*
 * Return the name of the (local) packfile with the specified sha1 in
 * its name.  The return value is a pointer to memory that is
 * overwritten each time this function is called.
 */
extern char *sha1_pack_name(const unsigned char *sha1);

/*
 * Return the name of the (local) pack index file with the specified
 * sha1 in its name.  The return value is a pointer to memory that is
 * overwritten each time this function is called.
 */
extern char *sha1_pack_index_name(const unsigned char *sha1);

extern struct packed_git *parse_pack_index(unsigned char *sha1, const char *idx_path);

/* A hook to report invalid files in pack directory */
#define PACKDIR_FILE_PACK 1
#define PACKDIR_FILE_IDX 2
#define PACKDIR_FILE_GARBAGE 4
extern void (*report_garbage)(unsigned seen_bits, const char *path);

extern void prepare_packed_git(void);
extern void reprepare_packed_git(void);
extern void install_packed_git(struct packed_git *pack);

/*
 * Give a rough count of objects in the repository. This sacrifices accuracy
 * for speed.
 */
unsigned long approximate_object_count(void);

extern struct packed_git *find_sha1_pack(const unsigned char *sha1,
					 struct packed_git *packs);

extern void pack_report(void);

/*
 * mmap the index file for the specified packfile (if it is not
 * already mmapped).  Return 0 on success.
 */
extern int open_pack_index(struct packed_git *);

/*
 * munmap the index file for the specified packfile (if it is
 * currently mmapped).
 */
extern void close_pack_index(struct packed_git *);

extern unsigned char *use_pack(struct packed_git *, struct pack_window **, off_t, unsigned long *);
extern void close_pack_windows(struct packed_git *);
extern void close_all_packs(void);
extern void unuse_pack(struct pack_window **);
extern void clear_delta_base_cache(void);
extern struct packed_git *add_packed_git(const char *path, size_t path_len, int local);

/*
 * Make sure that a pointer access into an mmap'd index file is within bounds,
 * and can provide at least 8 bytes of data.
 *
 * Note that this is only necessary for variable-length segments of the file
 * (like the 64-bit extended offset table), as we compare the size to the
 * fixed-length parts when we open the file.
 */
extern void check_pack_index_ptr(const struct packed_git *p, const void *ptr);

/*
 * Return the SHA-1 of the nth object within the specified packfile.
 * Open the index if it is not already open.  The return value points
 * at the SHA-1 within the mmapped index.  Return NULL if there is an
 * error.
 */
extern const unsigned char *nth_packed_object_sha1(struct packed_git *, uint32_t n);
/*
 * Like nth_packed_object_sha1, but write the data into the object specified by
 * the the first argument.  Returns the first argument on success, and NULL on
 * error.
 */
extern const struct object_id *nth_packed_object_oid(struct object_id *, struct packed_git *, uint32_t n);

/*
 * Return the offset of the nth object within the specified packfile.
 * The index must already be opened.
 */
extern off_t nth_packed_object_offset(const struct packed_git *, uint32_t n);

/*
 * If the object named sha1 is present in the specified packfile,
 * return its offset within the packfile; otherwise, return 0.
 */
extern off_t find_pack_entry_one(const unsigned char *sha1, struct packed_git *);

extern int is_pack_valid(struct packed_git *);
extern void *unpack_entry(struct packed_git *, off_t, enum object_type *, unsigned long *);
extern unsigned long unpack_object_header_buffer(const unsigned char *buf, unsigned long len, enum object_type *type, unsigned long *sizep);
extern unsigned long get_size_from_delta(struct packed_git *, struct pack_window **, off_t);
extern int unpack_object_header(struct packed_git *, struct pack_window **, off_t *, unsigned long *);

extern void release_pack_memory(size_t);

/* global flag to enable extra checks when accessing packed objects */
extern int do_check_packed_object_crc;

extern int packed_object_info(struct packed_git *pack, off_t offset, struct object_info *);

extern void mark_bad_packed_object(struct packed_git *p, const unsigned char *sha1);
extern const struct packed_git *has_packed_and_bad(const unsigned char *sha1);

extern int find_pack_entry(const unsigned char *sha1, struct pack_entry *e);

extern int has_sha1_pack(const unsigned char *sha1);

extern int has_pack_index(const unsigned char *sha1);

/*
 * Only iterate over packs obtained from the promisor remote.
 */
#define FOR_EACH_OBJECT_PROMISOR_ONLY 2

/*
 * Iterate over packed objects in both the local
 * repository and any alternates repositories (unless the
 * FOR_EACH_OBJECT_LOCAL_ONLY flag, defined in cache.h, is set).
 */
typedef int each_packed_object_fn(const struct object_id *oid,
				  struct packed_git *pack,
				  uint32_t pos,
				  void *data);
extern int for_each_packed_object(each_packed_object_fn, void *, unsigned flags);

/*
 * Return 1 if an object in a promisor packfile is or refers to the given
 * object, 0 otherwise.
 */
extern int is_promisor_object(const struct object_id *oid);

#endif
