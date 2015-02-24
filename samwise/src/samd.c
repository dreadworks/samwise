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


#include <czmq.h>
#include "../include/sam_prelude.h"
#include "../include/samd.h"


static void
send_error (zsock_t *client_rep, char *reason)
{
    zsock_send (client_rep, "is", -1, reason);
}


//  --------------------------------------------------------------------------
/// Handle external publishing/rpc requests. Checks the protocol
/// number to decide if libsam can handle it and then either delegates
/// or rejects the message.
static int
handle_req (zloop_t *loop UU, zsock_t *client_rep, void *args UU)
{
    zmsg_t *msg = zmsg_recv (client_rep);
    zmsg_destroy (&msg);

    zsock_send (client_rep, "i", 0);
    return 0;

    // samd_t *self = args;
    /* zmsg_t *msg = zmsg_recv (client_rep); */
    /* sam_log_trace ("received message on public reply socket"); */

    /* int version = zmsg_popint (msg); */
    /* char *action = (char *) zframe_data (zmsg_first (msg)); */

    /* // check protocol version */
    /* if (version != SAM_PROTOCOL_VERSION) { */
    /*     send_error (client_rep, "unsupported version"); */
    /* } */

    /* // check action frame */
    /* else if (!action) { */
    /*     send_error (client_rep, "malformed request"); */
    /* } */

    /* else { */
    /*     int rc = 0; //sam_handle (self->sam, msg); */
    /*     if (rc) { */
    /*         send_error (client_rep, "request failed"); */
    /*     } */
    /*     else { */
    /*         zsock_send (client_rep, "i", 0); */
    /*     } */
    /* } */

    /* free (action); */
    /* return 0; */
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
/// Main entry point, initializes and starts samd.
int
main ()
{
    samd_t *samd = samd_new (SAM_PUBLIC_ENDPOINT);
    samd_start (samd);
    samd_destroy (&samd);
    sam_log_info ("exiting");
}
