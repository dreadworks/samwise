/*  =========================================================================

    sam_cfg_test - Test config loader

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/


#include "../include/sam_prelude.h"


sam_cfg_t *cfg;


static void
setup ()
{
    cfg = sam_cfg_new ("cfg/base.cfg");
    if (!cfg) {
        ck_abort_msg ("could not create cfg");
    }
}


static void
destroy ()
{
    sam_cfg_destroy (&cfg);
    if (cfg) {
        ck_abort_msg ("cfg still reachable");
    }
}


//  --------------------------------------------------------------------------
/// Self test this class.
void *
sam_cfg_test ()
{
    Suite *s = suite_create ("sam_cfg");

    TCase *tc = tcase_create("config");
    tcase_add_unchecked_fixture (tc, setup, destroy);
    // tcase_add_test (tc, test_samd_proterr_malformed);
    suite_add_tcase (s, tc);

    return s;

}
