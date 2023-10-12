#!/bin/sh
#
# Test Git in parallel
#

. ${0%/*}/lib.sh

case "$CI_OS_NAME" in
windows*) cmd //c mklink //j t\\.prove "$(cygpath -aw "$cache_dir/.prove")";;
*) ln -s "$cache_dir/.prove" t/.prove;;
esac

group "Run tests" make --quiet -C t T="$(cd t &&
	./helper/test-tool path-utils slice-tests "$1" "$2" t[0-9]*.sh |
	tr '\n' ' ')" ||
handle_failed_tests

# We only have one unit test at the moment, so run it in the first slice
if [ "$1" == "0" ] ; then
	group "Run unit tests" make --quiet -C t unit-tests-prove
fi

check_unignored_build_artifacts
