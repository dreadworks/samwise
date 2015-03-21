/*  =========================================================================

    sam_selftest - run all self tests

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief run all self tests
   @file sam_selftest.c

   This file is used to offer an entry point to run all self
   tests. While there must not always appear a direct result in the
   terminal, it is very convenient to run all functions at least
   once. This checks if any inner assert signals SIGABRT and external
   programs can be used to check for memory leaks/segfaults and do
   heap profiling.

*/


#include "../include/sam_prelude.h"
#include "../include/samd.h"
#include "../include/sam_selftest.h"


typedef void *(*test_fn_t) ();
test_fn_t suites [] = {
    sam_log_test,
    sam_gen_test,
    sam_msg_test,
    sam_cfg_test,
    sam_buf_test,
    sam_be_rmq_test,
    sam_test
};



void
sam_selftest_introduce (const char *name)
{
    char buf [] = "|---| ----------------------------------------------------------------------------------|\n";
    char *tmpl = " %s |";

    snprintf (buf + 6, 50, tmpl, name);
    buf[strlen (buf)] = '-';

    printf ("%s", buf);
}


//  --------------------------------------------------------------------------
/// Print all necessary information for user invoking this
/// program. The function calls exit () with the provided return code.
static void
rtfm (int rc)
{
    printf ("sam selftest\n");
    printf ("usage: sam_selftest [-h] [--only SAM_MODULE [TESTCASE]]\n");
    printf ("options:\n\n");
    printf ("  -h: Print this message and exit\n\n");
    printf ("  --only SAM_MODULE [TESTCASE]:\n");
    printf ("      selective running of tests, where SAM_MODULE is one of\n");
    printf ("      { sam_gen, sam_log, sam_msg, sam_cfg, sam_be_rmq, sam }\n");
    printf ("      and TESTCASE one of the test cases defined in SAM_MODULE");
    printf ("\n\n");
    printf ("You can selectively run tests by setting the CK_RUN_SUITE and\n");
    printf ("CK_RUN_CASE environment variables instead of providing --only");
    printf ("\n");
    exit (rc);
}


//  --------------------------------------------------------------------------
/// Parses input parameters and sets environmental variables. Exits
/// the program for invalid arguments.
static char *
parse_args (int arg_c, char **arg_v)
{
    arg_c -= 1;
    arg_v += 1;

    if (arg_c) {

        // -h
        if (!strcmp ("-h", arg_v[0])) {
            rtfm (0);
        }

        // --only SAM_MODULE
        if (arg_c < 2 || 4 < arg_c || strcmp ("--only", arg_v[0])) {
            rtfm (2);
        }

        int buf_s = 128;

        // must be heap memory because putenv won't copy
        // any memory but instead point to the string
        char *buf = malloc (buf_s);
        snprintf (buf, buf_s / 2, "CK_RUN_SUITE=%s", arg_v[1]);
        int rc = putenv (buf);
        assert (!rc);

        // add optional TESTCASE if there's one provided
        if (arg_c == 3) {
            char *buf_ptr = buf + strlen (buf) + 1;
            snprintf (buf_ptr, buf_s / 2, "CK_RUN_CASE=%s", arg_v[2]);
            rc = putenv (buf_ptr);
            assert (!rc);
        }

        return buf;
    }

    return NULL;
}


//  --------------------------------------------------------------------------
/// Run tests.
int main (int arg_c, char **arg_v)
{
    char *env = parse_args (arg_c, arg_v);

    int suite_c = 0;
    int suites_size = sizeof (suites) / sizeof (test_fn_t);

    Suite *s = suites[suite_c] ();
    SRunner *sr = srunner_create (s);
    suite_c += 1;

    for (; suite_c < suites_size; suite_c++) {
        s = suites[suite_c] ();
        srunner_add_suite (sr, s);
    }

    srunner_set_fork_status (sr, CK_NOFORK);
    srunner_run_all (sr, CK_NORMAL);

    int failed = srunner_ntests_failed (sr);
    srunner_free (sr);

    if (env != NULL) {
        free (env);
    }

    return (failed) ? 2 : 0;
}
