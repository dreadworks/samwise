/*  =========================================================================

    sam_msg - message backend maintainer

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT


    =========================================================================
*/
/**

   @brief message backend maintainer

   TODO description

*/

#ifndef __SAM_MSG_H__
#define __SAM_MSG_H__


/// response types
typedef enum {
    SAM_MSG_RES_ACK,
    SAM_MSG_RES_CONNECTION_LOSS
} sam_msg_res_t;


typedef enum {
    SAM_MSG_BE_RABBITMQ
} sam_msg_be_t;


/// return type for "start" functions
/// of different message backends
typedef struct sam_msg_backend_t {
    void *self;
    zsock_t *req;
    zactor_t *actor;
} sam_msg_backend_t;


typedef struct sam_msg_state_t {
    unsigned int backend_c;
    zsock_t *actor_rep;
    zsock_t *backend_pull;
    sam_msg_backend_t *backend;
} sam_msg_state_t;


typedef struct sam_msg_t {
    zsock_t *actor_req;
    char *backend_pull_endpoint;
    zactor_t *actor;
    sam_msg_state_t *state;
} sam_msg_t;


//  --------------------------------------------------------------------------
/// @brief Creates a new sam_msg instance
/// @param name Messaging name, max. ~50 chars
/// @return Handle for inter thread communication
sam_msg_t *
sam_msg_new (
    const char *name);


//  --------------------------------------------------------------------------
/// @brief Save and publish a message
void
sam_msg_destroy (
    sam_msg_t **self);


//  --------------------------------------------------------------------------
/// @brief Save and publish a message
/// @param self A sam_msg instance
/// @param msg Message containing distribution method and payload
/// @return The calculated delay in ms or -1 in case of error
int
sam_msg_create_backend (
    sam_msg_t *self,
    sam_msg_be_t be_type,
    void *opts);


//  --------------------------------------------------------------------------
/// @brief Save and publish a message
/// @param self A sam_msg instance
/// @param msg Message containing distribution method and payload
/// @return The calculated delay in ms or -1 in case of error
int
sam_msg_publish (
    sam_msg_t *self,
    zmsg_t *msg);


//  --------------------------------------------------------------------------
/// @brief Self test this class
void
sam_msg_test ();


#endif
