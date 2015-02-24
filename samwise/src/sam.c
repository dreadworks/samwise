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
static void
send_error (zsock_t *sock, sam_ret_t *ret, char *msg)
{
    ret->rc = -1;
    ret->msg = msg;
    zsock_send (sock, "p", ret);
}


//  --------------------------------------------------------------------------
/// Callback for events on the internally used req/rep
/// connection. This function accepts messages crafted as defined by
/// the protocol to delegate them (based on the distribution method)
/// to various message backends.
static int
handle_actor_req (zloop_t *loop UU, zsock_t *rep, void *args)
{
    sam_ret_t *ret = malloc (sizeof (sam_ret_t));
    assert (ret);
    ret->rc = 0;

    sam_state_t *state = args;
    assert (state->backend);

    zmsg_t *msg = zmsg_new ();
    char *action = NULL;

    zsock_recv (rep, "sm", &action, &msg);

    if (action == NULL) {
        send_error(rep, ret, "malformed request: action required");
        return 0;
    }

    // publish
    sam_log_tracef ("request %s", action);
    if (!strcmp (action, "publish")) {
        char *distribution = zmsg_popstr (msg);

        int n = 1;
        if (!strcmp (distribution, "redundant")) {
            n = zmsg_popint (msg);
        }

        sam_log_tracef ("publish %s(%d)", distribution, n);

        if (n <= 0 ||
            (strcmp (distribution, "redundant") &&
             strcmp (distribution, "round robin"))) {
                send_error (rep, ret, "unknown distribution method");
                free (distribution);
                goto clean;
        }
        free (distribution);
    }

    // rpc
    else if (!strcmp (action, "rpc")) {
        if (zmsg_size (msg) < 2) {
            send_error (rep, ret, "rpc type and broker required");
            goto clean;
        }

        char *broker = zmsg_popstr (msg);
        char *type = zmsg_popstr (msg);

        if (
            strcmp (type, "exchange.declare") &&
            strcmp (type, "exchange.delete")) {

            send_error (rep, ret, "rpc type not known");
            free (broker);
            free (type);
            goto clean;
        }

        free (action);
        action = type;

        free (broker);
    }

    // ping
    else if (!strcmp (action, "ping")) {
        zsock_send (rep, "p", ret);
        goto clean;
    }

    else {
        send_error (rep, ret, "unknown method");
        goto clean;
    }

    zsock_send (state->backend->req, "sm", action, msg);

    int rc = -1;
    zsock_recv (state->backend->req, "i", &rc);
    if (rc == -1) {
        ret->rc = -1;
        ret->msg = "could not publish message, maybe wrong format";
    }

    sam_log_tracef ("received seqno %d", rc);
    zsock_send (rep, "p", ret);

clean:
    free (action);
    zmsg_destroy (&msg);
    return 0;
}


//  --------------------------------------------------------------------------
/// Demultiplexes acknowledgements arriving on the push/pull
/// connection wiring the backends to sam.
static int
handle_backend_req (zloop_t *loop UU, zsock_t *pull, void *args UU)
{
    sam_log_trace ("handle backend request");
    zmsg_t *msg = zmsg_recv (pull);
    zmsg_destroy (&msg);
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

    zloop_reader (loop, state->actor_rep, handle_actor_req, state);
    zloop_reader (loop, state->backend_pull, handle_backend_req, state);
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
sam_new ()
{
    sam_t *self = malloc (sizeof (sam_t));
    sam_state_t *state = malloc (sizeof (sam_state_t));
    assert (self);

    // actor req/rep
    char *endpoint = "inproc://sam-actor";
    self->actor_req = zsock_new_req (endpoint);
    assert (self->actor_req);
    state->actor_rep = zsock_new_rep (endpoint);
    assert (state->actor_rep);
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

    state->backend = NULL;
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
    sam_log_info ("destroying msg instance");
    zactor_destroy (&(*self)->actor);

    // TODO generic tear down
    sam_backend_t *backend = (*self)->state->backend;
    if (backend) {
        sam_msg_rabbitmq_t *rabbit = sam_msg_rabbitmq_stop (&backend);
        sam_msg_rabbitmq_destroy (&rabbit);
    }

    zsock_destroy (&(*self)->state->backend_pull);
    free ((*self)->backend_pull_endpoint);

    zsock_destroy (&(*self)->state->actor_rep);
    zsock_destroy (&(*self)->actor_req);

    free ((*self)->state);
    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
/// Function to create a sam_msg_rabbitmq instance and transform it
/// into a generic backend.
static int
create_be_rabbitmq (sam_t *self, sam_msg_rabbitmq_opts_t *opts)
{
    sam_msg_rabbitmq_opts_t *rabbit_opts = opts;
    sam_msg_rabbitmq_t *rabbit = sam_msg_rabbitmq_new (0); // TODO id
    assert (rabbit);

    sam_msg_rabbitmq_connect (rabbit, rabbit_opts);

    // TODO state MUST NOT be accessed from outside the running thread.
    // this is part of #44.
    self->state->backend =
        sam_msg_rabbitmq_start (&rabbit, self->backend_pull_endpoint);

    return 0;
}


//  --------------------------------------------------------------------------
/// Create a new backend instance based on sam_be_t.
int
sam_be_create (sam_t *self, sam_be_t be_type, void *opts)
{
    sam_log_infof ("creating backend %d", be_type);

    switch (be_type) {
    case SAM_BE_RABBITMQ:
        return create_be_rabbitmq (self, opts);

    default:
        sam_log_errorf ("unrecognized backend: %d", be_type);
    }

    return -1;
}


//  --------------------------------------------------------------------------
/// Read from a configuration file (TODO #32). Currently just
/// statically creates a sam_msg instance with one rabbitmq broker
/// connection.
int
sam_init (sam_t *self, const char *conf UU)
{
    // TODO create shared config (#32)
    sam_msg_rabbitmq_opts_t rabbitmq_opts = {
        .host = "localhost",
        .port = 5672,
        .user = "guest",
        .pass = "guest",
        .heartbeat = 3
    };

    int rc = sam_be_create (self, SAM_BE_RABBITMQ, &rabbitmq_opts);

    if (rc != 0) {
        sam_log_error ("could not create rabbitmq backend");
    }

    sam_log_info ("(re)loaded configuration");
    return 0;
}


//  --------------------------------------------------------------------------
/// TODO analyze message content before passing it on, part of #52.
sam_ret_t *
sam_send_action (sam_t *self, zmsg_t **msg)
{
    sam_ret_t *ret;
    if (zmsg_size (*msg) < 1) {
        ret = malloc (sizeof (sam_ret_t));
        ret->rc = -1;
        ret->msg = "malformed request: action required";
    }
    else {
        zmsg_send (msg, self->actor_req);
        zsock_recv (self->actor_req, "p", &ret);
    }

    zmsg_destroy (msg);
    return ret;
}


//  --------------------------------------------------------------------------
/// Creates a zmsg containing a variable number of strings as frames
static zmsg_t*
create_zmsg (uint arg_c, char **arg_v)
{
    zmsg_t *msg = zmsg_new ();
    uint frame_c = 0;

    for (; frame_c < arg_c; frame_c++) {
        char *str = arg_v[frame_c];
        zframe_t *frame = zframe_new (str, strlen (str));
        zmsg_append (msg, &frame);
    }

    return msg;
}


static void
assert_error (sam_t *sam, zmsg_t *msg)
{
    sam_ret_t *ret = sam_send_action (sam, &msg);
    assert (ret->rc == -1);
    sam_log_tracef ("got error: %s", ret->msg);
    free (ret);
}


//  --------------------------------------------------------------------------
/// Self test this class.
void
sam_test ()
{
    printf ("\n** MSG **\n");

    sam_t *sam = sam_new ("test");
    assert (sam);

    // testing rabbitmq
    sam_msg_rabbitmq_opts_t rabbitmq_opts = {
        .host = "localhost",
        .port = 5672,
        .user = "guest",
        .pass = "guest",
        .heartbeat = 2
    };

    sam_be_create (sam, SAM_BE_RABBITMQ, &rabbitmq_opts);


    // testing rabbitmq publish
    char *pub_msg [] = {
        "publish",        // action
        "round robin",    // distribution type
        "amq.direct",     // exchange
        "",               // routing key
        "hi!"             // payload
    };

    size_t char_s = sizeof (char *);
    zmsg_t *msg = create_zmsg (sizeof (pub_msg) / char_s, pub_msg);
    sam_ret_t *ret = sam_send_action (sam, &msg);
    assert (!ret->rc);
    free (ret);


    // rpc: exchange declare
    char *exch_decl_msg [] = {
        "rpc",
        "",                 // broker name
        "exchange.declare", // action
        "test-x",           // exchange name
        "direct"            // type
    };

    msg = create_zmsg (sizeof (exch_decl_msg) / char_s, exch_decl_msg);
    ret = sam_send_action (sam, &msg);
    assert (!ret->rc);
    free (ret);


    // rpc: exchange delete
    char *exch_del_msg [] = {
        "rpc",
        "",                // broker name
        "exchange.delete", // action
        "test-x"           // exchange name
    };

    msg = create_zmsg (sizeof (exch_del_msg) / char_s, exch_del_msg);
    ret = sam_send_action (sam, &msg);
    assert (!ret->rc);
    free (ret);


    // wrong formats
    // empty message
    msg = zmsg_new ();
    assert_error (sam, msg);

    // unknown method
    char *a_unknwn_meth [] = {
        "consume", "amq.direct", ""
    };
    msg = create_zmsg (sizeof (a_unknwn_meth) / char_s, a_unknwn_meth);
    assert_error (sam, msg);

    // missing distribution type
    char *a_miss_distr [] = {
        "publish", "amq.direct", "", "hi!"
    };
    msg = create_zmsg (sizeof (a_miss_distr) / char_s, a_miss_distr);
    assert_error (sam, msg);

    // missing distribution count for "redundant"
    char *a_miss_red_c [] = {
        "publish", "redundant", "amq.direct", "", "hi!"
    };
    msg = create_zmsg (sizeof (a_miss_red_c) / char_s, a_miss_red_c);
    assert_error (sam, msg);

    // wrong publish
    char *a_pub_wrong [] = {
        "publish", "round robin", "amq.direct"
    };
    msg = create_zmsg (sizeof (a_pub_wrong) / char_s, a_pub_wrong);
    assert_error (sam, msg);


    // wrong exchange.declare
    char *a_xdcl_wrong1 [] = {
        "rpc", "exchange.declare"
    };
    msg = create_zmsg (sizeof (a_xdcl_wrong1) / char_s, a_xdcl_wrong1);
    assert_error (sam, msg);

    char *a_xdcl_wrong2 [] = {
        "rpc", "exchange.declare", "", ""
    };
    msg = create_zmsg (sizeof (a_xdcl_wrong2) / char_s, a_xdcl_wrong2);
    assert_error (sam, msg);

    char *a_xdcl_wrong3 [] = {
        "rpc", "exchange.declare", "foo"
    };
    msg = create_zmsg (sizeof (a_xdcl_wrong3) / char_s, a_xdcl_wrong3);
    assert_error (sam, msg);


    // wrong exchange.delete
    char *a_xdel_wrong1 [] = {
        "rpc", "exchange.delete"
    };
    msg = create_zmsg (sizeof (a_xdel_wrong1) / char_s, a_xdel_wrong1);
    assert_error (sam, msg);

    char *a_xdel_wrong2 [] = {
        "rpc", "exchange.delete", ""
    };
    msg = create_zmsg (sizeof (a_xdel_wrong2) / char_s, a_xdel_wrong2);
    assert_error (sam, msg);


    sam_destroy (&sam);
    assert (!sam);
}
