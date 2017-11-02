#include "cache.h"
#include "dir.h"
#include "tag.h"
#include "commit.h"
#include "tree.h"
#include "blob.h"
#include "diff.h"
#include "tree-walk.h"
#include "revision.h"
#include "list-objects.h"
#include "list-objects-filter.h"
#include "list-objects-filter-options.h"
#include "oidset.h"

/* See object.h and revision.h */
#define FILTER_REVISIT (1<<25)

/*
 * A filter for list-objects to omit ALL blobs from the traversal.
 * And to OPTIONALLY collect a list of the omitted OIDs.
 */
struct filter_blobs_none_data {
	struct oidset *omits;
};

static enum list_objects_filter_result filter_blobs_none(
	enum list_objects_filter_type filter_type,
	struct object *obj,
	const char *pathname,
	const char *filename,
	void *filter_data_)
{
	struct filter_blobs_none_data *filter_data = filter_data_;

	switch (filter_type) {
	default:
		die("unkown filter_type");
		return LOFR_ZERO;

	case LOFT_BEGIN_TREE:
		assert(obj->type == OBJ_TREE);
		/* always include all tree objects */
		return LOFR_MARK_SEEN | LOFR_SHOW;

	case LOFT_END_TREE:
		assert(obj->type == OBJ_TREE);
		return LOFR_ZERO;

	case LOFT_BLOB:
		assert(obj->type == OBJ_BLOB);
		assert((obj->flags & SEEN) == 0);

		if (filter_data->omits)
			oidset_insert(filter_data->omits, &obj->oid);
		return LOFR_MARK_SEEN; /* but not LOFR_SHOW (hard omit) */
	}
}

static void *filter_blobs_none__init(
	struct oidset *omitted,
	struct list_objects_filter_options *filter_options,
	filter_object_fn *filter_fn,
	filter_free_fn *filter_free_fn)
{
	struct filter_blobs_none_data *d = xcalloc(1, sizeof(*d));
	d->omits = omitted;

	*filter_fn = filter_blobs_none;
	*filter_free_fn = free;
	return d;
}

/*
 * A filter for list-objects to omit large blobs,
 * but always include ".git*" special files.
 * And to OPTIONALLY collect a list of the omitted OIDs.
 */
struct filter_blobs_limit_data {
	struct oidset *omits;
	unsigned long max_bytes;
};

static enum list_objects_filter_result filter_blobs_limit(
	enum list_objects_filter_type filter_type,
	struct object *obj,
	const char *pathname,
	const char *filename,
	void *filter_data_)
{
	struct filter_blobs_limit_data *filter_data = filter_data_;
	unsigned long object_length;
	enum object_type t;
	int is_special_filename;

	switch (filter_type) {
	default:
		die("unkown filter_type");
		return LOFR_ZERO;

	case LOFT_BEGIN_TREE:
		assert(obj->type == OBJ_TREE);
		/* always include all tree objects */
		return LOFR_MARK_SEEN | LOFR_SHOW;

	case LOFT_END_TREE:
		assert(obj->type == OBJ_TREE);
		return LOFR_ZERO;

	case LOFT_BLOB:
		assert(obj->type == OBJ_BLOB);
		assert((obj->flags & SEEN) == 0);

		is_special_filename = ((strncmp(filename, ".git", 4) == 0) &&
				       filename[4]);
		if (is_special_filename) {
			/*
			 * Alwayse include ".git*" special files (regardless
			 * of size).
			 *
			 * (This may cause us to include blobs that we do
			 * not have locally because we are only looking at
			 * the filename and don't actually have to read
			 * them.)
			 */
			goto include_it;
		}

		t = sha1_object_info(obj->oid.hash, &object_length);
		if (t != OBJ_BLOB) { /* probably OBJ_NONE */
			/*
			 * We DO NOT have the blob locally, so we cannot
			 * apply the size filter criteria.  Be conservative
			 * and force show it (and let the caller deal with
			 * the ambiguity).  (This matches the behavior above
			 * when the special filename matches.)
			 */
			goto include_it;
		}

		if (object_length < filter_data->max_bytes)
			goto include_it;

		/*
		 * Provisionally omit it.  We've already established
		 * that this blob is too big and doesn't have a special
		 * filename, so we *WANT* to omit it.  However, there
		 * may be a special file elsewhere in the tree that
		 * references this same blob, so we cannot reject it
		 * just yet.  Leave the LOFR_ bits unset so that *IF*
		 * the blob appears again in the traversal, we will
		 * be asked again.
		 *
		 * If we are keeping a list of the ommitted objects,
		 * provisionally add it to the list.
		 */

		if (filter_data->omits)
			oidset_insert(filter_data->omits, &obj->oid);
		return LOFR_ZERO;
	}

include_it:
	if (filter_data->omits)
		oidset_remove(filter_data->omits, &obj->oid);
	return LOFR_MARK_SEEN | LOFR_SHOW;
}

static void *filter_blobs_limit__init(
	struct oidset *omitted,
	struct list_objects_filter_options *filter_options,
	filter_object_fn *filter_fn,
	filter_free_fn *filter_free_fn)
{
	struct filter_blobs_limit_data *d = xcalloc(1, sizeof(*d));
	d->omits = omitted;
	d->max_bytes = filter_options->blob_limit_value;

	*filter_fn = filter_blobs_limit;
	*filter_free_fn = free;
	return d;
}

/*
 * A filter driven by a sparse-checkout specification to only
 * include blobs that a sparse checkout would populate.
 *
 * The sparse-checkout spec can be loaded from a blob with the
 * given OID or from a local pathname.  We allow an OID because
 * the repo may be bare or we may be doing the filtering on the
 * server.
 */
struct frame {
	int defval;
	int child_prov_omit : 1;
};

struct filter_sparse_data {
	struct oidset *omits;
	struct exclude_list el;

	size_t nr, alloc;
	struct frame *array_frame;
};

static enum list_objects_filter_result filter_sparse(
	enum list_objects_filter_type filter_type,
	struct object *obj,
	const char *pathname,
	const char *filename,
	void *filter_data_)
{
	struct filter_sparse_data *filter_data = filter_data_;
	int val, dtype;
	struct frame *frame;

	switch (filter_type) {
	default:
		die("unkown filter_type");
		return LOFR_ZERO;

	case LOFT_BEGIN_TREE:
		assert(obj->type == OBJ_TREE);
		dtype = DT_DIR;
		val = is_excluded_from_list(pathname, strlen(pathname),
					    filename, &dtype, &filter_data->el,
					    &the_index);
		if (val < 0)
			val = filter_data->array_frame[filter_data->nr].defval;

		ALLOC_GROW(filter_data->array_frame, filter_data->nr + 1,
			   filter_data->alloc);
		filter_data->nr++;
		filter_data->array_frame[filter_data->nr].defval = val;
		filter_data->array_frame[filter_data->nr].child_prov_omit = 0;

		/*
		 * A directory with this tree OID may appear in multiple
		 * places in the tree. (Think of a directory move, with
		 * no other changes.)  And with a different pathname, the
		 * is_excluded...() results for this directory and items
		 * contained within it may be different.  So we cannot
		 * mark it SEEN (yet), since that will prevent process_tree()
		 * from revisiting this tree object with other pathnames.
		 *
		 * Only SHOW the tree object the first time we visit this
		 * tree object.
		 *
		 * We always show all tree objects.  A future optimization
		 * may want to attempt to narrow this.
		 */
		if (obj->flags & FILTER_REVISIT)
			return LOFR_ZERO;
		obj->flags |= FILTER_REVISIT;
		return LOFR_SHOW;

	case LOFT_END_TREE:
		assert(obj->type == OBJ_TREE);
		assert(filter_data->nr > 0);

		frame = &filter_data->array_frame[filter_data->nr];
		filter_data->nr--;

		/*
		 * Tell our parent directory if any of our children were
		 * provisionally omitted.
		 */
		filter_data->array_frame[filter_data->nr].child_prov_omit |=
			frame->child_prov_omit;

		/*
		 * If there are NO provisionally omitted child objects (ALL child
		 * objects in this folder were INCLUDED), then we can mark the
		 * folder as SEEN (so we will not have to revisit it again).
		 */
		if (!frame->child_prov_omit)
			return LOFR_MARK_SEEN;
		return LOFR_ZERO;

	case LOFT_BLOB:
		assert(obj->type == OBJ_BLOB);
		assert((obj->flags & SEEN) == 0);

		frame = &filter_data->array_frame[filter_data->nr];

		dtype = DT_REG;
		val = is_excluded_from_list(pathname, strlen(pathname),
					    filename, &dtype, &filter_data->el,
					    &the_index);
		if (val < 0)
			val = frame->defval;
		if (val > 0) {
			if (filter_data->omits)
				oidset_remove(filter_data->omits, &obj->oid);
			return LOFR_MARK_SEEN | LOFR_SHOW;
		}

		/*
		 * Provisionally omit it.  We've already established that
		 * this pathname is not in the sparse-checkout specification
		 * with the CURRENT pathname, so we *WANT* to omit this blob.
		 *
		 * However, a pathname elsewhere in the tree may also
		 * reference this same blob, so we cannot reject it yet.
		 * Leave the LOFR_ bits unset so that if the blob appears
		 * again in the traversal, we will be asked again.
		 */
		if (filter_data->omits)
			oidset_insert(filter_data->omits, &obj->oid);

		/*
		 * Remember that at least 1 blob in this tree was
		 * provisionally omitted.  This prevents us from short
		 * cutting the tree in future iterations.
		 */
		frame->child_prov_omit = 1;
		return LOFR_ZERO;
	}
}


static void filter_sparse_free(void *filter_data)
{
	struct filter_sparse_data *d = filter_data;
	/* TODO free contents of 'd' */
	free(d);
}

static void *filter_sparse_oid__init(
	struct oidset *omitted,
	struct list_objects_filter_options *filter_options,
	filter_object_fn *filter_fn,
	filter_free_fn *filter_free_fn)
{
	struct filter_sparse_data *d = xcalloc(1, sizeof(*d));
	d->omits = omitted;
	if (add_excludes_from_blob_to_list(filter_options->sparse_oid_value,
					   NULL, 0, &d->el) < 0)
		die("could not load filter specification");

	ALLOC_GROW(d->array_frame, d->nr + 1, d->alloc);
	d->array_frame[d->nr].defval = 0; /* default to include */
	d->array_frame[d->nr].child_prov_omit = 0;

	*filter_fn = filter_sparse;
	*filter_free_fn = filter_sparse_free;
	return d;
}

static void *filter_sparse_path__init(
	struct oidset *omitted,
	struct list_objects_filter_options *filter_options,
	filter_object_fn *filter_fn,
	filter_free_fn *filter_free_fn)
{
	struct filter_sparse_data *d = xcalloc(1, sizeof(*d));
	d->omits = omitted;
	if (add_excludes_from_file_to_list(filter_options->sparse_path_value,
					   NULL, 0, &d->el, NULL) < 0)
		die("could not load filter specification");

	ALLOC_GROW(d->array_frame, d->nr + 1, d->alloc);
	d->array_frame[d->nr].defval = 0; /* default to include */
	d->array_frame[d->nr].child_prov_omit = 0;

	*filter_fn = filter_sparse;
	*filter_free_fn = filter_sparse_free;
	return d;
}

typedef void *(*filter_init_fn)(
	struct oidset *omitted,
	struct list_objects_filter_options *filter_options,
	filter_object_fn *filter_fn,
	filter_free_fn *filter_free_fn);

/*
 * Must match "enum list_objects_filter_choice".
 */
static filter_init_fn s_filters[] = {
	NULL,
	filter_blobs_none__init,
	filter_blobs_limit__init,
	filter_sparse_oid__init,
	filter_sparse_path__init,
};

void *list_objects_filter__init(
	struct oidset *omitted,
	struct list_objects_filter_options *filter_options,
	filter_object_fn *filter_fn,
	filter_free_fn *filter_free_fn)
{
	filter_init_fn init_fn;

	assert((sizeof(s_filters) / sizeof(s_filters[0])) == LOFC__COUNT);

	if (filter_options->choice >= LOFC__COUNT)
		die("invalid list-objects filter choice: %d",
		    filter_options->choice);

	init_fn = s_filters[filter_options->choice];
	if (init_fn)
		return init_fn(omitted, filter_options,
			       filter_fn, filter_free_fn);
	*filter_fn = NULL;
	*filter_free_fn = NULL;
	return NULL;
}
