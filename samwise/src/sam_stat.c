/*  =========================================================================

    sam_be_rmq - message backend for the RabbitMQ AMQP broker

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief message backend for RabbitMQ
   @file sam_be_rmq.c

   <code>

   any other module | sam_stat
   -------------------------------
     PUSH: send statistical data


   Topology:
   ---------

   any other module [i] o -----> o sam_stat actor
                      PUSH      PULL

    </code>
*/

#include "../include/sam_prelude.h"


const char *ENDPOINT_PSHPLL = "inproc://sam_stat_metrics";
const char *ENDPOINT_REQREP = "inproc://sam_stat_digest";


/// actor state, maintains all metrics
typedef struct state_t {

    zsock_t *pll;
    zsock_t *rep;

    struct {
        struct {
            uint64_t accepted;
        } sam;
    } metrics;

} state_t;


struct sam_stat_handle_t {
    zsock_t *psh;
    zsock_t *req;
};


struct sam_stat_t {
    zactor_t *actor;
};



static int
resolve (
    state_t *state,
    char *id,
    int difference)
{
    const char *delim = ".";
    char *tok = strtok (id, delim);

    if (tok) {

        char *ref = tok;
        tok = strtok (NULL, delim);
        uint64_t *target = NULL;

        if (!strcmp (ref, "sam")) {
            if (!strcmp (tok, "accepted")) {
                target = &state->metrics.sam.accepted;
            }
        }

        if (target) {
            *target += difference;
            return 0;
        }
    }

    return -1;
}



int
handle_metric (
    zloop_t *loop UU,
    zsock_t *pll,
    void *arg)
{
    state_t *state = arg;

    char *id;
    int difference;

    zsock_recv (pll, "si", &id, &difference);
    sam_log_tracef ("handle metric request '%s'", id);

    if (resolve (state, id, difference)) {
        sam_log_errorf ("discarding stat request '%s'", id);
    }

    free (id);
    return 0;
}



int
handle_digest (
    zloop_t *loop UU,
    zsock_t *rep,
    void *arg)
{
    state_t *state = arg;

    sam_log_trace ("recv () digest request");
    zsock_recv (rep, "z");

    size_t buf_size = 256;
    char buf [buf_size];

    snprintf (
        buf, 256,
        "METRICS\n"
        "  sam:\n"
        "    accepted: %" PRIu64 "\n",

        state->metrics.sam.accepted);

    sam_log_trace ("send () digest response");
    zsock_send (rep, "s", buf);
    return 0;
}



void
actor (
    zsock_t *pipe,
    void *arg UU)
{
    zloop_t *loop = zloop_new ();

    state_t state;
    memset (&state, 0, sizeof (state_t));


    // initialize sockets
    state.pll = zsock_new_pull (ENDPOINT_PSHPLL);
    assert (state.pll);
    state.rep = zsock_new_rep (ENDPOINT_REQREP);
    assert (state.rep);


    // initialize reactor
    zloop_reader (loop, pipe, sam_gen_handle_pipe, NULL);
    zloop_reader (loop, state.pll, handle_metric, &state);
    zloop_reader (loop, state.rep, handle_digest, &state);


    // start
    zsock_signal (pipe, 0);
    sam_log_info ("ready to gather metrics");
    zloop_start (loop);


    // clean up
    sam_log_info ("shutting down");
    zloop_destroy (&loop);
    zsock_destroy (&state.pll);
    zsock_destroy (&state.rep);
}




sam_stat_t *
sam_stat_new ()
{
    sam_stat_t *self = malloc (sizeof (sam_stat_t));
    assert (self);

    self->actor = zactor_new (actor, NULL);
    sam_log_info ("created metric aggregator");

    return self;
}


void
sam_stat_destroy (
    sam_stat_t **self)
{
    assert (*self);

    zactor_destroy (&(*self)->actor);

    free (*self);
    *self = NULL;
}


sam_stat_handle_t *
sam_stat_handle_new ()
{
    sam_stat_handle_t *handle = malloc (sizeof (sam_stat_handle_t));
    assert (handle);

    handle->psh = zsock_new_push (ENDPOINT_PSHPLL);
    assert (handle->psh);

    handle->req = zsock_new_req (ENDPOINT_REQREP);
    assert (handle->req);

    assert (handle->psh);

    return handle;
}


void
sam_stat_handle_destroy (
    sam_stat_handle_t **handle)
{
    assert (*handle);
    zsock_destroy (&(*handle)->psh);
    zsock_destroy (&(*handle)->req);

    free (*handle);
    *handle = NULL;
}


void
sam_stat_ (
    sam_stat_handle_t *handle,
    const char *id,
    int difference)
{
    assert (handle);
    assert (handle->psh);

    zsock_send (handle->psh, "si", id, difference);
}


char *
sam_stat_str_ (
    sam_stat_handle_t *handle)
{
    assert (handle);
    assert (handle->req);

    sam_log_trace ("send () string repr request");
    zsock_send (handle->req, "z");

    char *str;
    sam_log_trace ("recv () string repr");
    zsock_recv (handle->req, "s", &str);
    return str;
}

