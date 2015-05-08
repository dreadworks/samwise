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

    zsock_t *pll;   ///< pull socket accepting metric requests
    zsock_t *rep;   ///< reply socket for digest-requests

    /// set of hashes for different modules
    struct {
        zhash_t *sam;
        zhash_t *samd;
    } metrics;

} state_t;


/// opaque handles to update metrics concurrently
struct sam_stat_handle_t {
    zsock_t *psh;   ///< push socket to send metric requests
    zsock_t *req;   ///< request socket to obtain digests
};


/// aggregator instance
struct sam_stat_t {
    zactor_t *actor;
};



//  --------------------------------------------------------------------------
/// Free-function for the hash maps.
static void
free_metric (void **item)
{
    assert (*item);
    free (*item);
    *item = NULL;
}


//  --------------------------------------------------------------------------
/// Takes an id and tries to find the corresponding hashmap. Inserts
/// or updates the provided value.
static int
resolve (
    state_t *state,
    char *id,
    int difference)
{
    const char *delim = ".";
    char *tok = strtok (id, delim);

    if (tok) {

        // retrieve map
        zhash_t *map;

        if (!strcmp (tok, "sam")) {
            map = state->metrics.sam;
        }
        else if (!strcmp (tok, "samd")) {
            map = state->metrics.samd;
        }
        else {
            assert (false);
        }


        // insert/update item
        char *key = strtok (NULL, delim);
        uint64_t *target = zhash_lookup (map, key);
        if (!target) {
            target = malloc (sizeof (uint64_t));
            *target = 0;
            zhash_insert (map, key, target);
        }

        *target += difference;
        return 0;
    }

    return -1;
}


//  --------------------------------------------------------------------------
/// Callback function for activity on the pull socket.
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


//  --------------------------------------------------------------------------
/// Callback function for activity on the reply socket. Creates a
/// string containinig all currently available metrics.
int
handle_digest (
    zloop_t *loop UU,
    zsock_t *rep,
    void *arg)
{
    state_t *state = arg;

    sam_log_trace ("recv () digest request");
    zsock_recv (rep, "z");

    size_t str_size = 2048;
    char str [str_size];
    char *str_ptr = str;


    // gather metrics
    zhash_t *map_refs [] = {
        state->metrics.samd,
        state->metrics.sam,
        NULL
    };

    const char *map_names [] = {
        "samd",
        "sam",
        NULL
    };


    // iterate maps
    zhash_t **maps = map_refs;
    const char **names = map_names;

    while (*maps) {
        sprintf (str_ptr, "\n%s:\n", *names);
        str_ptr += strlen (str_ptr);

        char buf [str_size];
        memset (buf, 0, str_size);

        // build string from all items
        void *item = zhash_first (*maps);
        while (item) {
            const char *key = zhash_cursor (*maps);
            uint64_t val = *((uint64_t *) item);

            sprintf (str_ptr, "  %s: %" PRIu64 "\n", key, val);

            str_ptr += strlen (str_ptr);
            item = zhash_next (*maps);
        }

        maps += 1;
        names += 1;
    }


    sam_log_trace ("send () digest response");
    zsock_send (rep, "s", str);
    return 0;
}


//  --------------------------------------------------------------------------
/// The stat actor function. Initializes all necessary sockets and
/// starts the reactor.
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


    // initialize store
    state.metrics.sam = zhash_new ();
    zhash_set_destructor (state.metrics.sam, free_metric);

    state.metrics.samd = zhash_new ();
    zhash_set_destructor (state.metrics.samd, free_metric);


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

    zhash_destroy (&state.metrics.sam);
    zhash_destroy (&state.metrics.samd);
}



//  --------------------------------------------------------------------------
/// Create a new stat thread.
sam_stat_t *
sam_stat_new ()
{
    sam_stat_t *self = malloc (sizeof (sam_stat_t));
    assert (self);

    self->actor = zactor_new (actor, NULL);
    sam_log_info ("created metric aggregator");

    return self;
}


//  --------------------------------------------------------------------------
/// Destroy a stat thread.
void
sam_stat_destroy (
    sam_stat_t **self)
{
    assert (*self);

    zactor_destroy (&(*self)->actor);

    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
/// Create a handle to communicate with a stat thread.
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


//  --------------------------------------------------------------------------
/// Destroy a stat handle.
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


//  --------------------------------------------------------------------------
/// Function to update metrics. Invoked by using the preprocessor
/// macro defined in the header.
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


//  --------------------------------------------------------------------------
/// Retrieve a string representation of the current metrics. Invoked
/// by using the preprocessor macro defined in the header.
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
