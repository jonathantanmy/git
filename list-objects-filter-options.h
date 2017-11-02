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
	 * The raw argument value given on the command line or
	 * protocol request.  (The part after the "--keyword=".)
	 */
	char *raw_value;

	/*
	 * Parsed values. Only 1 will be set depending on the flags below.
	 */
	struct object_id *sparse_oid_value;
	char *sparse_path_value;
	unsigned long blob_limit_value;

	enum list_objects_filter_choice choice;
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

struct argv_array;
void arg_format_list_objects_filter(
	struct argv_array *aa,
	const struct list_objects_filter_options *filter_options);

#endif /* LIST_OBJECTS_FILTER_OPTIONS_H */
