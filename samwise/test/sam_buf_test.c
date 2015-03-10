/*  =========================================================================

    sam_buf_test - Test the buffer

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/


#include "../include/sam_prelude.h"


START_TEST(test_buf_lifecyle)
{
    char *fname;

    sam_cfg_t *cfg = sam_cfg_new ("cfg/base.cfg");
    sam_cfg_buf_file (cfg, &fname);

    sam_buf_t *buf = sam_buf_new (fname);
    if (!buf) {
        ck_abort_msg ("buf was not created");
    }

    sam_buf_destroy (&buf);
    if (buf) {
        ck_abort_msg ("buf is still reachable");
    }

    sam_cfg_destroy (&cfg);
}
END_TEST


void *
sam_buf_test ()
{
    Suite *s = suite_create ("sam_buf");

    TCase *tc = tcase_create("config");
    tcase_add_test (tc, test_buf_lifecyle);
    suite_add_tcase (s, tc);

    return s;
}
