#include "cache.h"
#include "commit.h"
#include "config.h"
#include "revision.h"
#include "argv-array.h"
#include "list-objects.h"
#include "list-objects-filter.h"
#include "list-objects-filter-options.h"

/*
 * Parse value of the argument to the "filter" keword.
 * On the command line this looks like:
 *       --filter=<arg>
 * and in the pack protocol as:
 *       "filter" SP <arg>
 *
 * <arg> ::= blob:none
 *           blob:limit=<n>[kmg]
 *           sparse:oid=<oid-expression>
 *           sparse:path=<pathname>
 */
int parse_list_objects_filter(struct list_objects_filter_options *filter_options,
			      const char *arg)
{
	struct object_context oc;
	struct object_id sparse_oid;
	const char *v0;
	const char *v1;

	if (filter_options->choice)
		die(_("multiple object filter types cannot be combined"));

	/*
	 * TODO consider rejecting 'arg' if it contains any
	 * TODO injection characters (since we might send this
	 * TODO to a sub-command or to the server and we don't
	 * TODO want to deal with legacy quoting/escaping for
	 * TODO a new feature).
	 */

	filter_options->raw_value = strdup(arg);

	if (skip_prefix(arg, "blob:", &v0) || skip_prefix(arg, "blobs:", &v0)) {
		if (!strcmp(v0, "none")) {
			filter_options->choice = LOFC_BLOB_NONE;
			return 0;
		}

		if (skip_prefix(v0, "limit=", &v1) &&
		    git_parse_ulong(v1, &filter_options->blob_limit_value)) {
			filter_options->choice = LOFC_BLOB_LIMIT;
			return 0;
		}
	}
	else if (skip_prefix(arg, "sparse:", &v0)) {
		if (skip_prefix(v0, "oid=", &v1)) {
			filter_options->choice = LOFC_SPARSE_OID;
			if (!get_oid_with_context(v1, GET_OID_BLOB,
						  &sparse_oid, &oc)) {
				/*
				 * We successfully converted the <oid-expr>
				 * into an actual OID.  Rewrite the raw_value
				 * in canonoical form with just the OID.
				 * (If we send this request to the server, we
				 * want an absolute expression rather than a
				 * local-ref-relative expression.)
				 */
				free((char *)filter_options->raw_value);
				filter_options->raw_value =
					xstrfmt("sparse:oid=%s",
						oid_to_hex(&sparse_oid));
				filter_options->sparse_oid_value =
					oiddup(&sparse_oid);
			} else {
				/*
				 * We could not turn the <oid-expr> into an
				 * OID.  Leave the raw_value as is in case
				 * the server can parse it.  (It may refer to
				 * a branch, commit, or blob we don't have.)
				 */
			}
			return 0;
		}

		if (skip_prefix(v0, "path=", &v1)) {
			filter_options->choice = LOFC_SPARSE_PATH;
			filter_options->sparse_path_value = strdup(v1);
			return 0;
		}
	}

	die(_("invalid filter expression '%s'"), arg);
	return 0;
}

int opt_parse_list_objects_filter(const struct option *opt,
				  const char *arg, int unset)
{
	struct list_objects_filter_options *filter_options = opt->value;

	assert(arg);
	assert(!unset);

	return parse_list_objects_filter(filter_options, arg);
}

void arg_format_list_objects_filter(
	struct argv_array *argv_array,
	const struct list_objects_filter_options *filter_options)
{
	if (!filter_options->choice)
		return;

	/*
	 * TODO Think about quoting the value.
	 */
	argv_array_pushf(argv_array, "--%s=%s", CL_ARG__FILTER,
			 filter_options->raw_value);
}
