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



static int
handle_actor_req (zloop_t *loop UU, zsock_t *rep, void *args)
{
    sam_msg_state_t *state = args;
    zmsg_t *msg = zmsg_new ();
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

    zsock_send (rep, "i", 0);

    free (distribution);
    zmsg_destroy (&msg);
    return 0;
}


static int
handle_backend_req (zloop_t *loop UU, zsock_t *pull, void *args UU)
{
    sam_log_trace ("handle backend request");
    zmsg_t *msg = zmsg_recv (pull);
    zmsg_destroy (&msg);
    return 0;
}


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
    printf ("EXIT LOOP SAM_MSG\n");

    sam_log_trace ("destroying loop");
    zloop_destroy (&loop);
}


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

    // backend pull
    snprintf (buf, buf_s, "inproc://msg-backend-%s", name);
    sam_log_tracef ("pull socket at: %s", buf);
    state->backend_pull = zsock_new_pull (buf);
    assert (state->backend_pull);

    int buf_len = strlen (buf);
    self->backend_pull_endpoint = malloc (buf_len + 1);
    memcpy (self->backend_pull_endpoint, buf, buf_len + 1);

    // actor
    self->actor = zactor_new (actor, state);
    sam_log_info ("created msg instance");

    self->state = state;
    return self;
}


void
sam_msg_destroy (sam_msg_t **self)
{
    assert (self);
    sam_log_info ("destroying msg instance");
    zactor_destroy (&(*self)->actor);

    zsock_destroy (&(*self)->state->backend_pull);
    free ((*self)->backend_pull_endpoint);

    zsock_destroy (&(*self)->state->actor_rep);
    zsock_destroy (&(*self)->actor_req);

    free ((*self)->state);
    free (*self);
    *self = NULL;
}


static int
create_be_rabbitmq (sam_msg_t *self, sam_msg_rabbitmq_opts_t *opts)
{
    sam_msg_rabbitmq_opts_t *rabbit_opts = opts;
    sam_msg_rabbitmq_t *rabbit = sam_msg_rabbitmq_new (0); // TODO id
    assert (rabbit);

    sam_msg_rabbitmq_connect (rabbit, rabbit_opts);
    self->state->backend =
        sam_msg_rabbitmq_start (&rabbit, self->backend_pull_endpoint);

    return 0;
}


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


int
sam_msg_publish (sam_msg_t *self, zmsg_t *msg)
{
    sam_log_trace ("publish message");
    zmsg_send (&msg, self->actor_req);

    int rc;
    sam_log_trace ("waiting to receive");
    zsock_recv (self->actor_req, "i", &rc);
    sam_log_tracef ("received return code %d", rc);
    return rc;
}


void
sam_msg_test ()
{
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
    char *str;
    zframe_t *frame;

    // action
    str = "publish";
    frame = zframe_new (str, strlen (str));
    zmsg_append (msg, &frame);

    // distribution type
    str = "redundant";
    frame = zframe_new (str, strlen (str));
    zmsg_append (msg, &frame);

    // exchange name
    str = "amq.direct";
    frame = zframe_new (str, strlen (str));
    zmsg_append (msg, &frame);

    // payload
    str = "hi!";
    frame = zframe_new (str, strlen (str));
    zmsg_append (msg, &frame);

    sam_msg_publish (sms, msg);
    sam_msg_destroy (&sms);

    assert (!sms);
}
