#ifndef PARTIAL_CLONE_UTILS_H
#define PARTIAL_CLONE_UTILS_H

#include "list-objects-filter-options.h"

/*
 * Register that partial-clone was used to create the repo and
 * update the config on disk.
 *
 * If nothing else, this indicates that the ODB may have missing
 * objects and that various commands should handle that gracefully.
 *
 * Record the remote used for the clone so that we know where
 * to get missing objects in the future.
 *
 * Also record the filter expression so that we know something
 * about the missing objects (e.g., size-limit vs sparse).
 *
 * May also be used by a partial-fetch following a normal clone
 * to turn on the above tracking.
 */ 
extern void partial_clone_utils_register(
	const struct list_objects_filter_options *filter_options,
	const char *remote,
	const char *cmd_name);

/*
 * Return 1 if partial-clone was used to create the repo
 * or a subsequent partial-fetch was used.  This is an
 * indicator that there may be missing objects.
 */
extern int is_partial_clone_registered(void);

#endif /* PARTIAL_CLONE_UTILS_H */
