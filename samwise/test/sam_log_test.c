/*  =========================================================================

    sam_log_test - Test sam_log

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/

#include "../include/sam_prelude.h"



START_TEST(test_sam_log_trace)
{
    sam_selftest_introduce ("test_sam_log_trace");

    sam_log_trace ("trace");
    ck_assert (true);
}
END_TEST


START_TEST(test_sam_log_info)
{
    sam_selftest_introduce ("test_sam_log_info");

    sam_log_info ("info");
    ck_assert (true);
}
END_TEST


START_TEST(test_sam_log_error)
{
    sam_selftest_introduce ("test_sam_log_error");

    sam_log_error ("error");
    ck_assert (true);
}
END_TEST


START_TEST(test_sam_log_tracef)
{
    sam_selftest_introduce ("test_sam_log_tracef");

    sam_log_tracef ("%s formatted", "trace");
    ck_assert (true);
}
END_TEST


START_TEST(test_sam_log_infof)
{
    sam_selftest_introduce ("test_sam_log_infof");

    sam_log_infof ("%s formatted", "info");
    ck_assert (true);
}
END_TEST


START_TEST(test_sam_log_errorf)
{
    sam_selftest_introduce ("test_sam_log_errorf");

    sam_log_errorf ("%s formatted", "error");
    ck_assert (true);
}
END_TEST


//  --------------------------------------------------------------------------
/// Self test this class.
void *
sam_log_test ()
{
    Suite *s = suite_create ("sam_log");

    TCase *tc = tcase_create("log");
    tcase_add_test (tc, test_sam_log_trace);
    tcase_add_test (tc, test_sam_log_info);
    tcase_add_test (tc, test_sam_log_error);
    suite_add_tcase (s, tc);

    tc = tcase_create("logf");
    tcase_add_test (tc, test_sam_log_tracef);
    tcase_add_test (tc, test_sam_log_infof);
    tcase_add_test (tc, test_sam_log_errorf);
    suite_add_tcase (s, tc);

    return s;
}
