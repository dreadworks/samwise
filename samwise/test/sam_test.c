/*  =========================================================================

    sam_test - Test the libsam interface

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/

#include "../include/sam_prelude.h"


sam_t *sam;
sam_cfg_t *cfg;
size_t char_s = sizeof (char *);


//  --------------------------------------------------------------------------
/// Create a sam instance
static void
setup_rmq ()
{
    sam = sam_new (SAM_BE_RMQ);
    if (!sam) {
        ck_abort_msg ("could not create sam instance");
    }

    cfg = sam_cfg_new ("cfg/test/sam_three_brokers.cfg");
    int rc = sam_init (sam, cfg);
    ck_assert_int_eq (rc, 0);
}


//  --------------------------------------------------------------------------
/// Destroy a sam instance
static void
destroy ()
{
    sam_destroy (&sam);
    if (sam) {
        ck_abort_msg ("sam instance still reachable");
    }

    sam_cfg_destroy (&cfg);
}


//  --------------------------------------------------------------------------
/// Creates a zmsg containing a variable number of strings as frames
static sam_msg_t*
test_create_msg (uint arg_c, char **arg_v)
{
    zmsg_t *zmsg = zmsg_new ();
    uint frame_c = 0;

    for (; frame_c < arg_c; frame_c++) {
        char *str = arg_v[frame_c];
        zframe_t *frame = zframe_new (str, strlen (str));
        zmsg_append (zmsg, &frame);
    }

    sam_msg_t *msg = sam_msg_new (&zmsg);
    return msg;
}


//  --------------------------------------------------------------------------
/// Asserts that sam_eval will return with an error.
static void
test_assert_error (sam_t *sam, sam_msg_t *msg)
{
    sam_ret_t *ret = sam_eval (sam, msg);
    ck_assert_int_eq (ret->rc, -1);
    sam_log_tracef ("got error: %s", ret->msg);
    free (ret);
}


//  --------------------------------------------------------------------------
/// Test publishing a message to a RabbitMQ broker.
START_TEST(test_sam_rmq_publish_roundrobin)
{
    sam_selftest_introduce ("test_sam_rmq_publish_roundrobin");

    char *pub_msg [] = {
        "publish",        // action
        "round robin",    // distribution type
        "amq.direct",     // exchange
        "",               // routing key
        "hi!"             // payload
    };

    sam_msg_t *msg = test_create_msg (sizeof (pub_msg) / char_s, pub_msg);
    sam_ret_t *ret = sam_eval (sam, msg);

    // let the parts cope before tearing it down
    zclock_sleep (50);

    ck_assert_int_eq (ret->rc, 0);
    free (ret);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test publishing a message to two RabbitMQ brokers redundantly.
START_TEST(test_sam_rmq_publish_redundant)
{
    sam_selftest_introduce ("test_sam_rmq_redundant");

    char *pub_msg [] = {
        "publish",        // action
        "redundant",      // distribution type
        "2",              // distribution count
        "amq.direct",     // exchange
        "",               // routing key
        "hi!"             // payload
    };

    sam_msg_t *msg = test_create_msg (sizeof (pub_msg) / char_s, pub_msg);
    sam_ret_t *ret = sam_eval (sam, msg);

    // let the parts cope before tearing it down
    zclock_sleep (50);

    ck_assert_int_eq (ret->rc, 0);
    free (ret);
}
END_TEST



//  --------------------------------------------------------------------------
/// Test declaring an exchange on a RabbitMQ broker.
START_TEST(test_sam_rmq_xdecl)
{
    sam_selftest_introduce ("test_sam_rmq_xdecl");

    char *exch_decl_msg [] = {
        "rpc",
        "",                 // broker name
        "exchange.declare", // action
        "test-x",           // exchange name
        "direct"            // type
    };

    sam_msg_t *msg = test_create_msg (
        sizeof (exch_decl_msg) / char_s, exch_decl_msg);

    sam_ret_t *ret = sam_eval (sam, msg);

    ck_assert_int_eq (ret->rc, 0);
    free (ret);
}
END_TEST

//  --------------------------------------------------------------------------
/// Test deleting an exchange on a RabbitMQ broker
START_TEST(test_sam_rmq_xdel)
{
    sam_selftest_introduce ("test_sam_rmq_xdel");

    char *exch_del_msg [] = {
        "rpc",
        "",                // broker name
        "exchange.delete", // action
        "test-x"           // exchange name
    };

    sam_msg_t *msg = test_create_msg (
        sizeof (exch_del_msg) / char_s, exch_del_msg);

    sam_ret_t *ret = sam_eval (sam, msg);

    ck_assert_int_eq (ret->rc, 0);
    free (ret);
}
END_TEST


//  --------------------------------------------------------------------------
/// Send an empty message.
START_TEST(test_sam_rmq_prot_error_empty)
{
    sam_selftest_introduce ("test_sam_rmq_prot_error_empty");

    zmsg_t *zmsg = zmsg_new ();
    sam_msg_t *msg = sam_msg_new (&zmsg);
    test_assert_error (sam, msg);
}
END_TEST

//  --------------------------------------------------------------------------
/// Send a request with an unknown method frame.
START_TEST(test_sam_rmq_prot_error_unknown)
{
    sam_selftest_introduce ("test_sam_rmq_prot_error_unknown");

    char *a [] = {
        "consume", "amq.direct", ""
    };

    sam_msg_t *msg = test_create_msg (sizeof (a) / char_s, a);
    test_assert_error (sam, msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Send a request with a missing distribution type.
START_TEST(test_sam_rmq_prot_error_missing_type)
{
    sam_selftest_introduce ("test_sam_rmq_prot_error_missing_type");

    char *a [] = {
        "publish", "amq.direct", "", "hi!"
    };

    sam_msg_t *msg = test_create_msg (sizeof (a) / char_s, a);
    test_assert_error (sam, msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Send a request with a missing distribution count for "redundant".
START_TEST(test_sam_rmq_prot_error_missing_dcount)
{
    sam_selftest_introduce ("test_sam_rmq_prot_error_missing_dcount");

    char *a [] = {
        "publish", "redundant", "amq.direct", "", "hi!"
    };

    sam_msg_t *msg = test_create_msg (sizeof (a) / char_s, a);
    test_assert_error (sam, msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Send a wrongly formatted publishing request.
START_TEST(test_sam_rmq_prot_error_publish)
{
    sam_selftest_introduce ("test_sam_rmq_prot_error_publish");

    char *a [] = {
        "publish", "round robin", "amq.direct"
    };

    sam_msg_t *msg = test_create_msg (sizeof (a) / char_s, a);
    test_assert_error (sam, msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Send a wrong exchange.declare with missing options.
START_TEST(test_sam_rmq_prot_error_xdecl1)
{
    sam_selftest_introduce ("test_sam_rmq_prot_error_xdecl1");

    char *a [] = {
        "rpc", "exchange.declare"
    };

    sam_msg_t *msg = test_create_msg (sizeof (a) / char_s, a);
    test_assert_error (sam, msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Send a wrong exchange declare with empty options.
START_TEST(test_sam_rmq_prot_error_xdecl2)
{
    sam_selftest_introduce ("test_sam_rmq_prot_error_xdecl2");

    char *a [] = {
        "rpc", "exchange.declare", "", ""
    };

    sam_msg_t *msg = test_create_msg (sizeof (a) / char_s, a);
    test_assert_error (sam, msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Send a wrong exchange declare with wrong argument count.
START_TEST(test_sam_rmq_prot_error_xdecl3)
{
    sam_selftest_introduce ("test_sam_rmq_prot_error_xdecl3");

    char *a [] = {
        "rpc", "exchange.declare", "foo"
    };

    sam_msg_t *msg = test_create_msg (sizeof (a) / char_s, a);
    test_assert_error (sam, msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Send a wrong exchange delete with missing option frames.
START_TEST(test_sam_rmq_prot_error_xdel1)
{
    sam_selftest_introduce ("test_sam_rmq_prot_error_xdel1");

    // wrong exchange.delete
    char *a [] = {
        "rpc", "exchange.delete"
    };

    sam_msg_t *msg = test_create_msg (sizeof (a) / char_s, a);
    test_assert_error (sam, msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Send a wrong echange delete with empty option frames.
START_TEST(test_sam_rmq_prot_error_xdel2)
{
    sam_selftest_introduce ("test_sam_rmq_prot_error_xdel2");

    char *a [] = {
        "rpc", "exchange.delete", ""
    };

    sam_msg_t *msg = test_create_msg (sizeof (a) / char_s, a);
    test_assert_error (sam, msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Self test this class.
void *
sam_test ()
{
    Suite *s = suite_create ("sam");

    TCase *tc = tcase_create ("rmq_publish");
    tcase_add_unchecked_fixture (tc, setup_rmq, destroy);
    tcase_add_test (tc, test_sam_rmq_publish_roundrobin);
    tcase_add_test (tc, test_sam_rmq_publish_redundant);
    suite_add_tcase (s, tc);

    tc = tcase_create ("rmq_rpc");
    tcase_add_unchecked_fixture (tc, setup_rmq, destroy);
    tcase_add_test (tc, test_sam_rmq_xdecl);
    tcase_add_test (tc, test_sam_rmq_xdel);
    suite_add_tcase (s, tc);

    tc = tcase_create("rmq_proterror");
    tcase_add_unchecked_fixture (tc, setup_rmq, destroy);
    tcase_add_test(tc, test_sam_rmq_prot_error_empty);
    tcase_add_test(tc, test_sam_rmq_prot_error_unknown);
    tcase_add_test(tc, test_sam_rmq_prot_error_missing_type);
    tcase_add_test(tc, test_sam_rmq_prot_error_missing_dcount);
    tcase_add_test(tc, test_sam_rmq_prot_error_publish);
    tcase_add_test(tc, test_sam_rmq_prot_error_xdecl1);
    tcase_add_test(tc, test_sam_rmq_prot_error_xdecl2);
    tcase_add_test(tc, test_sam_rmq_prot_error_xdecl3);
    tcase_add_test(tc, test_sam_rmq_prot_error_xdel1);
    tcase_add_test(tc, test_sam_rmq_prot_error_xdel2);
    suite_add_tcase (s, tc);

    return s;
}
