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


START_TEST(test_cfg_endpoint)
{
    char *endpoint;

    int rc = sam_cfg_endpoint (cfg, &endpoint);
    ck_assert_int_eq (rc, 0);

    zsock_t *sock = zsock_new_rep (endpoint);
    if (!sock) {
        ck_abort_msg ("endpoint is not valid");
    }
    zsock_destroy (&sock);
}
END_TEST


START_TEST(test_cfg_be_type)
{
    sam_be_t be_type;
    int rc = sam_cfg_be_type (cfg, &be_type);

    ck_assert_int_eq (rc, 0);
    ck_assert (be_type == SAM_BE_RMQ);

}
END_TEST


START_TEST(test_cfg_backends_rmq)
{
    int backend_count;
    sam_be_rmq_opts_t *opts;
    sam_be_t be_type;

    sam_cfg_be_type (cfg, &be_type);
    int rc = sam_cfg_backends (cfg, be_type, &backend_count, (void **) &opts);

    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (backend_count, 2);

    ck_assert_str_eq (opts->host, "localhost");
    ck_assert_int_eq (opts->port, 5672);
    ck_assert_str_eq (opts->user, "guest");
    ck_assert_str_eq (opts->pass, "guest");
    ck_assert_int_eq (opts->heartbeat, 3);

    free (opts);
}
END_TEST


//  --------------------------------------------------------------------------
/// Self test this class.
void *
sam_cfg_test ()
{
    Suite *s = suite_create ("sam_cfg");

    TCase *tc = tcase_create("config");
    tcase_add_unchecked_fixture (tc, setup, destroy);
    tcase_add_test (tc, test_cfg_endpoint);
    tcase_add_test (tc, test_cfg_be_type);
    tcase_add_test (tc, test_cfg_backends_rmq);
    suite_add_tcase (s, tc);

    return s;

}
