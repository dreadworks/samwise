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

   Samwise is a store and forward service for messaging.
   TODO: throrough description when everything's running.

   <code>

   libsam | libsam actor
   ---------------------
     PIPE: libsam spawns its actor internally
     REQ/REP: synchronous requests (rpc, ctl)
     PSH/PLL: asynchronous requests (publish)

   libsam actor | be[i] actor
     REQ/REP: synchronous requests (rpc)
     PSH/PLL: asynchronous requests (publish)

   sam_buf_actor | libsam actor
     PSH/PLL: resending requests (publish)

   Topology:
   --------

               o  libsam  o
          REQ  ^    |     | PUSH
               |   PIPE   |
               v    |     v PULL
          REP  o    |     o
               libsam actor
         PULL  o          o o
              ^      PUSH |  ^ REQ
             /            |   \
      PUSH  o        PULL v    v REP
        sam_buf           o    o
         actor          be[i] actor

   <code>


*/


#include "../include/sam_prelude.h"



/// state used by the sam actor
typedef struct state_t {
    sam_be_t be_type;        ///< backend type, used to parse the protocol
    zsock_t *ctl_rep;        ///< reply socket for control commands
    zsock_t *frontend_rpc;   ///< reply socket for rpc requests
    zsock_t *frontend_pub;   ///< pull socket for publishing requests
    zlist_t *backends;       ///< maintains backend handles
} state_t;



//  --------------------------------------------------------------------------
/// Convenience function to create a new return object.
static sam_ret_t *
new_ret ()
{
    sam_ret_t *ret = malloc (sizeof (sam_ret_t));
    assert (ret);
    ret->rc = 0;
    return ret;
}


//  --------------------------------------------------------------------------
/// This removes a backend and all event listener from its signal socket.
static int
remove_backend (
    state_t *state,
    zloop_t *loop,
    const char *name)
{
    int rc = -1;
    sam_backend_t *be = zlist_first (state->backends);

    while (be != NULL) {
        if (!strcmp (name, be->name)) {
            sam_log_infof ("removing backend %s", name);

            zlist_remove (state->backends, be);
            zloop_reader_end (loop, be->sock_sig);

            sam_be_rmq_t *rabbit = sam_be_rmq_stop (&be);
            sam_be_rmq_destroy (&rabbit);
            rc = 0;
        }
    }

    return rc;
}



//  --------------------------------------------------------------------------
/// Handle signals from backends. Currently, the only existing signal
/// is a connection loss.
static int
handle_sig (
    zloop_t *loop,
    zsock_t *sig,
    void *args)
{
    state_t *state = args;

    int code;
    char *be_name;

    int rc = zsock_recv (sig, "is", &code, &be_name);
    if (rc) {
        sam_log_error ("could not receive signal");
        return rc;
    }

    sam_log_errorf ("got signal %d from '%s'!", code, be_name);
    assert (code == SAM_BE_SIG_CONNECTION_LOSS);

    rc = remove_backend (state, loop, be_name);
    free (be_name);

    return 0;
}


//  --------------------------------------------------------------------------
/// Publish a message to the backends.
static int
handle_frontend_pub (
    zloop_t *loop UU,
    zsock_t *pll,
    void *args)
{
    state_t *state = args;

    int key;
    sam_msg_t *msg;
    zframe_t *id_frame;

    sam_log_trace ("recv () frontend pub");
    zsock_recv (pll, "ifp", &key, &id_frame, &msg);

    // get mask containing already ack'd backends
    uint64_t be_acks = *(uint64_t *) zframe_data (id_frame);
    zframe_destroy (&id_frame);

    char *distribution;
    int rc = sam_msg_pop (msg, "s", &distribution);
    assert (!rc);

    int n = 1;
    if (!strcmp (distribution, "redundant")) {
        rc = sam_msg_pop (msg, "i", &n);
        assert (!rc);
    }

    int backend_c = zlist_size (state->backends);
    if (!backend_c) {
        sam_log_error ("discarding message, no backends available");
        sam_msg_destroy (&msg);
        return 0;
    }


    sam_log_tracef (
        "publish %s(%d), %d broker(s) available; 0x%" PRIx64 " ack'd",
        distribution, n, backend_c, be_acks);


    while (0 < n && 0 < backend_c) {
        n -= 1;
        backend_c -= 1;

        sam_backend_t *backend = zlist_next (state->backends);

        if (backend == NULL) {
            backend = zlist_first (state->backends);
        }

        // check that the backend not already ack'd the msg
        if (!(be_acks & backend->id)) {

            // the backend is trying to destroy it
            sam_msg_own (msg);
            sam_log_tracef (
                "send () message %d to '%s'",
                key, backend->name);

            zsock_send (backend->sock_pub, "ip", key, msg);
        }

        if (n && !backend_c) {
            sam_log_error (
                "discarding redundant msg, not enough backends available");
        }
    }

    sam_msg_destroy (&msg);
    return rc;
}


//  --------------------------------------------------------------------------
/// Callback for events on the internally used req/rep connection for
/// rpc requests. This function accepts messages crafted as defined by
/// the protocol to delegate them (based on the distribution method)
/// to various message backends.
static int
handle_frontend_rpc (
    zloop_t *loop UU,
    zsock_t *rep,
    void *args)
{
    state_t *state = args;
    sam_msg_t *msg;

    sam_log_trace ("recv () frontend rpc");
    zsock_recv (rep, "p", &msg);

    sam_ret_t *ret = new_ret ();

    char *broker;
    int rc = sam_msg_pop (msg, "s", &broker);
    // TODO: consider "broker"

    sam_backend_t *backend = zlist_first (state->backends);
    while (backend != NULL) {
        sam_log_tracef ("send () rpc req to '%s'", backend->name);
        rc = zsock_send (backend->sock_rpc, "p", msg);

        int status = -1;
        sam_log_tracef ("recv () reply from backend '%s'", backend->name);
        rc = zsock_recv (backend->sock_rpc, "i", &status);
        backend = zlist_next (state->backends);
    }

    sam_log_tracef ("send () ret (%d) for rpc internally", ret->rc);
    rc = zsock_send (rep, "p", ret);
    sam_msg_destroy (&msg);
    return rc;
}


//  --------------------------------------------------------------------------
/// Handle control commands like adding or removing backends.
static int
handle_ctl_req (
    zloop_t *loop,
    zsock_t *rep,
    void *args)
{
    state_t *state = args;
    int rc = 0;

    char *cmd;
    zmsg_t *zmsg = zmsg_new ();

    sam_log_trace ("recv () ctl request");
    zsock_recv (rep, "sm", &cmd, &zmsg);
    assert (cmd);

    sam_log_tracef ("got ctl command: '%s'", cmd);
    sam_msg_t *msg = sam_msg_new (&zmsg);

    // add a new backend
    if (!strcmp (cmd, "be.add")) {
        sam_backend_t *be;
        rc = sam_msg_pop (msg, "p", &be);
        if (!rc) {
            sam_log_infof ("inserting backend '%s'", be->name);
            rc = zlist_append (state->backends, be);
            zloop_reader (loop, be->sock_sig, handle_sig, state);
        }
    }

    // remove a backend
    else if (!strcmp (cmd, "be.rm")) {
        char *name;
        rc = sam_msg_pop (msg, "s", &name);
        if (!rc) {
            rc = remove_backend (state, loop, name);
        }
    }

    else {
        assert (false);
    }

    // clean up
    free (cmd);
    sam_msg_destroy (&msg);

    sam_log_tracef ("send () '%d' for ctl internally", rc);
    zsock_send (rep, "i", rc);
    return 0;
}



//  --------------------------------------------------------------------------
/// The thread encapsulated by sam instances. Starts a loop
/// listening to the various sockets to multiplex and demultiplex
/// requests to a backend pool.
static void
actor (
    zsock_t *pipe,
    void *args)
{
    state_t *state = args;
    zloop_t *loop = zloop_new ();

    // publishing and rpc calls to backends
    zloop_reader (loop, state->frontend_pub, handle_frontend_pub, state);
    zloop_reader (loop, state->frontend_rpc, handle_frontend_rpc, state);

    // internal channels for control commands
    zloop_reader (loop, state->ctl_rep, handle_ctl_req, state);
    zloop_reader (loop, pipe, sam_gen_handle_pipe, NULL);

    sam_log_info ("starting poll loop");
    zsock_signal (pipe, 0);
    zloop_start (loop);

    sam_log_trace ("destroying loop");
    zloop_destroy (&loop);

    zsock_destroy (&state->frontend_pub);
    zsock_destroy (&state->frontend_rpc);

    // destroy backends
    sam_backend_t *backend = zlist_first (state->backends);
    while (backend != NULL) {
        sam_log_tracef ("trying to delete backend '%s'", backend->name);

        // rabbitmq backends
        if (state->be_type == SAM_BE_RMQ) {
            sam_be_rmq_t *rabbit = sam_be_rmq_stop (&backend);
            sam_be_rmq_destroy (&rabbit);
        }

        else {
            assert (false);
        }

        backend = zlist_next (state->backends);
    }

    zlist_destroy (&state->backends);
    zsock_destroy (&state->ctl_rep);

    free (state);
}


//  --------------------------------------------------------------------------
/// Create a new sam instance. It initializes both the sam_t
/// object and the internally used state object.
sam_t *
sam_new (
    sam_be_t be_type)
{
    sam_t *self = malloc (sizeof (sam_t));
    state_t *state = malloc (sizeof (state_t));
    assert (self);

    self->be_id_power = 0;
    self->buf = NULL;


    // publishing requests
    self->frontend_pub_endpoint = "inproc://sam-pub";
    self->frontend_pub = zsock_new_push (self->frontend_pub_endpoint);
    state->frontend_pub = zsock_new_pull (self->frontend_pub_endpoint);

    assert (self->frontend_pub);
    assert (state->frontend_pub);
    sam_log_tracef (
        "created push/pull pair at '%s'", self->frontend_pub_endpoint);


    // rpc requests
    char *endpoint = "inproc://sam-rpc";

    self->frontend_rpc = zsock_new_req (endpoint);
    state->frontend_rpc = zsock_new_rep (endpoint);

    assert (self->frontend_rpc);
    assert (state->frontend_rpc);
    sam_log_tracef ("created req/rep pair at '%s'", endpoint);


    // actor control commands
    endpoint = "inproc://sam-ctl";
    self->ctl_req = zsock_new_req (endpoint);
    state->ctl_rep = zsock_new_rep (endpoint);

    assert (self->ctl_req);
    assert (state->ctl_rep);
    sam_log_tracef ("created req/rep pair at '%s'", endpoint);


    // backend pull, used by init_buf and when
    // creating new messaging backends
    self->backend_pull_endpoint = "inproc://sam-backend";


    // actor
    self->actor = zactor_new (actor, state);
    sam_log_info ("created msg instance");

    state->be_type = be_type;
    self->be_type = be_type;

    state->backends = zlist_new ();
    return self;
}


//  --------------------------------------------------------------------------
/// Destroy the sam_t instance. Also closes all sockets and free's
/// all memory of the internally used state object.
void
sam_destroy (
    sam_t **self)
{
    assert (*self);
    sam_log_info ("destroying sam instance");

    if ((*self)->buf) {
        sam_buf_destroy (&(*self)->buf);
    }

    zsock_destroy (&(*self)->frontend_pub);
    zsock_destroy (&(*self)->frontend_rpc);
    zsock_destroy (&(*self)->ctl_req);

    zactor_destroy (&(*self)->actor);

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

    sam_be_rmq_t *rabbit = sam_be_rmq_new (
        name, 1 << self->be_id_power);

    self->be_id_power += 1;
    assert (rabbit);

    int rc = sam_be_rmq_connect (rabbit, rabbit_opts);
    if (rc) {
        sam_be_rmq_destroy (&rabbit);
        return NULL;
    }

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
/// Create a new backend instance based on sam_be_t.
int
sam_be_remove (
    sam_t *self,
    const char *name)
{
    sam_log_infof ("send () 'be.rm' for '%s' internally", name);
    zsock_send (self->ctl_req, "ss", "be.rm", name);

    int rc = -1;
    sam_log_tracef ("recv () return code for be.rm for '%s'", name);
    zsock_recv (self->ctl_req, "i", &rc);
    return rc;
}


//  --------------------------------------------------------------------------
/// Create a new sam_buf instance based on sam_cfg.
static int
init_buf (
    sam_t *self,
    sam_cfg_t *cfg)
{
    if (self->buf) {
        sam_buf_destroy (&self->buf);
    }

    zsock_t *backend_pull = zsock_new_pull (self->backend_pull_endpoint);
    zsock_t *frontend_push = zsock_new_push (self->frontend_pub_endpoint);

    self->buf = sam_buf_new (cfg, &backend_pull, &frontend_push);

    if (self->buf == NULL) {
        zsock_destroy (&backend_pull);
        zsock_destroy (&frontend_push);
        return -1;
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Creates backend instances based on sam_cfg.
static int
init_backends (
    sam_t *self,
    sam_cfg_t *cfg)
{
    int count;
    char **names, **names_ptr;
    void *opts, *opts_ptr;

    int rc = sam_cfg_be_backends (
        cfg, self->be_type, &count, &names, &opts);

    names_ptr = names;
    opts_ptr = opts;

    if (rc || !count) {
        sam_log_error ("there are no backends to initialize");
        return -1;
    }

    while (count) {
        const char *name = *names_ptr;
        sam_backend_t *be = sam_be_create (self, name, opts_ptr);
        if (be != NULL) {
            sam_log_tracef ("send () 'be.add' to '%s'", name);
            zsock_send (self->ctl_req, "sp", "be.add", be);

            rc = -1;
            sam_log_tracef ("recv () for return code of 'be.add' for '%s'", name);
            zsock_recv (self->ctl_req, "i", &rc);
            if (rc) {
                sam_log_errorf (
                    "could not create backend %s", name);
            }

        }

        // advance pointers, maybe there is a more elegant way
        names_ptr += 1;
        if (self->be_type == SAM_BE_RMQ) {
            opts_ptr = (sam_be_rmq_opts_t *) opts_ptr + 1;
        }
        else {
            assert (false);
        }

        count -= 1;
    }

    free (names);
    free (opts);

    return rc;
}


//  --------------------------------------------------------------------------
/// Initialize backends and the store based on a samwise configuration file.
int
sam_init (
    sam_t *self,
    sam_cfg_t *cfg)
{
    assert (self);
    assert (cfg);

    int rc = init_buf (self, cfg);
    if (rc) {
        return rc;
    }

    rc = init_backends (self, cfg);
    if (rc) {
        return rc;
    }

    sam_log_info ("(re)loaded configuration");
    return 0;
}


//  --------------------------------------------------------------------------
/// Destroy the message and create a return object with an error message.
static sam_ret_t *
error (
    sam_msg_t *msg,
    char *error_msg)
{
    sam_msg_destroy (&msg);

    sam_ret_t *ret = new_ret ();
    ret->rc = -1;
    ret->msg = error_msg;

    return ret;
}


//  --------------------------------------------------------------------------
/// Checks if the protocol for the incoming RabbitMQ RPC message is obeyed.
static int
check_rpc_rmq (
    sam_msg_t *msg)
{
    char *type;
    int rc = sam_msg_get (msg, "?s", &type);
    if (rc == -1) {
        return rc;
    }

    if (!strcmp (type, "exchange.declare")) {
        rc = sam_msg_expect (
            msg, 4,
            SAM_MSG_ZERO,      // target broker
            SAM_MSG_NONZERO,   // type
            SAM_MSG_NONZERO,   // exchange name
            SAM_MSG_NONZERO);  // exchange type
    }

    else if (!strcmp (type, "exchange.delete")) {
        rc = sam_msg_expect (
            msg, 3,
            SAM_MSG_ZERO,      // target broker
            SAM_MSG_NONZERO,   // type
            SAM_MSG_NONZERO);  // exchange name
    }

    else {
        rc = -1;
    }

    free (type);
    return rc;
}


//  --------------------------------------------------------------------------
/// Checks if the rpc request conforms to the protocol.
static int
check_rpc (
    sam_be_t be_type,
    sam_msg_t *msg)
{
    int rc = 0;

    if (be_type == SAM_BE_RMQ) {
        rc = check_rpc_rmq (msg);
    }
    else {
        assert (false);
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// Checks if the protocol for the RabbitMQ publishing message is obeyed.
static int
check_pub_rmq (
    sam_msg_t *msg)
{
    char *distribution;
    int rc = sam_msg_get (msg, "s", &distribution);
    if (rc) {
        return rc;
    }

    if (!strcmp (distribution, "redundant")) {
        rc = sam_msg_expect (
            msg, 9,
            SAM_MSG_NONZERO,    // distribution
            SAM_MSG_NONZERO,    // min. acknowledged

            // amqp args
            SAM_MSG_NONZERO,    // exchange
            SAM_MSG_ZERO,       // routing key
            SAM_MSG_ZERO,       // mandatory
            SAM_MSG_ZERO,       // immediate

            // variable args
            SAM_MSG_LIST,       // amqp properties
            SAM_MSG_LIST,       // amqp headers

            SAM_MSG_NONZERO);   // payload
    }
    else if (!strcmp (distribution, "round robin")) {
        rc = sam_msg_expect (
            msg, 8,
            SAM_MSG_NONZERO,    // distribution
            SAM_MSG_NONZERO,    // exchange

            // amqp args
            SAM_MSG_ZERO,       // routing key
            SAM_MSG_ZERO,       // mandatory
            SAM_MSG_ZERO,       // immediate

            // variable args
            SAM_MSG_LIST,       // amqp properties
            SAM_MSG_LIST,       // amqp headers

            SAM_MSG_NONZERO);   // payload
    }
    else {
        rc = -1;
    }

    free (distribution);
    return rc;
}


//  --------------------------------------------------------------------------
/// Checks if the message contains a valid publishing request.
static int
check_pub (
    sam_be_t be_type,
    sam_msg_t *msg)
{
    if (be_type == SAM_BE_RMQ) {
        return check_pub_rmq (msg);
    }

    assert (false);
}


//  --------------------------------------------------------------------------
/// Send the sam actor thread a message.
sam_ret_t *
sam_eval (
    sam_t *self,
    sam_msg_t *msg)
{
    assert (self);
    assert (msg);

    char *action;
    int rc = sam_msg_pop (msg, "s", &action);
    if (rc) {
        return error (msg, "action required");
    }

    // publish, synchronous store, asynchronous distribution
    sam_log_tracef ("checking '%s' request", action);
    if (!strcmp (action, "publish")) {
        rc = check_pub (self->be_type, msg);
        if (rc) {
            return error (msg, "malformed publishing request");
        }

        // Create a copy of the message and pass that over to the
        // store. Crafting a copy is necessary because the actor
        // starts chewing up the message which is not thread safe. If
        // the store is going to act completely synchronously, this
        // operation may become obsolete...
        sam_msg_t *dup = sam_msg_dup (msg);
        int key = sam_buf_save (self->buf, dup);

        // pass the message on for distribution, 0 backends ack'd already
        uint64_t be_acks = 0;
        zframe_t *id_frame = zframe_new (&be_acks, sizeof (be_acks));

        sam_log_tracef ("send () message '%d' internally", key);
        zsock_send (self->frontend_pub, "ifp", key, id_frame, msg);

        zframe_destroy (&id_frame);
        return new_ret ();
    }

    // rpc, synchronous
    else if (!strcmp (action, "rpc")) {
        rc = check_rpc (self->be_type, msg);
        if (rc) {
            return error (msg, "malformed rpc request");
        }

        sam_ret_t *ret;
        sam_log_trace ("send () rpc internally");
        zsock_send (self->frontend_rpc, "p", msg);
        sam_log_trace ("recv () rpc internally");
        zsock_recv (self->frontend_rpc, "p", &ret);
        return ret;
    }

    // ping
    else if (!strcmp (action, "ping")) {
        sam_msg_destroy (&msg);
        goto suspend;

    // stop
    } else if (!strcmp (action, "stop")) {
        raise (SIGINT);
        goto suspend;

    // restart
    } else if (!strcmp (action, "restart")) {
        sam_ret_t *ret = new_ret ();
        ret->rc = SAM_RET_RESTART;
        sam_msg_destroy (&msg);
        return ret;
    }

    return error (msg, "unknown action");

suspend:
    sam_msg_destroy (&msg);
    return new_ret ();
}
