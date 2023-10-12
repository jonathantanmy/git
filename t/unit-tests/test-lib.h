#ifndef TEST_LIB_H
#define TEST_LIB_H

#include "git-compat-util.h"

/*
 * Run a test function, returns 1 if the test succeeds, 0 if it
 * fails. If test_skip_all() has been called then the test will not be
 * run. The description for each test should be unique. For example:
 *
 *  TEST(test_something(arg1, arg2), "something %d %d", arg1, arg2)
 */
#define TEST(t, ...)					\
	test__run_end(test__run_begin() ? 0 : (t, 1),	\
		      TEST_LOCATION(),  __VA_ARGS__)

/*
 * Print a test plan, should be called before any tests. If the number
 * of tests is not known in advance test_done() will automatically
 * print a plan at the end of the test program.
 */
void test_plan(int count);

/*
 * test_done() must be called at the end of main(). It will print the
 * plan if plan() was not called at the beginning of the test program
 * and returns the exit code for the test program.
 */
int test_done(void);

/* Skip the current test. */
__attribute__((format (printf, 1, 2)))
void test_skip(const char *format, ...);

/* Skip all remaining tests. */
__attribute__((format (printf, 1, 2)))
void test_skip_all(const char *format, ...);

/* Print a diagnostic message to stdout. */
__attribute__((format (printf, 1, 2)))
void test_msg(const char *format, ...);

/*
 * Test checks are built around test_assert(). checks return 1 on
 * success, 0 on failure. If any check fails then the test will
 * fail. To create a custom check define a function that wraps
 * test_assert() and a macro to wrap that function. For example:
 *
 *  static int check_oid_loc(const char *loc, const char *check,
 *			     struct object_id *a, struct object_id *b)
 *  {
 *	    int res = test_assert(loc, check, oideq(a, b));
 *
 *	    if (res) {
 *		    test_msg("   left: %s", oid_to_hex(a);
 *		    test_msg("  right: %s", oid_to_hex(a);
 *
 *	    }
 *	    return res;
 *  }
 *
 *  #define check_oid(a, b) \
 *	    check_oid_loc(TEST_LOCATION(), "oideq("#a", "#b")", a, b)
 */
int test_assert(const char *location, const char *check, int ok);

/* Helper macro to pass the location to checks */
#define TEST_LOCATION() TEST__MAKE_LOCATION(__LINE__)

/* Check a boolean condition. */
#define check(x)				\
	check_bool_loc(TEST_LOCATION(), #x, x)
int check_bool_loc(const char *loc, const char *check, int ok);

/*
 * Compare two integers. Prints a message with the two values if the
 * comparison fails. NB this is not thread safe.
 */
#define check_int(a, op, b)						\
	(test__tmp[0].i = (a), test__tmp[1].i = (b),			\
	 check_int_loc(TEST_LOCATION(), #a" "#op" "#b,			\
		       test__tmp[0].i op test__tmp[1].i, a, b))
int check_int_loc(const char *loc, const char *check, int ok,
		  intmax_t a, intmax_t b);

/*
 * Compare two unsigned integers. Prints a message with the two values
 * if the comparison fails. NB this is not thread safe.
 */
#define check_uint(a, op, b)						\
	(test__tmp[0].u = (a), test__tmp[1].u = (b),			\
	 check_uint_loc(TEST_LOCATION(), #a" "#op" "#b,			\
			test__tmp[0].u op test__tmp[1].u, a, b))
int check_uint_loc(const char *loc, const char *check, int ok,
		   uintmax_t a, uintmax_t b);

/*
 * Compare two chars. Prints a message with the two values if the
 * comparison fails. NB this is not thread safe.
 */
#define check_char(a, op, b)						\
	(test__tmp[0].c = (a), test__tmp[1].c = (b),			\
	 check_char_loc(TEST_LOCATION(), #a" "#op" "#b,			\
			test__tmp[0].c op test__tmp[1].c, a, b))
int check_char_loc(const char *loc, const char *check, int ok,
		   char a, char b);

/* Check whether two strings are equal. */
#define check_str(a, b)							\
	check_str_loc(TEST_LOCATION(), "!strcmp("#a", "#b")", a, b)
int check_str_loc(const char *loc, const char *check,
		  const char *a, const char *b);

/*
 * Wrap a check that is known to fail. If the check succeeds then the
 * test will fail. Returns 1 if the check fails, 0 if it
 * succeeds. For example:
 *
 *  TEST_TODO(check(0));
 */
#define TEST_TODO(check) \
	(test__todo_begin(), test__todo_end(TEST_LOCATION(), #check, check))

/* Private helpers */

#define TEST__STR(x) #x
#define TEST__MAKE_LOCATION(line) __FILE__ ":" TEST__STR(line)

union test__tmp {
	intmax_t i;
	uintmax_t u;
	char c;
};

extern union test__tmp test__tmp[2];

int test__run_begin(void);
__attribute__((format (printf, 3, 4)))
int test__run_end(int, const char *, const char *, ...);
void test__todo_begin(void);
int test__todo_end(const char *, const char *, int);

#endif /* TEST_LIB_H */
