#!/bin/sh

test_description='miscellaneous rev-list tests'

. ./test-lib.sh


test_expect_success setup '
	echo content1 >wanted_file &&
	echo content2 >unwanted_file &&
	git add wanted_file unwanted_file &&
	git commit -m one
'

test_expect_success 'rev-list --objects heeds pathspecs' '
	git rev-list --objects HEAD -- wanted_file >output &&
	grep wanted_file output &&
	! grep unwanted_file output
'

test_expect_success 'rev-list --objects with pathspecs and deeper paths' '
	mkdir foo &&
	>foo/file &&
	git add foo/file &&
	git commit -m two &&

	git rev-list --objects HEAD -- foo >output &&
	grep foo/file output &&

	git rev-list --objects HEAD -- foo/file >output &&
	grep foo/file output &&
	! grep unwanted_file output
'

test_expect_success 'rev-list --objects with pathspecs and copied files' '
	git checkout --orphan junio-testcase &&
	git rm -rf . &&

	mkdir two &&
	echo frotz >one &&
	cp one two/three &&
	git add one two/three &&
	test_tick &&
	git commit -m that &&

	ONE=$(git rev-parse HEAD:one) &&
	git rev-list --objects HEAD two >output &&
	grep "$ONE two/three" output &&
	! grep one output
'

test_expect_success 'rev-list A..B and rev-list ^A B are the same' '
	git commit --allow-empty -m another &&
	git tag -a -m "annotated" v1.0 &&
	git rev-list --objects ^v1.0^ v1.0 >expect &&
	git rev-list --objects v1.0^..v1.0 >actual &&
	test_cmp expect actual
'

test_expect_success 'propagate uninteresting flag down correctly' '
	git rev-list --objects ^HEAD^{tree} HEAD^{tree} >actual &&
	>expect &&
	test_cmp expect actual
'

test_expect_success 'symleft flag bit is propagated down from tag' '
	git log --format="%m %s" --left-right v1.0...master >actual &&
	cat >expect <<-\EOF &&
	> two
	> one
	< another
	< that
	EOF
	test_cmp expect actual
'

test_expect_success 'rev-list can show index objects' '
	# Of the blobs and trees in the index, note:
	#
	#   - we do not show two/three, because it is the
	#     same blob as "one", and we show objects only once
	#
	#   - we do show the tree "two", because it has a valid cache tree
	#     from the last commit
	#
	#   - we do not show the root tree; since we updated the index, it
	#     does not have a valid cache tree
	#
	cat >expect <<-\EOF &&
	8e4020bb5a8d8c873b25de15933e75cc0fc275df one
	d9d3a7417b9605cfd88ee6306b28dadc29e6ab08 only-in-index
	9200b628cf9dc883a85a7abc8d6e6730baee589c two
	EOF
	echo only-in-index >only-in-index &&
	git add only-in-index &&
	git rev-list --objects --indexed-objects >actual &&
	test_cmp expect actual
'

test_expect_success '--bisect and --first-parent can not be combined' '
	test_must_fail git rev-list --bisect --first-parent HEAD
'

test_expect_success '--header shows a NUL after each commit' '
	# We know that there is no Q in the true payload; names and
	# addresses of the authors and the committers do not have
	# any, and object names or header names do not, either.
	git rev-list --header --max-count=2 HEAD |
	nul_to_q |
	grep "^Q" >actual &&
	cat >expect <<-EOF &&
	Q$(git rev-parse HEAD~1)
	Q
	EOF
	test_cmp expect actual
'

test_expect_success '--trust-promises' '
	rm -rf .git &&
	git init &&

	test_commit foo &&
	HASH=$(git hash-object foo.t) &&
	rm .git/objects/$(echo $HASH | cut -c1-2)/$(echo $HASH | cut -c3-40) &&

	# Since the missing blob is not a promised blob, rev-list will
	# fail regardless of the presence of --trust-promises.
	test_must_fail git rev-list --objects HEAD &&
	test_must_fail git rev-list --objects --trust-promises HEAD &&

	# Add the blob as a promised blob.
	printf "%s%016x" "$HASH" "$(wc -c <foo.t)" |
		hex_pack >.git/objects/promisedblob &&

	# Now it succeeds if --trust-promises is set.
	test_must_fail git rev-list --objects HEAD &&
	git rev-list --objects --trust-promises HEAD
'

test_done
