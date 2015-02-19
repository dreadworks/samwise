/*  =========================================================================

    samd - send all message daemon (for samwise)

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT


    =========================================================================
*/
/**

   @brief daemon process to accept publishing requests

   TODO: description

*/

#ifndef __SAMD_H__
#define __SAMD_H__


#define SAMD_DEALER_ENDPOINT "inproc://samd"


typedef struct samd_t {
    sam_t *sam;
    sam_logger_t *logger;
    zsock_t *client_rep;
} samd_t;


samd_t *
samd_new (const char *endpoint);


void
samd_destroy (samd_t **self);


void
samd_start (samd_t *self);


#endif
