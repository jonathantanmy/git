#!/bin/sh

test_description='partial clone'

. ./test-lib.sh

delete_object () {
	rm $1/.git/objects/$(echo $2 | sed -e 's|^..|&/|')
}

pack_as_from_promisor () {
	HASH=$(git -C repo pack-objects .git/objects/pack/pack) &&
	>repo/.git/objects/pack/pack-$HASH.promisor
}

test_expect_success 'missing reflog object, but promised by a commit, passes fsck' '
	test_create_repo repo &&
	test_commit -C repo my_commit &&

	A=$(git -C repo commit-tree -m a HEAD^{tree}) &&
	C=$(git -C repo commit-tree -m c -p $A HEAD^{tree}) &&

	# Reference $A only from reflog, and delete it
	git -C repo branch my_branch "$A" &&
	git -C repo branch -f my_branch my_commit &&
	delete_object repo "$A" &&

	# State that we got $C, which refers to $A, from promisor
	printf "$C\n" | pack_as_from_promisor &&

	# Normally, it fails
	test_must_fail git -C repo fsck &&

	# But with the extension, it succeeds
	git -C repo config core.repositoryformatversion 1 &&
	git -C repo config extensions.partialcloneremote "arbitrary string" &&
	git -C repo fsck
'

test_expect_success 'missing reflog object, but promised by a tag, passes fsck' '
	rm -rf repo &&
	test_create_repo repo &&
	test_commit -C repo my_commit &&

	A=$(git -C repo commit-tree -m a HEAD^{tree}) &&
	git -C repo tag -a -m d my_tag_name $A &&
	T=$(git -C repo rev-parse my_tag_name) &&
	git -C repo tag -d my_tag_name &&

	# Reference $A only from reflog, and delete it
	git -C repo branch my_branch "$A" &&
	git -C repo branch -f my_branch my_commit &&
	delete_object repo "$A" &&

	# State that we got $T, which refers to $A, from promisor
	printf "$T\n" | pack_as_from_promisor &&

	git -C repo config core.repositoryformatversion 1 &&
	git -C repo config extensions.partialcloneremote "arbitrary string" &&
	git -C repo fsck
'

test_expect_success 'missing reflog object alone fails fsck, even with extension set' '
	rm -rf repo &&
	test_create_repo repo &&
	test_commit -C repo my_commit &&

	A=$(git -C repo commit-tree -m a HEAD^{tree}) &&
	B=$(git -C repo commit-tree -m b HEAD^{tree}) &&

	# Reference $A only from reflog, and delete it
	git -C repo branch my_branch "$A" &&
	git -C repo branch -f my_branch my_commit &&
	delete_object repo "$A" &&

	git -C repo config core.repositoryformatversion 1 &&
	git -C repo config extensions.partialcloneremote "arbitrary string" &&
	test_must_fail git -C repo fsck
'

test_done
