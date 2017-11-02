#include "cache.h"
#include "config.h"
#include "partial-clone-utils.h"

int is_partial_clone_registered(void)
{
	if (repository_format_partial_clone_remote ||
	    repository_format_partial_clone_filter)
		return 1;

	return 0;
}

void partial_clone_utils_register(
	const struct list_objects_filter_options *filter_options,
	const char *remote,
	const char *cmd_name)
{
	if (is_partial_clone_registered()) {
		/*
		 * The original partial-clone or a previous partial-fetch
		 * already registered the partial-clone settings.
		 * If we get here, we are in a subsequent partial-* command
		 * (with explicit filter args on the command line).
		 *
		 * For now, we restrict subsequent commands to one
		 * consistent with the original request.  We may relax
		 * this later after we get more experience with the
		 * partial-clone feature.
		 *
		 * [] Restrict to same remote because our dynamic
		 *    object loading only knows how to fetch objects
		 *    from 1 remote.
		 */
		assert(filter_options && filter_options->choice);
		assert(remote && *remote);

		if (strcmp(remote, repository_format_partial_clone_remote))
			die("%s --%s currently limited to remote '%s'",
			    cmd_name, CL_ARG__FILTER,
			    repository_format_partial_clone_remote);

		/*
		 * Treat the (possibly new) filter-spec as transient;
		 * use it for the current command, but do not overwrite
		 * the default.
		 */
		return;
	}

	repository_format_partial_clone_remote = xstrdup(remote);
	repository_format_partial_clone_filter = xstrdup(filter_options->raw_value);

	/*
	 * Force repo version > 0 to enable extensions namespace.
	 *
	 * TODO if already set > 0, we should not overwrite it. 
	 */
	git_config_set("core.repositoryformatversion", "1");

	/*
	 * Use the "extensions" namespace in the config to record
	 * the name of the remote used in the partial clone.
	 * This will help us return to that server when we need
	 * to backfill missing objects.
	 *
	 * It is also used to indicate that there *MAY* be
	 * missing objects so that subsequent commands don't
	 * immediately die if they hit one.
	 *
	 * Also remember the initial filter settings used by
	 * clone as a default for future fetches.
	 */
	git_config_set("extensions." KEY_PARTIALCLONEREMOTE,
		       repository_format_partial_clone_remote);
	git_config_set("extensions." KEY_PARTIALCLONEFILTER,
		       repository_format_partial_clone_filter);
}
