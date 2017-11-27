#ifndef LIST_OBJECTS_FILTER_OPTIONS_H
#define LIST_OBJECTS_FILTER_OPTIONS_H

#include "parse-options.h"

/*
 * The list of defined filters for list-objects.
 */
enum list_objects_filter_choice {
	LOFC_DISABLED = 0,
	LOFC_BLOB_NONE,
	LOFC_BLOB_LIMIT,
	LOFC_SPARSE_OID,
	LOFC_SPARSE_PATH,
	LOFC__COUNT /* must be last */
};

struct list_objects_filter_options {
	/*
	 * 'filter_spec' is the raw argument value given on the command line
	 * or protocol request.  (The part after the "--keyword=".)  For
	 * commands that launch filtering sub-processes, this value should be
	 * passed to them as received by the current process.
	 */
	char *filter_spec;

	/*
	 * 'choice' is determined by parsing the filter-spec.  This indicates
	 * the filtering algorithm to use.
	 */
	enum list_objects_filter_choice choice;

	/*
	 * Parsed values (fields) from within the filter-spec.  These are
	 * choice-specific; not all values will be defined for any given
	 * choice.
	 */
	struct object_id *sparse_oid_value;
	char *sparse_path_value;
	unsigned long blob_limit_value;
};

/* Normalized command line arguments */
#define CL_ARG__FILTER "filter"

int parse_list_objects_filter(
	struct list_objects_filter_options *filter_options,
	const char *arg);

int opt_parse_list_objects_filter(const struct option *opt,
				  const char *arg, int unset);

#define OPT_PARSE_LIST_OBJECTS_FILTER(fo) \
	{ OPTION_CALLBACK, 0, CL_ARG__FILTER, fo, N_("args"), \
	  N_("object filtering"), PARSE_OPT_NONEG, \
	  opt_parse_list_objects_filter }

#endif /* LIST_OBJECTS_FILTER_OPTIONS_H */
