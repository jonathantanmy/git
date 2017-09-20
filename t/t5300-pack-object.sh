#!/bin/sh
#
# Copyright (c) 2005 Junio C Hamano
#

test_description='git pack-object

'
. ./test-lib.sh

TRASH=$(pwd)

test_expect_success \
    'setup' \
    'rm -f .git/index* &&
     perl -e "print \"a\" x 4096;" > a &&
     perl -e "print \"b\" x 4096;" > b &&
     perl -e "print \"c\" x 4096;" > c &&
     test-genrandom "seed a" 2097152 > a_big &&
     test-genrandom "seed b" 2097152 > b_big &&
     git update-index --add a a_big b b_big c &&
     cat c >d && echo foo >>d && git update-index --add d &&
     tree=$(git write-tree) &&
     commit=$(git commit-tree $tree </dev/null) && {
	 echo $tree &&
	 echo $commit &&
	 git ls-tree $tree | sed -e "s/.* \\([0-9a-f]*\\)	.*/\\1/"
     } >obj-list && {
	 git diff-tree --root -p $commit &&
	 while read object
	 do
	    t=$(git cat-file -t $object) &&
	    git cat-file $t $object || return 1
	 done <obj-list
     } >expect'

test_expect_success \
    'pack without delta' \
    'packname_1=$(git pack-objects --window=0 test-1 <obj-list)'

test_expect_success \
    'pack-objects with bogus arguments' \
    'test_must_fail git pack-objects --window=0 test-1 blah blah <obj-list'

rm -fr .git2
mkdir .git2

test_expect_success \
    'unpack without delta' \
    "GIT_OBJECT_DIRECTORY=.git2/objects &&
     export GIT_OBJECT_DIRECTORY &&
     git init &&
     git unpack-objects -n <test-1-${packname_1}.pack &&
     git unpack-objects <test-1-${packname_1}.pack"

unset GIT_OBJECT_DIRECTORY
cd "$TRASH/.git2"

test_expect_success \
    'check unpack without delta' \
    '(cd ../.git && find objects -type f -print) |
     while read path
     do
         cmp $path ../.git/$path || {
	     echo $path differs.
	     return 1
	 }
     done'
cd "$TRASH"

test_expect_success \
    'pack with REF_DELTA' \
    'pwd &&
     packname_2=$(git pack-objects test-2 <obj-list)'

rm -fr .git2
mkdir .git2

test_expect_success \
    'unpack with REF_DELTA' \
    'GIT_OBJECT_DIRECTORY=.git2/objects &&
     export GIT_OBJECT_DIRECTORY &&
     git init &&
     git unpack-objects -n <test-2-${packname_2}.pack &&
     git unpack-objects <test-2-${packname_2}.pack'

unset GIT_OBJECT_DIRECTORY
cd "$TRASH/.git2"
test_expect_success \
    'check unpack with REF_DELTA' \
    '(cd ../.git && find objects -type f -print) |
     while read path
     do
         cmp $path ../.git/$path || {
	     echo $path differs.
	     return 1
	 }
     done'
cd "$TRASH"

test_expect_success \
    'pack with OFS_DELTA' \
    'pwd &&
     packname_3=$(git pack-objects --delta-base-offset test-3 <obj-list)'

rm -fr .git2
mkdir .git2

test_expect_success \
    'unpack with OFS_DELTA' \
    'GIT_OBJECT_DIRECTORY=.git2/objects &&
     export GIT_OBJECT_DIRECTORY &&
     git init &&
     git unpack-objects -n <test-3-${packname_3}.pack &&
     git unpack-objects <test-3-${packname_3}.pack'

unset GIT_OBJECT_DIRECTORY
cd "$TRASH/.git2"
test_expect_success \
    'check unpack with OFS_DELTA' \
    '(cd ../.git && find objects -type f -print) |
     while read path
     do
         cmp $path ../.git/$path || {
	     echo $path differs.
	     return 1
	 }
     done'
cd "$TRASH"

test_expect_success 'compare delta flavors' '
	perl -e '\''
		defined($_ = -s $_) or die for @ARGV;
		exit 1 if $ARGV[0] <= $ARGV[1];
	'\'' test-2-$packname_2.pack test-3-$packname_3.pack
'

rm -fr .git2
mkdir .git2

test_expect_success \
    'use packed objects' \
    'GIT_OBJECT_DIRECTORY=.git2/objects &&
     export GIT_OBJECT_DIRECTORY &&
     git init &&
     cp test-1-${packname_1}.pack test-1-${packname_1}.idx .git2/objects/pack && {
	 git diff-tree --root -p $commit &&
	 while read object
	 do
	    t=$(git cat-file -t $object) &&
	    git cat-file $t $object || return 1
	 done <obj-list
    } >current &&
    cmp expect current'

test_expect_success \
    'use packed deltified (REF_DELTA) objects' \
    'GIT_OBJECT_DIRECTORY=.git2/objects &&
     export GIT_OBJECT_DIRECTORY &&
     rm -f .git2/objects/pack/test-* &&
     cp test-2-${packname_2}.pack test-2-${packname_2}.idx .git2/objects/pack && {
	 git diff-tree --root -p $commit &&
	 while read object
	 do
	    t=$(git cat-file -t $object) &&
	    git cat-file $t $object || return 1
	 done <obj-list
    } >current &&
    cmp expect current'

test_expect_success \
    'use packed deltified (OFS_DELTA) objects' \
    'GIT_OBJECT_DIRECTORY=.git2/objects &&
     export GIT_OBJECT_DIRECTORY &&
     rm -f .git2/objects/pack/test-* &&
     cp test-3-${packname_3}.pack test-3-${packname_3}.idx .git2/objects/pack && {
	 git diff-tree --root -p $commit &&
	 while read object
	 do
	    t=$(git cat-file -t $object) &&
	    git cat-file $t $object || return 1
	 done <obj-list
    } >current &&
    cmp expect current'

unset GIT_OBJECT_DIRECTORY

test_expect_success 'survive missing objects/pack directory' '
	(
		rm -fr missing-pack &&
		mkdir missing-pack &&
		cd missing-pack &&
		git init &&
		GOP=.git/objects/pack
		rm -fr $GOP &&
		git index-pack --stdin --keep=test <../test-3-${packname_3}.pack &&
		test -f $GOP/pack-${packname_3}.pack &&
		cmp $GOP/pack-${packname_3}.pack ../test-3-${packname_3}.pack &&
		test -f $GOP/pack-${packname_3}.idx &&
		cmp $GOP/pack-${packname_3}.idx ../test-3-${packname_3}.idx &&
		test -f $GOP/pack-${packname_3}.keep
	)
'

test_expect_success \
    'verify pack' \
    'git verify-pack	test-1-${packname_1}.idx \
			test-2-${packname_2}.idx \
			test-3-${packname_3}.idx'

test_expect_success \
    'verify pack -v' \
    'git verify-pack -v	test-1-${packname_1}.idx \
			test-2-${packname_2}.idx \
			test-3-${packname_3}.idx'

test_expect_success \
    'verify-pack catches mismatched .idx and .pack files' \
    'cat test-1-${packname_1}.idx >test-3.idx &&
     cat test-2-${packname_2}.pack >test-3.pack &&
     if git verify-pack test-3.idx
     then false
     else :;
     fi'

test_expect_success \
    'verify-pack catches a corrupted pack signature' \
    'cat test-1-${packname_1}.pack >test-3.pack &&
     echo | dd of=test-3.pack count=1 bs=1 conv=notrunc seek=2 &&
     if git verify-pack test-3.idx
     then false
     else :;
     fi'

test_expect_success \
    'verify-pack catches a corrupted pack version' \
    'cat test-1-${packname_1}.pack >test-3.pack &&
     echo | dd of=test-3.pack count=1 bs=1 conv=notrunc seek=7 &&
     if git verify-pack test-3.idx
     then false
     else :;
     fi'

test_expect_success \
    'verify-pack catches a corrupted type/size of the 1st packed object data' \
    'cat test-1-${packname_1}.pack >test-3.pack &&
     echo | dd of=test-3.pack count=1 bs=1 conv=notrunc seek=12 &&
     if git verify-pack test-3.idx
     then false
     else :;
     fi'

test_expect_success \
    'verify-pack catches a corrupted sum of the index file itself' \
    'l=$(wc -c <test-3.idx) &&
     l=$(expr $l - 20) &&
     cat test-1-${packname_1}.pack >test-3.pack &&
     printf "%20s" "" | dd of=test-3.idx count=20 bs=1 conv=notrunc seek=$l &&
     if git verify-pack test-3.pack
     then false
     else :;
     fi'

test_expect_success \
    'build pack index for an existing pack' \
    'cat test-1-${packname_1}.pack >test-3.pack &&
     git index-pack -o tmp.idx test-3.pack &&
     cmp tmp.idx test-1-${packname_1}.idx &&

     git index-pack test-3.pack &&
     cmp test-3.idx test-1-${packname_1}.idx &&

     cat test-2-${packname_2}.pack >test-3.pack &&
     git index-pack -o tmp.idx test-2-${packname_2}.pack &&
     cmp tmp.idx test-2-${packname_2}.idx &&

     git index-pack test-3.pack &&
     cmp test-3.idx test-2-${packname_2}.idx &&

     cat test-3-${packname_3}.pack >test-3.pack &&
     git index-pack -o tmp.idx test-3-${packname_3}.pack &&
     cmp tmp.idx test-3-${packname_3}.idx &&

     git index-pack test-3.pack &&
     cmp test-3.idx test-3-${packname_3}.idx &&

     cat test-1-${packname_1}.pack >test-4.pack &&
     rm -f test-4.keep &&
     git index-pack --keep=why test-4.pack &&
     cmp test-1-${packname_1}.idx test-4.idx &&
     test -f test-4.keep &&

     :'

test_expect_success 'unpacking with --strict' '

	for j in a b c d e f g
	do
		for i in 0 1 2 3 4 5 6 7 8 9
		do
			o=$(echo $j$i | git hash-object -w --stdin) &&
			echo "100644 $o	0 $j$i"
		done
	done >LIST &&
	rm -f .git/index &&
	git update-index --index-info <LIST &&
	LIST=$(git write-tree) &&
	rm -f .git/index &&
	head -n 10 LIST | git update-index --index-info &&
	LI=$(git write-tree) &&
	rm -f .git/index &&
	tail -n 10 LIST | git update-index --index-info &&
	ST=$(git write-tree) &&
	PACK5=$( git rev-list --objects "$LIST" "$LI" "$ST" | \
		git pack-objects test-5 ) &&
	PACK6=$( (
			echo "$LIST"
			echo "$LI"
			echo "$ST"
		 ) | git pack-objects test-6 ) &&
	test_create_repo test-5 &&
	(
		cd test-5 &&
		git unpack-objects --strict <../test-5-$PACK5.pack &&
		git ls-tree -r $LIST &&
		git ls-tree -r $LI &&
		git ls-tree -r $ST
	) &&
	test_create_repo test-6 &&
	(
		# tree-only into empty repo -- many unreachables
		cd test-6 &&
		test_must_fail git unpack-objects --strict <../test-6-$PACK6.pack
	) &&
	(
		# already populated -- no unreachables
		cd test-5 &&
		git unpack-objects --strict <../test-6-$PACK6.pack
	)
'

test_expect_success 'index-pack with --strict' '

	for j in a b c d e f g
	do
		for i in 0 1 2 3 4 5 6 7 8 9
		do
			o=$(echo $j$i | git hash-object -w --stdin) &&
			echo "100644 $o	0 $j$i"
		done
	done >LIST &&
	rm -f .git/index &&
	git update-index --index-info <LIST &&
	LIST=$(git write-tree) &&
	rm -f .git/index &&
	head -n 10 LIST | git update-index --index-info &&
	LI=$(git write-tree) &&
	rm -f .git/index &&
	tail -n 10 LIST | git update-index --index-info &&
	ST=$(git write-tree) &&
	PACK5=$( git rev-list --objects "$LIST" "$LI" "$ST" | \
		git pack-objects test-5 ) &&
	PACK6=$( (
			echo "$LIST"
			echo "$LI"
			echo "$ST"
		 ) | git pack-objects test-6 ) &&
	test_create_repo test-7 &&
	(
		cd test-7 &&
		git index-pack --strict --stdin <../test-5-$PACK5.pack &&
		git ls-tree -r $LIST &&
		git ls-tree -r $LI &&
		git ls-tree -r $ST
	) &&
	test_create_repo test-8 &&
	(
		# tree-only into empty repo -- many unreachables
		cd test-8 &&
		test_must_fail git index-pack --strict --stdin <../test-6-$PACK6.pack
	) &&
	(
		# already populated -- no unreachables
		cd test-7 &&
		git index-pack --strict --stdin <../test-6-$PACK6.pack
	)
'

test_expect_success 'honor pack.packSizeLimit' '
	git config pack.packSizeLimit 3m &&
	packname_10=$(git pack-objects test-10 <obj-list) &&
	test 2 = $(ls test-10-*.pack | wc -l)
'

test_expect_success 'verify resulting packs' '
	git verify-pack test-10-*.pack
'

test_expect_success 'tolerate packsizelimit smaller than biggest object' '
	git config pack.packSizeLimit 1 &&
	packname_11=$(git pack-objects test-11 <obj-list) &&
	test 5 = $(ls test-11-*.pack | wc -l)
'

test_expect_success 'verify resulting packs' '
	git verify-pack test-11-*.pack
'

test_expect_success 'set up pack for non-repo tests' '
	# make sure we have a pack with no matching index file
	cp test-1-*.pack foo.pack
'

test_expect_success 'index-pack --stdin complains of non-repo' '
	nongit test_must_fail git index-pack --stdin <foo.pack &&
	test_path_is_missing non-repo/.git
'

test_expect_success 'index-pack <pack> works in non-repo' '
	nongit git index-pack ../foo.pack &&
	test_path_is_file foo.idx
'

test_expect_success !PTHREADS,C_LOCALE_OUTPUT 'index-pack --threads=N or pack.threads=N warns when no pthreads' '
	test_must_fail git index-pack --threads=2 2>err &&
	grep ^warning: err >warnings &&
	test_line_count = 1 warnings &&
	grep -F "no threads support, ignoring --threads=2" err &&

	test_must_fail git -c pack.threads=2 index-pack 2>err &&
	grep ^warning: err >warnings &&
	test_line_count = 1 warnings &&
	grep -F "no threads support, ignoring pack.threads" err &&

	test_must_fail git -c pack.threads=2 index-pack --threads=4 2>err &&
	grep ^warning: err >warnings &&
	test_line_count = 2 warnings &&
	grep -F "no threads support, ignoring --threads=4" err &&
	grep -F "no threads support, ignoring pack.threads" err
'

test_expect_success !PTHREADS,C_LOCALE_OUTPUT 'pack-objects --threads=N or pack.threads=N warns when no pthreads' '
	git pack-objects --threads=2 --stdout --all </dev/null >/dev/null 2>err &&
	grep ^warning: err >warnings &&
	test_line_count = 1 warnings &&
	grep -F "no threads support, ignoring --threads" err &&

	git -c pack.threads=2 pack-objects --stdout --all </dev/null >/dev/null 2>err &&
	grep ^warning: err >warnings &&
	test_line_count = 1 warnings &&
	grep -F "no threads support, ignoring pack.threads" err &&

	git -c pack.threads=2 pack-objects --threads=4 --stdout --all </dev/null >/dev/null 2>err &&
	grep ^warning: err >warnings &&
	test_line_count = 2 warnings &&
	grep -F "no threads support, ignoring --threads" err &&
	grep -F "no threads support, ignoring pack.threads" err
'

lcut () {
	perl -e '$/ = undef; $_ = <>; s/^.{'$1'}//s; print $_'
}

test_expect_success '--blob-size-limit works with multiple excluded' '
	rm -rf server &&
	git init server &&
	printf a > server/a &&
	printf b > server/b &&
	printf c-very-long-file > server/c &&
	printf d-very-long-file > server/d &&
	git -C server add a b c d &&
	git -C server commit -m x &&

	git -C server rev-parse HEAD >objects &&
	git -C server pack-objects --revs --stdout --blob-max-bytes=10 <objects >my.pack &&

	# Ensure that only the small blobs are in the packfile
	git index-pack my.pack &&
	git verify-pack -v my.idx >objectlist &&
	grep $(git hash-object server/a) objectlist &&
	grep $(git hash-object server/b) objectlist &&
	! grep $(git hash-object server/c) objectlist &&
	! grep $(git hash-object server/d) objectlist
'

test_expect_success '--blob-size-limit never excludes special files' '
	rm -rf server &&
	git init server &&
	printf a-very-long-file > server/a &&
	printf a-very-long-file > server/.git-a &&
	printf b-very-long-file > server/b &&
	git -C server add a .git-a b &&
	git -C server commit -m x &&

	git -C server rev-parse HEAD >objects &&
	git -C server pack-objects --revs --stdout --blob-max-bytes=10 <objects >my.pack &&

	# Ensure that the .git-a blob is in the packfile, despite also
	# appearing as a non-.git file
	git index-pack my.pack &&
	git verify-pack -v my.idx >objectlist &&
	grep $(git hash-object server/a) objectlist
'

#
# WARNING!
#
# The following test is destructive.  Please keep the next
# two tests at the end of this file.
#

test_expect_success \
    'fake a SHA1 hash collision' \
    'test -f	.git/objects/c8/2de19312b6c3695c0c18f70709a6c535682a67 &&
     cp -f	.git/objects/9d/235ed07cd19811a6ceb342de82f190e49c9f68 \
		.git/objects/c8/2de19312b6c3695c0c18f70709a6c535682a67'

test_expect_success \
    'make sure index-pack detects the SHA1 collision' \
    'test_must_fail git index-pack -o bad.idx test-3.pack 2>msg &&
     test_i18ngrep "SHA1 COLLISION FOUND" msg'

test_expect_success \
    'make sure index-pack detects the SHA1 collision (large blobs)' \
    'test_must_fail git -c core.bigfilethreshold=1 index-pack -o bad.idx test-3.pack 2>msg &&
     test_i18ngrep "SHA1 COLLISION FOUND" msg'

test_done
