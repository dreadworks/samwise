/*  =========================================================================

    sam_be_rmq_test - Test the RabbitMQ messaging backend

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/

#include "../include/sam_prelude.h"


// rabbitmq instance
char *be_name = "test";
uint64_t be_id = 1;
sam_be_rmq_t *rabbit;

// feedback channel for async. communication
zsock_t *pll;
char *pll_endpoint = "inproc://test-pll";
sam_backend_t *backend;


//  --------------------------------------------------------------------------
/// Setups the connection to a RabbitMQ broker.
static void
setup_connection ()
{
    rabbit = sam_be_rmq_new (be_name, be_id);
    if (!rabbit) {
        ck_abort_msg ("could not create be_rmq instance");
    }

    sam_be_rmq_opts_t opts = {
        .host = "localhost",
        .port = 5672,
        .user = "guest",
        .pass = "guest",
        .heartbeat = 1
    };

    int rc = sam_be_rmq_connect (rabbit, &opts);
    ck_assert_int_eq (rc, 0);
}


//  --------------------------------------------------------------------------
/// Destroys a rmq instance.
static void
destroy_connection ()
{
    sam_be_rmq_destroy (&rabbit);
}


//  --------------------------------------------------------------------------
/// Setups a generic rmq message backend, starts the connection beforehand.
static void
setup_backend ()
{
    setup_connection ();
    pll = zsock_new_pull (pll_endpoint);

    if (!pll) {
        ck_abort_msg ("could not create PULL socket");
    }

    backend = sam_be_rmq_start (&rabbit, pll_endpoint);
    if (!backend) {
        ck_abort_msg ("could not create backend");
    }

    if (rabbit) {
        ck_abort_msg ("rmq instance still reachable");
    }
}


//  --------------------------------------------------------------------------
/// Destroys a generic rmq message backend, closes the connection afterwards.
static void
destroy_backend ()
{
    // reclaim ownership
    rabbit = sam_be_rmq_stop (&backend);
    if (!rabbit) {
        ck_abort_msg ("could not reclaim the rmq instnace");
    }

    if (backend) {
        ck_abort_msg ("backend still reachable");
    }

    zsock_destroy (&pll);
    destroy_connection ();
}


//  --------------------------------------------------------------------------
/// Test synchronous exchange declaration.
START_TEST(test_be_rmq_sync_xdecl)
{
    sam_selftest_introduce ("test_be_rmq_sync_xdecl");

    int rc = sam_be_rmq_exchange_declare (rabbit, "x-test", "direct");
    ck_assert_int_eq (rc, 0);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test synchronous exchange deletion.
START_TEST(test_be_rmq_sync_xdel)
{
    sam_selftest_introduce ("test_be_rmq_sync_xdel");

    int rc = sam_be_rmq_exchange_delete (rabbit, "x-test");
    ck_assert_int_eq (rc, 0);

}
END_TEST


//  --------------------------------------------------------------------------
/// Test synchronous publishing.
START_TEST(test_be_rmq_sync_publish)
{
    sam_selftest_introduce ("test_be_rmq_sync_publish");

    zframe_t *payload = zframe_new ("hi!", 3);

    sam_be_rmq_pub_t opts;
    memset (&opts, 0, sizeof (sam_be_rmq_pub_t));

    opts.exchange = "amq.direct";
    opts.payload = payload;

    int rc = sam_be_rmq_publish (rabbit, &opts);
    ck_assert_int_eq (rc, 0);

    zmq_pollitem_t items = {
        .socket = NULL,
        .fd = sam_be_rmq_sockfd (rabbit),
        .events = ZMQ_POLLIN,
        .revents = 0
    };

    // wait for ack to arrive
    sam_log_trace ("waiting for ACK");
    rc = zmq_poll (&items, 1, 500);
    ck_assert_int_eq (rc, 1);

    // TODO: this must either be removed
    // or replaced by something more useful
    sam_be_rmq_handle_ack (rabbit);
    zframe_destroy (&payload);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test if all public properties of the rmq backend instance are initialized.
START_TEST(test_be_rmq_async_beprops)
{
    sam_selftest_introduce ("test_be_rmq_async_beprops");

    if (!backend->name) {
        ck_abort_msg ("backend name not available");
    }

    if (!backend->id) {
        ck_abort_msg ("backend id not available");
    }

    if (!backend->sock_sig) {
        ck_abort_msg ("backend signal socket not available");
    }

    if (!backend->sock_pub) {
        ck_abort_msg ("backend push socket not available");
    }

    if (!backend->sock_rpc) {
        ck_abort_msg ("backend rpc request socket not available");
    }
}
END_TEST


//  --------------------------------------------------------------------------
/// Test asynchronous exchange declaration.
START_TEST(test_be_rmq_async_xdecl)
{
    sam_selftest_introduce ("test_be_rmq_async_xdecl");

    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "direct");
    zmsg_pushstr (zmsg, "x-test-async");
    zmsg_pushstr (zmsg, "exchange.declare");

    sam_msg_t *msg = sam_msg_new (&zmsg);
    int rc = zsock_send (backend->sock_rpc, "p", msg);
    ck_assert_int_eq (rc, 0);

    int ret = -1;
    rc = zsock_recv (backend->sock_rpc, "i", &ret);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (ret, 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test asynchronous exchange deletion.
START_TEST(test_be_rmq_async_xdel)
{
    sam_selftest_introduce ("test_be_rmq_async_xdel");

    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "x-test-async");
    zmsg_pushstr (zmsg, "exchange.delete");

    sam_msg_t *msg = sam_msg_new (&zmsg);
    int rc = zsock_send (backend->sock_rpc, "p", msg);
    ck_assert_int_eq (rc, 0);

    int ret = -1;
    rc = zsock_recv (backend->sock_rpc, "i", &ret);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (ret, 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test asynchronous publishing.
START_TEST(test_be_rmq_async_publish)
{
    sam_selftest_introduce ("test_be_rmq_async_publish");

    zmsg_t *zmsg = zmsg_new ();
    char *str_payload = "hi!";
    zframe_t *frame = zframe_new (str_payload, strlen (str_payload));

    // payload
    zmsg_prepend (zmsg, &frame);

    // headers
    zmsg_pushstr (zmsg, "0");

    // props
    for (int i = 0; i < 12; i++) {
        frame = zframe_new_empty ();
        zmsg_prepend (zmsg, &frame);
    }
    zmsg_pushstr (zmsg, "12");

    // args
    zmsg_pushstr (zmsg, "0");           // 4. immediate
    zmsg_pushstr (zmsg, "0");           // 3. mandatory
    frame = zframe_new_empty ();
    zmsg_prepend (zmsg, &frame);        // 2. routing key
    zmsg_pushstr (zmsg, "amq.direct");  // 1. exchange

    int msg_id = 17;
    sam_msg_t *msg = sam_msg_new (&zmsg);
    int rc = zsock_send (backend->sock_pub, "ip", msg_id, msg);
    ck_assert_int_eq (rc, 0);

    // wait for ack
    uint64_t returned_be_id;
    zframe_t *id_frame;
    int returned_msg_id;

    rc = zsock_recv (pll, "fi", &id_frame, &returned_msg_id);
    returned_be_id = *(uint64_t *) zframe_data (id_frame);
    zframe_destroy (&id_frame);

    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (returned_be_id, be_id);
    ck_assert_int_eq (returned_msg_id, msg_id);
}
END_TEST


//  --------------------------------------------------------------------------
/// Self test this class.
void *
sam_be_rmq_test ()
{
    Suite *s = suite_create ("sam_be_rmq");

    TCase *tc = tcase_create("synchronous");
    tcase_add_unchecked_fixture (tc, setup_connection, destroy_connection);
    tcase_add_test (tc, test_be_rmq_sync_xdecl);
    tcase_add_test (tc, test_be_rmq_sync_xdel);
    tcase_add_test (tc, test_be_rmq_sync_publish);
    suite_add_tcase (s, tc);

    tc = tcase_create("asynchronous");
    tcase_add_unchecked_fixture (tc, setup_backend, destroy_backend);
    tcase_add_test (tc, test_be_rmq_async_beprops);
    tcase_add_test (tc, test_be_rmq_async_xdecl);
    tcase_add_test (tc, test_be_rmq_async_xdel);
    tcase_add_test (tc, test_be_rmq_async_publish);
    suite_add_tcase (s, tc);

    return s;
}
