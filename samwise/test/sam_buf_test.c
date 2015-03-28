/*  =========================================================================

    sam_buf_test - Test the buffer

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/


#include "../include/sam_prelude.h"


zsock_t
    *backend_push,  // messages arriving from backends
    *frontend_pull; // messages distributed by libsam

sam_cfg_t *cfg;
sam_buf_t *buf;


//  --------------------------------------------------------------------------
/// Create sockets and a sam_buf instance.
static void
setup ()
{
    char *endpoint = "inproc://test-buf_be";
    zsock_t *backend_pull = zsock_new_pull (endpoint);
    backend_push = zsock_new_push (endpoint);

    endpoint = "inproc://test-buf_fe";
    zsock_t *frontend_push = zsock_new_push (endpoint);
    frontend_pull = zsock_new_pull (endpoint);

    cfg = sam_cfg_new ("cfg/test/buf.cfg");
    buf = sam_buf_new (cfg, &backend_pull, &frontend_push);

    if (!buf) {
        ck_abort_msg ("buf instance was not created");
    }

    if (backend_pull) {
        ck_abort_msg ("backend pull socket still reachable");
    }

    if (frontend_push) {
        ck_abort_msg ("frontend push still reachable");
    }

}


//  --------------------------------------------------------------------------
/// Tear down test fixture.
static void
destroy ()
{
    sam_buf_destroy (&buf);
    sam_cfg_destroy (&cfg);
    zsock_destroy (&backend_push);
    zsock_destroy (&frontend_pull);
}


//  --------------------------------------------------------------------------
/// Retrieves the retry interval.
static uint64_t
get_interval ()
{
    uint64_t interval;
    int rc = sam_cfg_buf_retry_interval (cfg, &interval);
    ck_assert_int_eq (rc, 0);
    return interval;
}


//  --------------------------------------------------------------------------
/// Emulates a backend sending an acknowledgement.
void
send_ack (uint64_t be_id, int key)
{
    zframe_t *id_frame = zframe_new (&be_id, sizeof (be_id));

    zsock_send (
        backend_push, "fi",
        id_frame, key);

    zframe_destroy (&id_frame);

}


//  --------------------------------------------------------------------------
/// Finish composing the message and hand it over to sam_buf.
static int
save (zmsg_t *zmsg, const char *payload)
{
    zmsg_addstr (zmsg, "test-x");
    zmsg_addstr (zmsg, "");
    zmsg_addstr (zmsg, payload);

    sam_msg_t *msg = sam_msg_new (&zmsg);
    return sam_buf_save (buf, msg);
}


//  --------------------------------------------------------------------------
/// Eat all re-sent messages up.
static void
eat ()
{
    // sleep at least one resend interval
    uint64_t interval = get_interval ();

    zclock_sleep (interval + 100);
    zpoller_t *poller = zpoller_new (frontend_pull, NULL);
    while (zpoller_wait (poller, 0) != NULL) {

        int msg_id;
        zframe_t *be_acks;
        sam_msg_t *msg;

        zsock_recv (frontend_pull, "ifp", &msg_id, &be_acks, &msg);
        zframe_destroy (&be_acks);
        sam_msg_destroy (&msg);

    }

    zpoller_destroy (&poller);
}


//  --------------------------------------------------------------------------
/// Save a sam_msg_t to sam_buf with round robin distribution type.
static int
save_roundrobin (const char *payload)
{
    zmsg_t *zmsg = zmsg_new ();
    zmsg_addstr (zmsg, "round robin");
    return save (zmsg, payload);
}


//  --------------------------------------------------------------------------
/// Save a sam_msg_t to sam_buf with redundant distribution type.
static int
save_redundant (const char *n, const char *payload)
{
    zmsg_t *zmsg = zmsg_new ();
    zmsg_addstr (zmsg, "redundant");
    zmsg_addstr (zmsg, n);
    return save (zmsg, payload);
}


static void
consume_resends ()
{
    zpoller_t *poller = zpoller_new (frontend_pull, NULL);
    zsock_t *sock = zpoller_wait (poller, 10);

    while (sock != NULL) {

        int key;
        zframe_t *id_frame;
        sam_msg_t *msg;

        zsock_recv (frontend_pull, "ifp", &key, &id_frame, &msg);

        ck_assert (key);
        ck_assert (*(uint64_t *) zframe_data (id_frame) == 0);
        ck_assert_int_eq (sam_msg_size (msg), 4);

        zframe_destroy (&id_frame);
        sam_msg_destroy (&msg);
        ck_assert (msg == NULL); // refc assertion

        sock = zpoller_wait (poller, 10);
    }

    zpoller_destroy (&poller);
}



//  --------------------------------------------------------------------------
/// Test saving round robin acknowledged messages.
/// Flow: save -> ack1
START_TEST(test_buf_save_roundrobin)
{
    sam_selftest_introduce ("test_buf_save_roundrobin");

    save_roundrobin ("round robin");
    zclock_sleep (10);

    send_ack (1, 1);
    zclock_sleep (10);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test saving round robin acknowledged messages.
/// Flow: ack1 -> save
START_TEST(test_buf_save_roundrobin_race)
{
    sam_selftest_introduce ("test_buf_save_roundrobin_race");
    send_ack (1, 1);
    zclock_sleep (10);

    save_roundrobin ("round robin race");
    zclock_sleep (10);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test saving redundantly acknowledged messages. n=3, backends: 1, 2, 4.
/// Flow: save -> ack 1 -> ack 2 -> ack 4.
START_TEST(test_buf_save_redundant)
{
    sam_selftest_introduce ("test_buf_save_redundant");

    // save
    int key = save_redundant ("3", "redundant");
    zclock_sleep (10);

    // ack1, ack2, ack4
    int be_power;
    for (be_power = 0; be_power < 3; be_power++) {
        zclock_sleep (10);
        uint64_t be_id = 1 << be_power;
        send_ack (be_id, key);
    }

    zclock_sleep (10);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test saving redundantly acknowledged messages. n=2, backends: 1, 2
/// Flow: ack1 -> save -> ack2
START_TEST(test_buf_save_redundant_race)
{
    sam_selftest_introduce ("test_buf_save_redundant_race");

    // ack1
    send_ack (1, 1);
    zclock_sleep (10);

    // save
    int key = save_redundant ("2", "redundant race");
    ck_assert_int_eq (key, 1);
    zclock_sleep (10);

    // ack 2
    send_ack (2, 1);
    zclock_sleep (10);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test saving redundantly acknowledged messages. n=2, backends: 1, 2
/// Flow: save -> ack1 -> ack1 -> ack2
START_TEST(test_buf_save_redundant_idempotency)
{
    sam_selftest_introduce ("test_buf_save_redundant_idempotency");

    // save
    save_redundant ("2", "redundant idempotency");
    zclock_sleep (10);

    // ack1
    send_ack (1, 1);
    zclock_sleep (10);

    // ack1 again
    send_ack (1, 1);
    zclock_sleep (10);

    // ack 2
    send_ack (2, 1);
    zclock_sleep (10);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test saving redundantly acknowledged messages. n=2, backends: 1, 2
/// Flow: ack1 -> ack1 -> save -> ack2
START_TEST(test_buf_save_redundant_race_idempotency)
{
    sam_selftest_introduce ("test_buf_save_redundant_race_idempotency");

    // ack1
    send_ack (1, 1);
    zclock_sleep (10);

    // ack1 again
    send_ack (1, 1);
    zclock_sleep (10);

    // save
    save_redundant ("2", "redundant race idempotency");
    zclock_sleep (10);

    // ack 2
    send_ack (2, 1);
    zclock_sleep (10);
}
END_TEST


//  --------------------------------------------------------------------------
/// Lets the buffer resend a message multiple times, before an
/// acknowledgment arrives for the very first
START_TEST(test_buf_resend)
{
    sam_selftest_introduce ("test_buf_resend");

    // save
    save_roundrobin ("resend late ack");
    zclock_sleep (10);

    zclock_sleep (get_interval () * 3);

    // late ack
    send_ack (1, 1);

    eat ();
}
END_TEST



//  --------------------------------------------------------------------------
/// Checks if a message gets resent multiple times and if multiple
/// messages are resent.
START_TEST(test_buf_resend_multiple)
{
    sam_selftest_introduce ("test_buf_resend");
    int ref_key1 = save_roundrobin ("resend 1");
    int ref_key2 = save_roundrobin ("resend 2");

    zclock_sleep (get_interval () * 3);

    send_ack (1, ref_key1);
    send_ack (1, ref_key2);

    eat ();
}
END_TEST


//  --------------------------------------------------------------------------
/// Re-initializes the buffer before acknowledging the stored message.
START_TEST(test_buf_restore)
{
    sam_selftest_introduce ("test_buf_restore");
    int ref_key = save_roundrobin ("restore");

    destroy ();
    setup ();

    send_ack (1, ref_key);
    eat ();
}
END_TEST


void *
sam_buf_test ()
{
    Suite *s = suite_create ("sam_buf");

    TCase *tc = tcase_create ("save round robin");
    tcase_add_checked_fixture (tc, setup, destroy);
    tcase_add_test (tc, test_buf_save_roundrobin);
    tcase_add_test (tc, test_buf_save_roundrobin_race);
    suite_add_tcase (s, tc);

    tc = tcase_create ("save redundant");
    tcase_add_checked_fixture (tc, setup, destroy);
    tcase_add_test (tc, test_buf_save_redundant);
    tcase_add_test (tc, test_buf_save_redundant_race);
    tcase_add_test (tc, test_buf_save_redundant_idempotency);
    tcase_add_test (tc, test_buf_save_redundant_race_idempotency);
    suite_add_tcase (s, tc);

    tc = tcase_create ("resending");
    tcase_add_checked_fixture (tc, setup, destroy);
    tcase_add_test (tc, test_buf_resend);
    tcase_add_test (tc, test_buf_resend_multiple);
    suite_add_tcase (s, tc);

    tc = tcase_create ("restore state");
    tcase_add_checked_fixture (tc, setup, destroy);
    tcase_add_test (tc, test_buf_restore);
    suite_add_tcase (s, tc);

    return s;
}
