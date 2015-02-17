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


int
sam_gen_handle_pipe (zloop_t *loop UU, zsock_t *pipe, void *args UU)
{
    zmsg_t *msg = zmsg_recv (pipe);

    if (!msg) {
        return -1;
    }

    bool term = false;
    char *cmd = zmsg_popstr (msg);
    if (!strcmp (cmd, "$TERM")) {
        term = true;
    }

    free (cmd);
    zmsg_destroy (&msg);

    if (term) {
        return -1;
    }

    return 0;
}