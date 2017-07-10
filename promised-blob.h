#ifndef PROMISED_BLOB_H
#define PROMISED_BLOB_H

/*
 * Returns 1 if oid is the name of a promised blob. If size is not NULL, also
 * returns its size.
 */
extern int is_promised_blob(const struct object_id *oid,
			    unsigned long *size);

typedef int each_promised_blob_fn(const struct object_id *oid, void *data);
int for_each_promised_blob(each_promised_blob_fn, void *);

#endif
