#ifndef PROMISED_BLOB_H
#define PROMISED_BLOB_H

#include "sha1-array.h"

/*
 * Returns 1 if oid is the name of a promised blob. If size is not NULL, also
 * returns its size.
 */
extern int is_promised_blob(const struct object_id *oid,
			    unsigned long *size);

typedef int each_promised_blob_fn(const struct object_id *oid, void *data);
int for_each_promised_blob(each_promised_blob_fn, void *);

/*
 * If any of the given blobs are promised blobs, invokes
 * core.promisedblobcommand with those blobs and returns the number of blobs
 * requested. No check is made as to whether the invocation actually populated
 * the repository with the promised blobs.
 *
 * If none of the given blobs are promised blobs, this function does not invoke
 * anything and returns 0.
 */
int request_promised_blobs(const struct oid_array *blobs);

#endif
