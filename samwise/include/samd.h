/*  =========================================================================

    samd - send all message daemon (for samwise)

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT


    =========================================================================
*/
/**

   @brief daemon process to accept publishing requests

   This file offers a possible implementation of libsam as a daemon.
   It offers a public endpoint for clients to connect to and send
   publishing or rpc requests.

*/

#ifndef __SAMD_H__
#define __SAMD_H__


/// samd instance object
typedef struct samd_t {
    sam_t *sam;            ///< encapsulates a sam thread
    zsock_t *client_rep;   ///< REPLY socket for client requests
} samd_t;


//  --------------------------------------------------------------------------
/// @brief Create a new samd daemon instance
/// @param endpoint Endpoint name for client requests (TCP/IPC)
/// @return A new samd instance
samd_t *
samd_new (const char *endpoint);


//  --------------------------------------------------------------------------
/// @brief Destroy a samd daemon instance
/// @param self To be deleted instance
void
samd_destroy (samd_t **self);


//  --------------------------------------------------------------------------
/// @brief Start a (blocking) event loop listening to client requests
void
samd_start (samd_t *self);


#endif
