/*  =========================================================================

    samd - send all message daemon (for samwise)

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief daemon process to accept publishing requests
   @file samd.c

   TODO description

*/


#include "../include/sam_prelude.h"
#include "../include/samd.h"


static sam_ret_t *
create_error (char *msg)
{
    sam_ret_t *ret = malloc (sizeof (sam_ret_t));
    ret->rc = -1;
    ret->msg = msg;
    return ret;
}



//  --------------------------------------------------------------------------
/// Handle external publishing/rpc requests. Checks the protocol
/// number to decide if libsam can handle it and then either delegates
/// or rejects the message.
static int
handle_req (zloop_t *loop UU, zsock_t *client_rep, void *args)
{
    samd_t *self = args;
    sam_ret_t *ret;

    zmsg_t *zmsg = zmsg_new ();
    int version = -1;

    zsock_recv (client_rep, "im", &version, &zmsg);
    if (version == -1) {
        ret = create_error ("malformed request");
        zmsg_destroy (&zmsg);
    }

    else if (version != SAM_PROTOCOL_VERSION) {
        ret = create_error ("wrong protocol version");
        zmsg_destroy (&zmsg);
    }

    else if (zmsg_size (zmsg) < 1) {
        ret = create_error ("no payload");
        zmsg_destroy (&zmsg);
    }

    else {
        sam_msg_t *msg = sam_msg_new (&zmsg);
        ret = sam_send_action (self->sam, &msg);
        assert (!msg);
    }

    if (!ret->rc) {
        sam_log_trace ("success, sending reply to client");
        zsock_send (client_rep, "i", 0);
    }

    else {
        sam_log_trace ("error, sending reply to client");
        zsock_send (client_rep, "is", ret->rc, ret->msg);
    }

    free (ret);
    return 0;
}


//  --------------------------------------------------------------------------
/// Creates a new samd instance and binds the public endpoints socket.
samd_t *
samd_new (const char *endpoint)
{
    samd_t *self = malloc (sizeof (samd_t));
    assert (self);

    self->sam = sam_new ();
    assert (self->sam);

    self->client_rep = zsock_new_rep (endpoint);
    assert (self->client_rep);

    sam_init (self->sam, NULL);
    sam_log_info ("created samd");
    return self;
}


//  --------------------------------------------------------------------------
/// Destroys the samd instance and free's all allocated memory.
void
samd_destroy (samd_t **self)
{
    sam_log_info ("destroying samd");

    zsock_destroy (&(*self)->client_rep);
    sam_destroy (&(*self)->sam);

    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
/// Start a blocking loop for client requests.
void
samd_start (samd_t *self)
{
    zloop_t *loop = zloop_new ();
    zloop_reader (loop, self->client_rep, handle_req, self);
    zloop_start (loop);
    zloop_destroy (&loop);
    sam_log_info ("leaving main loop");
}


//  --------------------------------------------------------------------------
/// Self test the daemon.
static void
test_actor (zsock_t *pipe, void *args UU)
{
    samd_t *samd = samd_new (SAM_PUBLIC_ENDPOINT);
    zloop_t *loop = zloop_new ();

    zloop_reader (loop, samd->client_rep, handle_req, samd);
    zloop_reader (loop, pipe, sam_gen_handle_pipe, NULL);

    zsock_signal (pipe, 0);
    zloop_start (loop);

    zloop_destroy (&loop);
    samd_destroy (&samd);
}


static void
assert_error (zmsg_t **msg)
{
    assert (zmsg_size (*msg) == 2);
    assert (zmsg_popint (*msg) == -1);

    char *error_msg = zmsg_popstr (*msg);
    sam_log_tracef ("got error: %s", error_msg);

    free (error_msg);
    zmsg_destroy (msg);
}



//  --------------------------------------------------------------------------
/// Self test the daemon.
void
samd_test ()
{
    printf ("\n** SAMD **\n");
    zactor_t *actor = zactor_new (test_actor, NULL);
    zsock_t *req = zsock_new_req (SAM_PUBLIC_ENDPOINT);

    // ping
    zsock_send (req, "is", SAM_PROTOCOL_VERSION, "ping");
    zmsg_t *msg = zmsg_recv (req);
    assert (zmsg_size (msg) == 1);
    assert (zmsg_popint (msg) == 0);
    zmsg_destroy (&msg);

    // wrong protocol version
    zsock_send (req, "is", 0, "ping");
    msg = zmsg_recv (req);
    assert_error (&msg);

    // malformed request
    zsock_send (req, "z");
    msg = zmsg_recv (req);
    assert_error (&msg);

    // tear down
    zactor_destroy (&actor);
    zsock_destroy (&req);
}


#ifndef __SAM_TEST
//  --------------------------------------------------------------------------
/// Main entry point, initializes and starts samd.
int
main ()
{
    samd_t *samd = samd_new (SAM_PUBLIC_ENDPOINT);
    samd_start (samd);
    samd_destroy (&samd);
    sam_log_info ("exiting");
}
#endif
