/*  =========================================================================

    samd_test - Test samd

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/


#include "../include/sam_prelude.h"
#include "../include/samd.h"


zactor_t *actor;
zsock_t *req;


//  --------------------------------------------------------------------------
/// Thread for the tests.
static void
test_actor (zsock_t *pipe, void *args UU)
{
    // samd_t *samd = samd_new (SAM_PUBLIC_ENDPOINT);
    // zsock_signal (pipe, 0);
    // samd_start (samd);
    // samd_destroy (&samd);
}


//  --------------------------------------------------------------------------
/// Create a samd instance.
static void
setup ()
{
    // actor = zactor_new (test_actor, NULL);
    // req = zsock_new_req (SAM_PUBLIC_ENDPOINT);
}


//  --------------------------------------------------------------------------
/// Destroy the samd instance.
static void
destroy ()
{
    zactor_destroy (&actor);
    zsock_destroy (&req);
}


//  --------------------------------------------------------------------------
/// Checks that the answer is contains an error.
static void
assert_error (zmsg_t **msg)
{
    ck_assert_int_eq (zmsg_size (*msg), 2);
    ck_assert_int_eq (zmsg_popint (*msg), -1);

    char *error_msg = zmsg_popstr (*msg);
    sam_log_tracef ("got error: %s", error_msg);

    free (error_msg);
    zmsg_destroy (msg);
}


//  --------------------------------------------------------------------------
/// Send a ping.
START_TEST(test_samd_ping)
{
    zsock_send (req, "is", SAM_PROTOCOL_VERSION, "ping");
    zmsg_t *msg = zmsg_recv (req);

    ck_assert_int_eq (zmsg_size (msg), 1);
    ck_assert_int_eq (zmsg_popint (msg), 0);

    zmsg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Send a wrong protocol version.
START_TEST(test_samd_proterr_version)
{
    zsock_send (req, "is", 0, "ping");
    zmsg_t *msg = zmsg_recv (req);
    assert_error (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Send a malformed request.
START_TEST(test_samd_proterr_malformed)
{
    zsock_send (req, "z");
    zmsg_t *msg = zmsg_recv (req);
    assert_error (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Self test this class.
void *
samd_test ()
{
/*

  TODO:

    This might not be the best place to check for all these things.
    Maybe it is better to just let the clients (rb, c) test the public
    endpoint of samd. Some integration test strategy must be found.

    It is currently not possible to shut down correctly.

*/
    Suite *s = suite_create ("samd");
    TCase *tc = tcase_create("protocol");

/*
    tcase_add_unchecked_fixture (tc, setup, destroy);
    tcase_add_test (tc, test_samd_ping);
    tcase_add_test (tc, test_samd_proterr_version);
    tcase_add_test (tc, test_samd_proterr_malformed);
*/

    suite_add_tcase (s, tc);
    return s;

}
