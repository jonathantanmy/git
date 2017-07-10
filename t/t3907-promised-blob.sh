#!/bin/sh

test_description='promised blobs'

. ./test-lib.sh

test_expect_success 'fsck fails on missing blobs' '
	rm -rf repo &&

	git init repo &&
	test_commit -C repo 1 &&
	HASH=$(git hash-object repo/1.t) &&

	rm repo/.git/objects/$(echo $HASH | cut -c1-2)/$(echo $HASH | cut -c3-40) &&
	test_must_fail git -C repo fsck
'

test_expect_success '...but succeeds if it is a promised blob' '
	printf "%s%016x" "$HASH" "$(wc -c <repo/1.t)" |
		hex_pack >repo/.git/objects/promisedblob &&
	git -C repo fsck
'

test_expect_success '...but fails again with GIT_IGNORE_PROMISED_BLOBS' '
	GIT_IGNORE_PROMISED_BLOBS=1 test_must_fail git -C repo fsck &&
	unset GIT_IGNORE_PROMISED_BLOBS
'

test_done
