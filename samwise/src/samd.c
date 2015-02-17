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
handle_req (zloop_t *loop UU, zsock_t *router, void *args)
{
    sam_t *sam = args;
    zmsg_t *msg = zmsg_recv (router);
    sam_log_trace (sam->logger, "received message on router socket");

    int delay = sam_publish (sam, msg);

    // sleep and return to client
    zclock_sleep (delay);
    zsock_signal (router, 0);

    return 0;
}


int main ()
{
    sam_logger_t *logger = sam_logger_new ("samd", SAM_LOG_ENDPOINT);

    sam_t *sam = sam_new ();
    assert (sam);

    zsock_t *router = zsock_new_router (SAM_PUBLIC_ENDPOINT);
    assert (router);

    zloop_t *loop = zloop_new ();
    zloop_reader (loop, router, handle_req, sam);

    sam_log_info (logger, "starting main event loop");
    zloop_start (loop);

    sam_log_info (logger, "exiting");
    zloop_destroy (&loop);
    zsock_destroy (&router);
    sam_logger_destroy (&logger);

    zclock_sleep (100);
    sam_destroy (&sam);
}

