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


typedef void *(*test_fn_t) ();
test_fn_t suites [] = {
    sam_log_test,
    sam_gen_test,
    sam_msg_test,
    sam_be_rmq_test
};
/*     sam_test */
/*     return samd_test; */

//  --------------------------------------------------------------------------
/// Print all necessary information for user invoking this
/// program. The function calls exit () with the provided return code.
static void
rtfm (int rc)
{
    printf ("sam selftest\n");
    printf ("usage: sam_selftest [-h]\n");
    printf ("options:\n");
    printf ("  -h: Print this message and exit\n");
    printf ("\n");
    printf ("You can selectively run tests by setting the CK_RUN_SUITE and\n");
    printf ("CK_RUN_CASE environment variables.");
    printf ("\n");
    exit (rc);
}


//  --------------------------------------------------------------------------
/// Run tests.
int main (int arg_c, char **arg_v)
{
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

    return (failed) ? 2 : 0;
}
