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
        ret = sam_send_action (self->sam, msg);
    }

    if (!ret->rc) {
        sam_log_trace ("success, sending reply to client");
        zsock_send (client_rep, "i", 0);
    }

    else {
        sam_log_tracef ("error, sending reply to client (%s)", ret->msg);
        zsock_send (client_rep, "is", ret->rc, ret->msg);
    }

    free (ret);
    return 0;
}


//  --------------------------------------------------------------------------
/// Creates a new samd instance and binds the public endpoints socket.
samd_t *
samd_new (const char *cfg_file UU)
{
    samd_t *self = malloc (sizeof (samd_t));
    assert (self);

    sam_cfg_t *cfg = sam_cfg_new (cfg_file);
    sam_cfg_destroy (&cfg); // TO BE REMOVED

    sam_log_info ("created samd");
    return self;

/*
    self->sam = sam_new (SAM_BE_RMQ);
    assert (self->sam);

    self->client_rep = zsock_new_rep (endpoint);
    assert (self->client_rep);

    sam_init (self->sam, NULL);
*/
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


#ifndef __SAM_TEST
//  --------------------------------------------------------------------------
/// Main entry point, initializes and starts samd.
int
main (int argc, char **argv)
{
    argc -= 1;
    argv += 1;

    if (!argc) {
        printf ("samd - samwise messaging daemon\n");
        printf ("usage: samd path/to/config.cfg\n\n");
        return 2;
    }

    samd_t *samd = samd_new (*argv);
    samd_start (samd);
    samd_destroy (&samd);
    sam_log_info ("exiting");
}
#endif
