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

test_expect_success 'sha1_object_info_extended (through git cat-file)' '
	rm -rf server client &&

	git init server &&
	test_commit -C server 1 1.t abcdefgh &&
	HASH=$(git hash-object server/1.t) &&

	git init client &&
	git -C client config core.promisedblobcommand \
		"git -C \"$(pwd)/server\" pack-objects --stdout |
		 git unpack-objects" &&
	
	test_must_fail git -C client cat-file -p "$HASH"
'

test_expect_success '...succeeds if it is a promised blob' '
	printf "%s%016x" "$HASH" "$(wc -c <server/1.t)" |
		hex_pack >client/.git/objects/promisedblob &&
	git -C client cat-file -p "$HASH"
'

test_expect_success 'cat-file --batch-all-objects with promised blobs' '
	rm -rf client &&

	git init client &&
	git -C client config core.promisedblobcommand \
		"git -C \"$(pwd)/server\" pack-objects --stdout |
		 git unpack-objects" &&
	printf "%s%016x" "$HASH" "$(wc -c <server/1.t)" |
		hex_pack >client/.git/objects/promisedblob &&

	# Verify that the promised blob is printed
	git -C client cat-file --batch --batch-all-objects | tee out |
		grep abcdefgh
'

test_done
