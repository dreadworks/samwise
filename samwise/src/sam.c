/*  =========================================================================

    samwise - best effort store and forward message publishing

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief public API
   @file sam.c

   TODO description

*/


#include "../include/sam_prelude.h"


//  --------------------------------------------------------------------------
/// Send a reference of a proper sam_ret_t via INPROC.
static int
send_error (zsock_t *sock, sam_ret_t *ret, char *msg)
{
    ret->rc = -1;
    ret->msg = msg;
    sam_log_trace ("send frontend rep socket (error)");
    zsock_send (sock, "p", ret);
    return 0;
}


//  --------------------------------------------------------------------------
/// Prepare a rabbitmq publishing request.
static int
prepare_publish_rmq (sam_msg_t *msg)
{
    int rc = sam_msg_contain (msg, "ssf");
    return rc;
}


//  --------------------------------------------------------------------------
/// Publish a message to the backends.
static int
publish_to_backends (
    sam_state_t *state,
    sam_msg_t *msg,
    sam_ret_t *ret)
{
    char *distribution;
    int rc = sam_msg_pop (msg, "s", &distribution);
    if (rc == -1) {
        return send_error (
            state->frontend_rep, ret,
            "malformed request: distribution required");
    }

    int n = 1;
    if (!strcmp (distribution, "redundant")) {
        rc = sam_msg_pop (msg, "i", &n);
        if (rc == -1) {
            return send_error (
                state->frontend_rep, ret,
                "malformed request: distribution count required");
        }
    }

    sam_log_tracef ("publish %s(%d)", distribution, n);

    if (n <= 0 ||
        (strcmp (distribution, "redundant") &&
         strcmp (distribution, "round robin"))) {
        return send_error (
            state->frontend_rep, ret,
            "unknown distribution method");
    }

    rc = prepare_publish_rmq (msg);
    if (rc) {
        return send_error (state->frontend_rep, ret, "malformed request");
    }

    // all necessary information is now "contained"
    sam_log_trace ("send publishing request backend");

    // this is not distributing optimally; if redundant publishing is
    // not needed, replace with fair queuing through zeromq sockets.
    sam_backend_t *backend = zhash_next (state->backends);
    if (backend == NULL) {
        backend = zhash_first (state->backends);
    }

    if (backend != NULL) {
        zsock_send (backend->req, "sp", "publish", msg);
        distribution = NULL;

        int seqno = -1;
        sam_log_trace ("recv on the backend req socket");
        rc = zsock_recv (backend->req, "i", &seqno);

        if (rc == -1) {
            sam_log_error ("recv failed");
            return rc;
        }

        // should never occur, maybe make an assert out of it
        if (seqno == -1) {
            ret->rc = -1;
            ret->msg = "could not publish message, maybe wrong format";
        }

        sam_log_tracef ("backend req socket read, received seqno %d", seqno);
    }

    rc = zsock_send (state->frontend_rep, "p", ret);
    if (rc) {
        sam_log_error ("send failed");
        return rc;
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// Checks if the protocol for the incoming message is obeyed.
static int
prepare_rpc_rmq (sam_msg_t *msg)
{
    int rc = sam_msg_contain (msg, "s");
    if (rc) {
        return rc;
    }

    char *type;
    rc = sam_msg_contained (msg, "s", &type);
    if (rc == -1) {
        return rc;
    }

    if (!strcmp (type, "exchange.declare")) {
        rc = sam_msg_contain (msg, "ss");
    }

    else if (!strcmp (type, "exchange.delete")) {
        rc = sam_msg_contain (msg, "s");
    }

    else {
        return -1;
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// Make an rpc call to one or multiple backends.
static int
make_rpc_call (
    sam_state_t *state,
    sam_msg_t *msg,
    sam_ret_t *ret)
{
    char *broker;
    int rc = sam_msg_pop (msg, "s", &broker);
    if (rc) {
        return send_error (
            state->frontend_rep, ret, "broker required");
    }

    if (state->be_type == SAM_BE_RMQ) {
        rc = prepare_rpc_rmq (msg);
        if (rc) {
            return send_error (
                state->frontend_rep, ret, "malformed request");
        }
    }
    else {
        return send_error (
            state->frontend_rep, ret, "unknown backend type");
    }

    sam_backend_t *backend = zhash_first (state->backends);
    while (backend != NULL) {
        rc = zsock_send (backend->req, "sp", "rpc", msg);
        if (rc) {
            sam_log_error ("send failed");
            return rc;
        }

        int status = -1;
        rc = zsock_recv (backend->req, "i", &status);
        if (rc) {
            sam_log_error ("recv failed");
            return rc;
        }

        backend = zhash_next (state->backends);
    }

    rc = zsock_send (state->frontend_rep, "p", ret);
    return rc;
}



//  --------------------------------------------------------------------------
/// Callback for events on the internally used req/rep
/// connection. This function accepts messages crafted as defined by
/// the protocol to delegate them (based on the distribution method)
/// to various message backends.
static int
handle_frontend_req (zloop_t *loop UU, zsock_t *rep, void *args)
{
    sam_ret_t *ret = malloc (sizeof (sam_ret_t));
    assert (ret);
    ret->rc = 0;

    sam_state_t *state = args;

    sam_msg_t *msg;
    sam_log_trace ("recv on the frontends rep socket");
    int rc = zsock_recv (rep, "p", &msg);
    if (rc == -1) { // interrupted
        return -1;
    }

    assert (msg);
    char *action;
    rc = sam_msg_pop (msg, "s", &action);
    if (rc == -1) {
        return send_error(
            rep, ret,
            "malformed request: action required");
    }

    // publish
    sam_log_tracef ("frontends rep socket read, request %s", action);
    if (!strcmp (action, "publish")) {
        return publish_to_backends (state, msg, ret);
    }

    // rpc
    else if (!strcmp (action, "rpc")) {
        return make_rpc_call (state, msg, ret);
    }

    // ping
    else if (!strcmp (action, "ping")) {
        rc = zsock_send (rep, "p", ret);
        return rc;
    }

    return send_error (rep, ret, "unknown method");
}


//  --------------------------------------------------------------------------
/// Demultiplexes acknowledgements arriving on the push/pull
/// connection wiring the backends to sam.
static int
handle_backend_req (zloop_t *loop UU, zsock_t *pull, void *args UU)
{
    sam_log_trace ("recv on the backend pull socket");
    zmsg_t *msg = zmsg_recv (pull);
    zmsg_destroy (&msg);

    sam_log_trace ("backend pull socket read");
    return 0;
}


//  --------------------------------------------------------------------------
/// Handle control commands like adding or removing backends.
static int
handle_ctl_req (zloop_t *loop UU, zsock_t *rep, void *args)
{
    sam_state_t *state = args;

    char *cmd;
    zmsg_t *zmsg = zmsg_new ();

    zsock_recv (rep, "sm", &cmd, &zmsg);
    assert (cmd);

    sam_log_tracef ("got ctl command: '%s'", cmd);
    sam_msg_t *msg = sam_msg_new (&zmsg);

    // add a new backend
    if (!strcmp (cmd, "be.add")) {
        sam_backend_t *be;
        int rc = sam_msg_pop (msg, "p", &be);
        assert (!rc);
        assert (be);

        sam_log_infof ("inserting backend '%s'", be->name);
        zhash_insert (state->backends, be->name, be);
    }

    else {
        assert (false);
    }

    // clean up
    free (cmd);
    sam_msg_destroy (&msg);

    zsock_send (rep, "i", 0);
    return 0;
}



//  --------------------------------------------------------------------------
/// The thread encapsulated by sam instances. Starts a loop
/// listening to the various sockets to multiplex and demultiplex
/// requests to a backend pool.
static void
actor (zsock_t *pipe, void *args)
{
    sam_state_t *state = args;
    zloop_t *loop = zloop_new ();

    zloop_reader (loop, state->frontend_rep, handle_frontend_req, state);
    zloop_reader (loop, state->backend_pull, handle_backend_req, state);
    zloop_reader (loop, state->ctl_rep, handle_ctl_req, state);
    zloop_reader (loop, pipe, sam_gen_handle_pipe, NULL);

    sam_log_info ("starting poll loop");
    zsock_signal (pipe, 0);
    zloop_start (loop);

    sam_log_trace ("destroying loop");
    zloop_destroy (&loop);
}


//  --------------------------------------------------------------------------
/// Create a new sam instance. It initializes both the sam_t
/// object and the internally used state object.
sam_t *
sam_new (sam_be_t be_type)
{
    sam_t *self = malloc (sizeof (sam_t));
    sam_state_t *state = malloc (sizeof (sam_state_t));
    assert (self);

    // actor control commands
    char *endpoint = "inproc://sam-ctl";
    self->ctl_req = zsock_new_req (endpoint);
    assert (self->ctl_req);
    state->ctl_rep = zsock_new_rep (endpoint);
    assert (state->ctl_rep);
    sam_log_tracef ("created req/rep pair at '%s'", endpoint);

    // actor frontend/backend
    endpoint = "inproc://sam-frontend";
    self->frontend_req = zsock_new_req (endpoint);
    assert (self->frontend_req);
    state->frontend_rep = zsock_new_rep (endpoint);
    assert (state->frontend_rep);
    sam_log_tracef ("created req/rep pair at '%s'", endpoint);

    // backend pull
    endpoint = "inproc://sam-backend";
    state->backend_pull = zsock_new_pull (endpoint);
    assert (state->backend_pull);
    sam_log_tracef ("created pull socket at '%s'", endpoint);

    int str_len = strlen (endpoint);
    self->backend_pull_endpoint = malloc (str_len + 1);
    memcpy (self->backend_pull_endpoint, endpoint, str_len + 1);
    self->backend_pull_endpoint[str_len] = '\0';

    // actor
    self->actor = zactor_new (actor, state);
    sam_log_info ("created msg instance");

    state->be_type = be_type;
    self->be_type = be_type;

    state->backends = zhash_new ();
    self->state = state;
    return self;
}


//  --------------------------------------------------------------------------
/// Destroy the sam_t instance. Also closes all sockets and free's
/// all memory of the internally used state object.
void
sam_destroy (sam_t **self)
{
    assert (self);
    sam_log_info ("destroying sam instance");
    zactor_destroy (&(*self)->actor);

    // destroy backends
    sam_state_t *state = (*self)->state;
    sam_backend_t *backend = zhash_first (state->backends);
    while (backend != NULL) {
        sam_log_tracef ("trying to delete backend %s", backend->name);

        // rabbitmq backends
        if (state->be_type == SAM_BE_RMQ) {
            sam_be_rmq_t *rabbit = sam_be_rmq_stop (&backend);
            sam_be_rmq_destroy (&rabbit);
        }

        else {
            assert (false);
        }
    }
    zhash_destroy (&state->backends);

    zsock_destroy (&(*self)->state->backend_pull);
    free ((*self)->backend_pull_endpoint);

    zsock_destroy (&(*self)->state->frontend_rep);
    zsock_destroy (&(*self)->frontend_req);

    zsock_destroy (&(*self)->state->ctl_rep);
    zsock_destroy (&(*self)->ctl_req);

    free ((*self)->state);
    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
/// Function to create a sam_be_rmq instance and transform it
/// into a generic backend.
static sam_backend_t *
create_be_rmq (
    sam_t *self,
    const char *name,
    sam_be_rmq_opts_t *opts)
{
    sam_be_rmq_opts_t *rabbit_opts = opts;
    sam_be_rmq_t *rabbit = sam_be_rmq_new (name);
    assert (rabbit);

    sam_be_rmq_connect (rabbit, rabbit_opts); // TODO handle erors
    sam_backend_t *be = sam_be_rmq_start (
        &rabbit, self->backend_pull_endpoint);

    return be;
}


//  --------------------------------------------------------------------------
/// Create a new backend instance based on sam_be_t.
sam_backend_t *
sam_be_create (
    sam_t *self,
    const char *name,
    void *opts)
{
    sam_log_infof ("creating backend '%s'", name);

    if (self->be_type == SAM_BE_RMQ) {
        return create_be_rmq (self, name, opts);
    }

    else {
        sam_log_errorf ("unrecognized backend: %d", self->be_type);
    }

    return NULL;
}


//  --------------------------------------------------------------------------
/// Read from a configuration file (TODO #32). Currently just
/// statically creates a sam_msg instance with one rabbitmq broker
/// connection.
int
sam_init (sam_t *self, const char *conf UU)
{
    // TODO create shared config (#32)
    sam_be_rmq_opts_t rmq_opts = {
        .host = "localhost",
        .port = 5672,
        .user = "guest",
        .pass = "guest",
        .heartbeat = 3
    };

    const char *be_name = "be1";
    sam_backend_t *be = sam_be_create (self, be_name, &rmq_opts);
    zsock_send (self->ctl_req, "sp", "be.add", be);

    int rc = -1;
    zsock_recv (self->ctl_req, "i", &rc);
    if (!rc) {
        sam_log_error ("could not create backend");
    }

    sam_log_info ("(re)loaded configuration");
    return 0;
}


//  --------------------------------------------------------------------------
/// Send the sam actor thread a message.
sam_ret_t *
sam_send_action (sam_t *self, sam_msg_t **msg)
{
    sam_ret_t *ret;
    if (sam_msg_size (*msg) < 1) {
        ret = malloc (sizeof (sam_ret_t));
        ret->rc = -1;
        ret->msg = "malformed request: action required";
    }
    else {
        sam_log_trace ("send on frontends req socket");
        zsock_send (self->frontend_req, "p", *msg);

        sam_log_trace ("recv on frontends req socket");
        zsock_recv (self->frontend_req, "p", &ret);
        sam_log_tracef ("frontends req socket read, code: %d", ret->rc);
    }

    sam_msg_destroy (msg);
    return ret;
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
/// Asserts that sam_send_action will return with an error.
static void
test_assert_error (sam_t *sam, sam_msg_t *msg)
{
    sam_ret_t *ret = sam_send_action (sam, &msg);
    assert (ret->rc == -1);
    assert (!msg);

    sam_log_tracef ("got error: %s", ret->msg);
    free (ret);
}


//  --------------------------------------------------------------------------
/// Self test this class.
void
sam_test ()
{
    printf ("\n** MSG **\n");

    sam_t *sam = sam_new (SAM_BE_RMQ);
    assert (sam);

    // testing rabbitmq
    sam_init (sam, NULL);


    // testing rabbitmq publish
    char *pub_msg [] = {
        "publish",        // action
        "round robin",    // distribution type
        "amq.direct",     // exchange
        "",               // routing key
        "hi!"             // payload
    };

    size_t char_s = sizeof (char *);
    sam_msg_t *msg = test_create_msg (sizeof (pub_msg) / char_s, pub_msg);
    sam_ret_t *ret = sam_send_action (sam, &msg);
    assert (!ret->rc);
    assert (!msg);
    free (ret);

    // rpc: exchange declare
    char *exch_decl_msg [] = {
        "rpc",
        "",                 // broker name
        "exchange.declare", // action
        "test-x",           // exchange name
        "direct"            // type
    };

    msg = test_create_msg (sizeof (exch_decl_msg) / char_s, exch_decl_msg);
    ret = sam_send_action (sam, &msg);
    assert (!ret->rc);
    assert (!msg);
    free (ret);


    // rpc: exchange delete
    char *exch_del_msg [] = {
        "rpc",
        "",                // broker name
        "exchange.delete", // action
        "test-x"           // exchange name
    };

    msg = test_create_msg (sizeof (exch_del_msg) / char_s, exch_del_msg);
    ret = sam_send_action (sam, &msg);
    assert (!ret->rc);
    assert (!msg);
    free (ret);


    // wrong formats
    // empty message
    zmsg_t *zmsg = zmsg_new ();
    msg = sam_msg_new (&zmsg);
    test_assert_error (sam, msg);

    // unknown method
    char *a_unknwn_meth [] = {
        "consume", "amq.direct", ""
    };
    msg = test_create_msg (sizeof (a_unknwn_meth) / char_s, a_unknwn_meth);
    test_assert_error (sam, msg);

    // missing distribution type
    char *a_miss_distr [] = {
        "publish", "amq.direct", "", "hi!"
    };
    msg = test_create_msg (sizeof (a_miss_distr) / char_s, a_miss_distr);
    test_assert_error (sam, msg);

    // missing distribution count for "redundant"
    char *a_miss_red_c [] = {
        "publish", "redundant", "amq.direct", "", "hi!"
    };
    msg = test_create_msg (sizeof (a_miss_red_c) / char_s, a_miss_red_c);
    test_assert_error (sam, msg);

    // wrong publish
    char *a_pub_wrong [] = {
        "publish", "round robin", "amq.direct"
    };
    msg = test_create_msg (sizeof (a_pub_wrong) / char_s, a_pub_wrong);
    test_assert_error (sam, msg);


    // wrong exchange.declare
    char *a_xdcl_wrong1 [] = {
        "rpc", "exchange.declare"
    };
    msg = test_create_msg (sizeof (a_xdcl_wrong1) / char_s, a_xdcl_wrong1);
    test_assert_error (sam, msg);

    char *a_xdcl_wrong2 [] = {
        "rpc", "exchange.declare", "", ""
    };
    msg = test_create_msg (sizeof (a_xdcl_wrong2) / char_s, a_xdcl_wrong2);
    test_assert_error (sam, msg);

    char *a_xdcl_wrong3 [] = {
        "rpc", "exchange.declare", "foo"
    };
    msg = test_create_msg (sizeof (a_xdcl_wrong3) / char_s, a_xdcl_wrong3);
    test_assert_error (sam, msg);


    // wrong exchange.delete
    char *a_xdel_wrong1 [] = {
        "rpc", "exchange.delete"
    };
    msg = test_create_msg (sizeof (a_xdel_wrong1) / char_s, a_xdel_wrong1);
    test_assert_error (sam, msg);

    char *a_xdel_wrong2 [] = {
        "rpc", "exchange.delete", ""
    };
    msg = test_create_msg (sizeof (a_xdel_wrong2) / char_s, a_xdel_wrong2);
    test_assert_error (sam, msg);


    sam_destroy (&sam);
    assert (!sam);
}
