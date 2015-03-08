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


static int
prepare_publish (sam_be_t be_type, sam_msg_t *msg)
{
    if (be_type == SAM_BE_RMQ) {
        return prepare_publish_rmq (msg);
    }

    assert (false);
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

    // check if the message is correctly formatted
    // and contain necessary data
    rc = prepare_publish (state->be_type, msg);
    if (rc) {
        return send_error (state->frontend_rep, ret, "malformed request");
    }

    sam_backend_t *backend = zlist_next (state->backends);
    if (backend == NULL) {
        backend = zlist_first (state->backends);
    }

    if (backend != NULL) {
        // all necessary information is now "contained"
        sam_log_tracef ("send publishing request to '%s'", backend->name);
        sam_msg_own (msg); // the backend is trying to destroy it
        zsock_send (backend->publish_psh, "p", msg);
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


static int
prepare_rpc (sam_be_t be_type, sam_msg_t *msg)
{
    if (be_type == SAM_BE_RMQ) {
        return prepare_rpc_rmq (msg);
    }

    assert (false);
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

    rc = prepare_rpc (state->be_type, msg);
    if (rc) {
        return send_error (
            state->frontend_rep, ret, "malformed request");
    }

    sam_backend_t *backend = zlist_first (state->backends);
    while (backend != NULL) {
        rc = zsock_send (backend->rpc_req, "p", msg);
        if (rc) {
            sam_log_error ("send failed");
            return rc;
        }

        int status = -1;
        rc = zsock_recv (backend->rpc_req, "i", &status);
        if (rc) {
            sam_log_error ("recv failed");
            return rc;
        }

        backend = zlist_next (state->backends);
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
    int rc = 0;

    char *cmd;
    zmsg_t *zmsg = zmsg_new ();

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
        }
    }

    // remove a backend
    else if (!strcmp (cmd, "be.rm")) {
        char *name;
        rc = sam_msg_pop (msg, "s", &name);
        if (!rc) {

            rc = -1;
            sam_backend_t *be = zlist_first (state->backends);
            while (be != NULL) {
                if (!strcmp (name, be->name)) {
                    sam_log_infof ("removing backend %s", name);
                    zlist_remove (state->backends, be);
                    sam_be_rmq_t *rabbit = sam_be_rmq_stop (&be);
                    sam_be_rmq_destroy (&rabbit);
                    rc = 0;
                }
            }
        }
    }

    else {
        assert (false);
    }

    // clean up
    free (cmd);
    sam_msg_destroy (&msg);

    zsock_send (rep, "i", rc);
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

    state->backends = zlist_new ();
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
    sam_backend_t *backend = zlist_first (state->backends);
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

        backend = zlist_next (state->backends);
    }

    zlist_destroy (&state->backends);

    // destroy sockets
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
/// Create a new backend instance based on sam_be_t.
int
sam_be_remove (
    sam_t *self,
    const char *name)
{
    sam_log_infof ("removing backend '%s'", name);
    zsock_send (self->ctl_req, "ss", "be.rm", name);

    int rc = -1;
    zsock_recv (self->ctl_req, "i", &rc);
    return rc;
}


//  --------------------------------------------------------------------------
/// Initialize backends based on a samwise configuration file.
int
sam_init (sam_t *self, sam_cfg_t *cfg)
{
    int count;
    char **names, **names_ptr;
    void *opts, *opts_ptr;

    int rc = sam_cfg_backends (
        cfg, self->be_type, &count, &names, &opts);

    names_ptr = names;
    opts_ptr = opts;

    if (rc || !count) {
        return -1;
    }

    sam_log_infof ("initializing %d backends", count);

    while (count) {
        const char *name = *names_ptr;
        sam_backend_t *be = sam_be_create (self, name, opts_ptr);
        zsock_send (self->ctl_req, "sp", "be.add", be);

        rc = -1;
        zsock_recv (self->ctl_req, "i", &rc);
        if (rc) {
            sam_log_errorf (
                "could not create backend %s", name);
        }

        // advance pointers, maybe there is a more elegant way
        names_ptr += 1;
        if (self->be_type == SAM_BE_RMQ) {
            opts_ptr = (sam_be_rmq_opts_t *) opts_ptr + 1;
        }

        count -= 1;
    }

    free (names);
    free (opts);

    sam_log_info ("(re)loaded configuration");
    return 0;
}


//  --------------------------------------------------------------------------
/// Send the sam actor thread a message.
sam_ret_t *
sam_send_action (sam_t *self, sam_msg_t *msg)
{
    sam_ret_t *ret;
    if (sam_msg_size (msg) < 1) {
        ret = malloc (sizeof (sam_ret_t));
        ret->rc = -1;
        ret->msg = "malformed request: action required";
    }
    else {
        sam_log_trace ("send on frontends req socket");
        zsock_send (self->frontend_req, "p", msg);

        sam_log_trace ("recv on frontends req socket");
        zsock_recv (self->frontend_req, "p", &ret);
        sam_log_tracef ("frontends req socket read, code: %d", ret->rc);
    }

    sam_msg_destroy (&msg);
    return ret;
}
