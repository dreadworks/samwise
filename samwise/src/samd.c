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
#include "../include/sam.h"
#include "../include/samd.h"


static int
handle_req (zloop_t *loop UU, zsock_t *client_rep, void *args)
{
    samd_t *self = args;
    zmsg_t *msg = zmsg_recv (client_rep);
    zmsg_destroy (&msg);

    sam_log_trace ("received message on router socket");

    // int rc = sam_publish (self->sam, msg);
    int rc = 0;
    zsock_send (client_rep, "i", rc);
    return 0;
}


samd_t *
samd_new (const char *endpoint)
{
    samd_t *self = malloc (sizeof (samd_t));
    assert (self);

    self->sam = sam_new ();
    assert (self->sam);

    self->client_rep = zsock_new_rep (endpoint);
    assert (self->client_rep);

    sam_log_info ("created samd");
    return self;
}


void
samd_destroy (samd_t **self)
{
    sam_log_info ("destroying samd");

    zsock_destroy (&(*self)->client_rep);
    sam_destroy (&(*self)->sam);

    free (*self);
    *self = NULL;
}


void
samd_start (samd_t *self)
{
    zloop_t *loop = zloop_new ();
    zloop_reader (loop, self->client_rep, handle_req, self);
    zloop_start (loop);
    printf ("EXIT MAIN LOOP\n");
}


int main ()
{
    samd_t *samd = samd_new (SAM_PUBLIC_ENDPOINT);
    samd_start (samd);
    samd_destroy (&samd);
    printf ("EXIT MAIN FUNCTION\n");
    sleep (20);
}
