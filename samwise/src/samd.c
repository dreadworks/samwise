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


static int
handle_req (zloop_t *loop UU, zsock_t *rep, void *args)
{
    sam_t *sam = args;
    zmsg_t *msg = zmsg_recv (rep);
    int delay = sam_publish (sam, msg);

    // sleep and return to client
    zclock_sleep (delay);
    zsock_signal (rep, 0);

    return 0;
}


int main ()
{
    sam_t *sam = sam_new ();
    assert (sam);

    zsock_t *rep = zsock_new_rep (SAM_PUBLIC_ENDPOINT);
    assert (rep);

    zloop_t *loop = zloop_new ();
    zloop_reader (loop, rep, handle_req, sam);
    zloop_start (loop);

    zloop_destroy (&loop);
    zsock_destroy (&rep);
    sam_destroy (&sam);
}

