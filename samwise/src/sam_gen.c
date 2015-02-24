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


//  --------------------------------------------------------------------------
/// Generic function to handle $TERM on actor pipes and interrupts for
/// zactors.
int
sam_gen_handle_pipe (zloop_t *loop UU, zsock_t *pipe, void *args UU)
{
    zmsg_t *msg = zmsg_recv (pipe);

    if (!msg) {
        sam_log_trace ("got interrupted");
        return -1;
    }

    bool term = false;
    char *cmd = zmsg_popstr (msg);
    if (!strcmp (cmd, "$TERM")) {
        sam_log_trace ("got terminated");
        term = true;
    }

    free (cmd);
    zmsg_destroy (&msg);

    if (term) {
        return -1;
    }

    return 0;
}



static void
test_actor (zsock_t *pipe, void *args UU)
{
    zloop_t *loop = zloop_new ();
    zloop_reader (loop, pipe, sam_gen_handle_pipe, NULL);
    zsock_signal (pipe, 0);
    zloop_start (loop);
    zloop_destroy (&loop);
}


//  --------------------------------------------------------------------------
/// Self test this file.
void
sam_gen_test ()
{
    printf ("\n** SAM GEN **\n");
    zactor_t *actor = zactor_new (test_actor, NULL);
    assert (actor);

    zactor_destroy (&actor);
    assert (!actor);
}

