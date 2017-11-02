#ifndef LIST_OBJECTS_FILTER_H
#define LIST_OBJECTS_FILTER_H

/*
 * During list-object traversal we allow certain objects to be
 * filtered (omitted) from the result.  The active filter uses
 * these result values to guide list-objects.
 *
 * _ZERO      : Do nothing with the object at this time.  It may
 *              be revisited if it appears in another place in
 *              the tree or in another commit during the overall
 *              traversal.
 *
 * _MARK_SEEN : Mark this object as "SEEN" in the object flags.
 *              This will prevent it from being revisited during
 *              the remainder of the traversal.  This DOES NOT
 *              imply that it will be included in the results.
 *
 * _SHOW      : Show this object in the results (call show() on it).
 *              In general, objects should only be shown once, but
 *              this result DOES NOT imply that we mark it SEEN.
 *
 * Most of the time, you want the combination (_MARK_SEEN | _SHOW)
 * but they can be used independently, such as when sparse-checkout
 * pattern matching is being applied.
 *
 * A _MARK_SEEN without _SHOW can be called a hard-omit -- the
 * object is not shown and will never be reconsidered (unless a
 * previous iteration has already shown it).
 *
 * A _ZERO is can be called a provisional-omit -- the object is
 * not shown, but *may* be revisited (if the object appears again
 * in the traversal).  Therefore, it will be omitted from the
 * results *unless* a later iteration causes it to be shown.
 */
enum list_objects_filter_result {
	LOFR_ZERO      = 0,
	LOFR_MARK_SEEN = 1<<0,
	LOFR_SHOW      = 1<<1,
};

enum list_objects_filter_type {
	LOFT_BEGIN_TREE,
	LOFT_END_TREE,
	LOFT_BLOB
};

typedef enum list_objects_filter_result (*filter_object_fn)(
	enum list_objects_filter_type filter_type,
	struct object *obj,
	const char *pathname,
	const char *filename,
	void *filter_data);

typedef void (*filter_free_fn)(void *filter_data);

struct oidset;
struct list_objects_filter_options;

void traverse_commit_list_filtered(
	struct list_objects_filter_options *filter_options,
	struct rev_info *revs,
	show_commit_fn show_commit,
	show_object_fn show_object,
	void *show_data,
	struct oidset *omitted);

/*
 * Constructor for the set of defined list-objects filters.
 * Returns a generic "void *filter_data".
 *
 * The returned "filter_fn" will be used by traverse_commit_list()
 * to filter the results.
 *
 * The returned "filter_free_fn" is a destructor for the
 * filter_data.
 */
void *list_objects_filter__init(
	struct oidset *omitted,
	struct list_objects_filter_options *filter_options,
	filter_object_fn *filter_fn,
	filter_free_fn *filter_free_fn);

#endif /* LIST_OBJECTS_FILTER_H */
