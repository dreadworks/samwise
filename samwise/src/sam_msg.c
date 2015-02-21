/*  =========================================================================

    sam_msg - central message distribution

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief message backend for RabbitMQ
   @file sam_msg_rabbit.c

   TODO description

*/

#include "../include/sam_prelude.h"


//  --------------------------------------------------------------------------
/// Callback for events on the internally used req/rep
/// connection. This function accepts messages crafted as defined by
/// the protocol to delegate them (based on the distribution method)
/// to various message backends.
static int
handle_actor_req (zloop_t *loop UU, zsock_t *rep, void *args)
{
    sam_msg_state_t *state = args;
    zframe_t *payload;
    char
        *distribution,
        *exchange,
        *routing_key;

    zsock_recv (
        rep, "sssf", &distribution, &exchange, &routing_key, &payload);

    sam_log_tracef (
        "handle req called, payload size: %d",
        zframe_size (payload));

    zsock_send (
        state->backend->req, "issf",
        SAM_MSG_REQ_PUBLISH,
        exchange, routing_key,
        payload);

    int rc = -1;
    zsock_recv (state->backend->req, "i", &rc);
    assert (rc != -1);

    sam_log_tracef ("received seqno %d", rc);
    zsock_send (rep, "i", 0);

    free (distribution);
    free (exchange);
    free (routing_key);
    zframe_destroy (&payload);

    return 0;
}


//  --------------------------------------------------------------------------
/// Demultiplexes acknowledgements arriving on the push/pull
/// connection wiring the backends to sam_msg.
static int
handle_backend_req (zloop_t *loop UU, zsock_t *pull, void *args UU)
{
    sam_log_trace ("handle backend request");
    zmsg_t *msg = zmsg_recv (pull);
    zmsg_destroy (&msg);
    return 0;
}


//  --------------------------------------------------------------------------
/// The thread encapsulated by sam_msg instances. Starts a loop
/// listening to the various sockets to multiplex and demultiplex
/// requests to a backend pool.
static void
actor (zsock_t *pipe, void *args)
{
    sam_msg_state_t *state = args;
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
/// Create a new sam_msg instance. It initializes both the sam_msg_t
/// object and the internally used state object.
sam_msg_t *
sam_msg_new (const char *name)
{
    int buf_s = 64;
    char buf [64];

    sam_msg_t *self = malloc (sizeof (sam_msg_t));
    sam_msg_state_t *state = malloc (sizeof (sam_msg_state_t));
    assert (self);

    // actor req/rep
    snprintf (buf, buf_s, "inproc://msg-actor-%s", name);
    self->actor_req = zsock_new_req (buf);
    assert (self->actor_req);
    state->actor_rep = zsock_new_rep (buf);
    assert (state->actor_rep);
    sam_log_tracef ("created req/rep pair at '%s'", buf);

    // backend pull
    snprintf (buf, buf_s, "inproc://msg-backend-%s", name);
    state->backend_pull = zsock_new_pull (buf);
    assert (state->backend_pull);
    sam_log_tracef ("created pull socket at '%s'", buf);

    int buf_len = strlen (buf);
    self->backend_pull_endpoint = malloc (buf_len + 1);
    memcpy (self->backend_pull_endpoint, buf, buf_len + 1);

    // actor
    self->actor = zactor_new (actor, state);
    sam_log_info ("created msg instance");

    self->state = state;
    return self;
}


//  --------------------------------------------------------------------------
/// Destroy the sam_msg_t instance. Also closes all sockets and free's
/// all memory of the internally used state object.
void
sam_msg_destroy (sam_msg_t **self)
{
    assert (self);
    sam_log_info ("destroying msg instance");
    zactor_destroy (&(*self)->actor);

    // TODO generic tear down
    sam_msg_backend_t *be = (*self)->state->backend;
    if (be) {
        sam_msg_rabbitmq_t *rabbit = sam_msg_rabbitmq_stop (&be);
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
create_be_rabbitmq (sam_msg_t *self, sam_msg_rabbitmq_opts_t *opts)
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
/// Create a new backend instance based on sam_msg_be_t.
int
sam_msg_create_backend (sam_msg_t *self, sam_msg_be_t be_type, void *opts)
{
    sam_log_infof ("creating backend %d", be_type);

    switch (be_type) {
    case SAM_MSG_BE_RABBITMQ:
        return create_be_rabbitmq (self, (sam_msg_rabbitmq_opts_t *) opts);

    default:
        sam_log_errorf ("unrecognized backend: %d", be_type);
    }

    return -1;
}


//  --------------------------------------------------------------------------
/// Delegates a publishing request to the thread.
int
sam_msg_publish (sam_msg_t *self, zmsg_t *msg)
{
    sam_log_trace ("publish message");
    zmsg_send (&msg, self->actor_req);

    int rc = -1;
    zsock_recv (self->actor_req, "i", &rc);
    assert (rc != -1);

    sam_log_tracef ("received return code %d", rc);
    return rc;
}


//  --------------------------------------------------------------------------
/// Self test this class.
void
sam_msg_test ()
{
    printf ("\n** MSG **\n");

    sam_msg_t *sms = sam_msg_new ("test");
    assert (sms);

    // testing rabbitmq
    sam_msg_rabbitmq_opts_t rabbitmq_opts = {
        .host = "localhost",
        .port = 5672,
        .user = "guest",
        .pass = "guest",
        .heartbeat = 2
    };

    sam_msg_create_backend (sms, SAM_MSG_BE_RABBITMQ, &rabbitmq_opts);

    // construct publishing message
    zmsg_t *msg = zmsg_new ();
    zframe_t *frame;

    char *pub_msg [] = {
        "redundant",
        "amq.direct",
        "",
        "hi!"
    };

    // distribution type
    uint frame_c = 0;
    for (; frame_c < sizeof (pub_msg) / sizeof (char *); frame_c++) {
        char *str = pub_msg[frame_c];
        frame = zframe_new (str, strlen (str));
        zmsg_append (msg, &frame);
    }

    sam_msg_publish (sms, msg);
    sam_msg_destroy (&sms);

    assert (!sms);
}
