#!/bin/sh

test_description='git partial clone'

. ./test-lib.sh

# create a normal "src" repo where we can later create new commits.
# expect_1.oids will contain a list of the OIDs of all blobs.
test_expect_success 'setup normal src repo' '
	echo "{print \$1}" >print_1.awk &&
	echo "{print \$2}" >print_2.awk &&

	git init src &&
	for n in 1 2 3 4
	do
		echo "This is file: $n" > src/file.$n.txt
		git -C src add file.$n.txt
		git -C src commit -m "file $n"
		git -C src ls-files -s file.$n.txt >>temp
	done &&
	awk -f print_2.awk <temp | sort >expect_1.oids &&
	test "$(wc -l <expect_1.oids)" = "4"
'

# bare clone "src" giving "srv.bare" for use as our server.
test_expect_success 'setup bare clone for server' '
	git clone --bare "file://$(pwd)/src" srv.bare &&
	git -C srv.bare config --local uploadpack.allowfilter 1 &&
	git -C srv.bare config --local uploadpack.allowanysha1inwant 1
'

# do basic partial clone from "srv.bare"
# confirm we are missing all of the known blobs.
# confirm partial clone was registered in the local config.
test_expect_success 'do partial clone 1' '
	git clone --no-checkout --filter=blob:none "file://$(pwd)/srv.bare" pc1 &&
	git -C pc1 rev-list HEAD --quiet --objects --missing=print \
		| awk -f print_1.awk \
		| sed "s/?//" \
		| sort >observed.oids &&
	test_cmp expect_1.oids observed.oids &&
	test "$(git -C pc1 config --local core.repositoryformatversion)" = "1" &&
	test "$(git -C pc1 config --local extensions.partialclone)" = "origin" &&
	test "$(git -C pc1 config --local core.partialclonefilter)" = "blob:none"
'

# checkout master to force dynamic object fetch of blobs at HEAD.
# confirm we now have the expected blobs in a new packfile.
test_expect_success 'verify checkout with dynamic object fetch' '
	git -C pc1 checkout master &&
	(	cd pc1/.git/objects/pack;
		git verify-pack -v *.pack
	) >temp &&
	grep blob <temp \
		| awk -f print_1.awk \
		| sort >observed.oids &&
	test_cmp expect_1.oids observed.oids
'

# create new commits in "src" repo and push to "srv.bare".
# repack srv.bare just to make it easy to count the blobs.
# expect_2.oids will contain a list of the OIDs of all blobs.
test_expect_success 'push new commits to server' '
	git -C src remote add srv "file://$(pwd)/srv.bare" &&
	for x in a b c d
	do
		echo "Mod $x" >>src/file.1.txt
		git -C src add file.1.txt
		git -C src commit -m "mod $x"
	done &&
	git -C src push -u srv master &&
	git -C srv.bare repack &&
	(	cd srv.bare/objects/pack;
		git verify-pack -v *.pack
	) >temp &&
	grep blob <temp \
		| awk -f print_1.awk \
		| sort >expect_2.oids &&
	test "$(wc -l <expect_2.oids)" = "8" &&
	git -C src blame master -- file.1.txt >expect.blame
'

# fetch in the partial clone repo from the server (the promisor remote).
# verify that fetch was a "partial fetch".
# [] that it inherited the filter settings
# [] that is DOES NOT have the new blobs.
test_expect_success 'partial fetch inherits filter settings' '
	git -C pc1 fetch origin &&
	(	cd pc1/.git/objects/pack;
		git verify-pack -v *.pack
	) >temp &&
	grep blob <temp \
		| awk -f print_1.awk \
		| sort >observed.oids &&
	test_cmp expect_1.oids observed.oids
'

# force dynamic object fetch using diff.
# we should only get 1 new blob (for the file in origin/master).
# it should be in a new packfile (since the promisor boundary is
# currently a packfile, it should not get unpacked upon receipt.)
test_expect_success 'verify diff causes dynamic object fetch' '
	test "$(wc -l <observed.oids)" = "4" &&
		cat observed.oids &&
	git -C pc1 diff master..origin/master -- file.1.txt &&
	(	cd pc1/.git/objects/pack;
		git verify-pack -v *.pack
	) >temp &&
	grep blob <temp \
		| awk -f print_1.awk \
		| sort >observed.oids &&
		cat observed.oids &&
	test "$(wc -l <observed.oids)" = "4"
'

# force dynamic object fetch using blame.
# we should get the intermediate blobs for the file.
# we may get multiple packfiles (one for each blob/commit) or
# we may get a single new packfile, but we don't care.
test_expect_success 'verify blame causes dynamic object fetch' '
	git -C pc1 blame origin/master -- file.1.txt >observed.blame &&
	test_cmp expect.blame observed.blame
'

test_done
