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
handle_req (zloop_t *loop UU, zsock_t *rep, void *args)
{
    sam_msg_state_t *state = args;
    zmsg_t *msg = zmsg_new ();
    char *distribution;

    zsock_recv (rep, "sm", &distribution, &msg);
    sam_log_tracef (state->logger, "handle req called (%s)", distribution);

    zsock_send (rep, "i", 0);

    free (distribution);
    zmsg_destroy (&msg);
    return 0;
}


//  --------------------------------------------------------------------------
/// Callback for requests on the actor's PIPE. Only handles interrupts
/// and termination commands.
static int
handle_pipe (zloop_t *loop UU, zsock_t *pipe, void *args)
{
    sam_msg_state_t *self = args;
    zmsg_t *msg = zmsg_recv (pipe);

    if (!msg) {
        sam_log_info (self->logger, "pipe: interrupted!");
        return -1;
    }

    bool term = false;
    char *cmd = zmsg_popstr (msg);
    if (!strcmp (cmd, "$TERM")) {
        sam_log_info (self->logger, "pipe: terminated");
        term = true;
    }

    free (cmd);
    zmsg_destroy (&msg);

    if (term) {
        return -1;
    }

    return 0;
}


static void
actor (zsock_t *pipe, void *args)
{
    sam_msg_state_t *state = args;
    zloop_t *loop = zloop_new ();

    zloop_reader (loop, state->rep, handle_req, state);
    zloop_reader (loop, pipe, handle_pipe, state);

    sam_log_info (state->logger, "starting poll loop");
    zsock_signal (pipe, 0);
    zloop_start (loop);

    sam_log_trace (state->logger, "destroying loop");
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

    snprintf (buf, buf_s, "msg_%s", name);
    self->logger = sam_logger_new (buf, SAM_LOG_ENDPOINT);

    snprintf (buf, buf_s, "msg_actor_%s", name);
    state->logger = sam_logger_new (buf, SAM_LOG_ENDPOINT);

    snprintf (buf, buf_s, "inproc://msg-%s", name);
    sam_log_tracef (self->logger, "creating req/rep pair at: %s", buf);

    state->rep = zsock_new_rep (buf);
    assert (state->rep);

    self->req = zsock_new_req (buf);
    assert (self->req);

    self->actor = zactor_new (actor, state);
    sam_log_info (self->logger, "created msg instance");

    self->state = state;
    return self;
}


void
sam_msg_destroy (sam_msg_t **self)
{
    assert (self);
    sam_log_info ((*self)->logger, "destroying msg instance");
    zactor_destroy (&(*self)->actor);

    sam_logger_destroy (&(*self)->logger);
    sam_logger_destroy (&(*self)->state->logger);

    zsock_destroy (&(*self)->req);
    zsock_destroy (&(*self)->state->rep);

    free ((*self)->state);
    free (*self);
    *self = NULL;
}


int
sam_msg_publish (sam_msg_t *self, zmsg_t *msg)
{
    sam_log_trace (self->logger, "publish message");
    zmsg_send (&msg, self->req);

    int rc;
    sam_log_trace (self->logger, "waiting to receive");
    zsock_recv (self->req, "i", &rc);
    sam_log_tracef (self->logger, "received %dms delay", rc);
    return rc;
}


void
sam_msg_test ()
{
    sam_msg_t *sms = sam_msg_new ("test");
    assert (sms);

    // construct publishing message
    zmsg_t *msg = zmsg_new ();
    char *str;
    zframe_t *frame;

    // frame 1: distribution type
    str = "redundant";
    frame = zframe_new (str, strlen (str));
    zmsg_append (msg, &frame);

    // frame 2: exchange name
    str = "amq.direct";
    frame = zframe_new (str, strlen (str));
    zmsg_append (msg, &frame);

    // frame 3: payload
    str = "hi!";
    frame = zframe_new (str, strlen (str));
    zmsg_append (msg, &frame);

    sam_msg_publish (sms, msg);
    sam_msg_destroy (&sms);

    assert (!sms);
}
