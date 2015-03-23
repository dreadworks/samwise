/*  =========================================================================

    sam_gen - generic helper functions

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief daemon process to accept publishing requests
   @file samd.c

   Gather all functions commonly used by multiple modules.

*/


#include "../include/sam_prelude.h"


//  --------------------------------------------------------------------------
/// Generic function to handle $TERM on actor pipes and interrupts for
/// zactors.
int
sam_gen_handle_pipe (
    zloop_t *loop UU,
    zsock_t *pipe,
    void *args UU)
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
