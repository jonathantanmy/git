#!/bin/sh

test_description='server-endpoint'

. ./test-lib.sh

test_expect_success 'fetch-blobs basic' '
	rm -rf server client &&
	git init server &&
	(
		cd server &&
		test_commit 0 &&
		test_commit 1 &&
		git repack -a -d --write-bitmap-index
	) &&
	BLOB0=$(git hash-object server/0.t) &&
	BLOB1=$(git hash-object server/1.t) &&
	printf "000ffetch-blobs0031want %s0031want %s0000" "$BLOB0" "$BLOB1" | git server-endpoint server >out &&

	test "$(head -1 out)" = "0008ACK" &&

	git init client &&
	sed 1d out | git -C client unpack-objects &&
	git -C client cat-file -e "$BLOB0" &&
	git -C client cat-file -e "$BLOB1"
'

test_expect_success 'fetch-blobs no such object' '
	rm -rf server client &&
	git init server &&
	(
		cd server &&
		test_commit 0 &&
		git repack -a -d --write-bitmap-index
	) &&
	BLOB0=$(git hash-object server/0.t) &&
	echo myblob >myblob &&
	MYBLOB=$(git hash-object myblob) &&
	printf "000ffetch-blobs0031want %s0031want %s0000" "$BLOB0" "$MYBLOB" | git server-endpoint server >out &&

	test_i18ngrep "$(printf "ERR not our blob.*%s" "$MYBLOB")" out
'

test_expect_success 'fetch-blobs unreachable' '
	rm -rf server client &&
	git init server &&
	(
		cd server &&
		test_commit 0 &&
		git repack -a -d --write-bitmap-index
	) &&
	BLOB0=$(git hash-object server/0.t) &&
	echo myblob >myblob &&
	MYBLOB=$(git -C server hash-object -w ../myblob) &&
	printf "000ffetch-blobs0031want %s0031want %s0000" "$BLOB0" "$MYBLOB" | git server-endpoint server >out &&

	test_i18ngrep "$(printf "ERR not our blob.*%s" "$MYBLOB")" out
'

test_done
