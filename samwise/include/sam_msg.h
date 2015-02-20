/*  =========================================================================

    sam_msg - message backend maintainer

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT


    =========================================================================
*/
/**

   @brief message backend maintainer

                   other
               REQ ^   |
                   |   | PIPE
               REP o   |
          REQ > sam_msg[i] < REQ
             /  /    o    \  \
            /  /    PULL   \  \
           / PIPE  /    \ PIPE \
      REP o  /    /      \   \  o REP
   sam_msg_backend[0]   sam_msg_backend[n]
            |                |
          broker[0]       broker[n]

   This class maintains different message backends. It encapsulates a
   thread that listens for publishing or rpc requests on an internal
   REQ/REP connection. It then handles distribution (round
   robin/redundant (TODO #42)) and memoizes non-acknowledged messages
   (TODO #12). It resends messages after a period of time, deletes
   them based on a TTL value (TODO #43). It pushes data to the
   different backends by using a REQ/REP connection and receives
   acknowledgements on one central PULL socket.

   TODO persistence #11

*/

#ifndef __SAM_MSG_H__
#define __SAM_MSG_H__


/// response types
typedef enum {
    SAM_MSG_RES_ACK,                ///< acknowledgement from be[i]
    SAM_MSG_RES_CONNECTION_LOSS     ///< connection loss registered in be[i]
} sam_msg_res_t;


/// backend types
typedef enum {
    SAM_MSG_BE_RABBITMQ     ///< RabbitMQ message backend
} sam_msg_be_t;


/// return type for "start" functions
/// of different message backends
typedef struct sam_msg_backend_t {
    void *self;         ///< reference used by stop() give back control
    zsock_t *req;       ///< request channel to the backend
    zactor_t *actor;    ///< thread handling the broker connection
} sam_msg_backend_t;


/// state used by the sam_msg actor
typedef struct sam_msg_state_t {
    zsock_t *actor_rep;          ///< reply socket for the internal actor
    zsock_t *backend_pull;       ///< back channel for backend acknowledgments
    sam_msg_backend_t *backend;  ///< reference to a backend (TODO #44)
} sam_msg_state_t;


/// a sam_msg instance
typedef struct sam_msg_t {
    zsock_t *actor_req;           ///< request socket for the internal actor
    char *backend_pull_endpoint;  ///< pull endpoint name for backends to bind
    zactor_t *actor;              ///< thread maintaining broker connections
    sam_msg_state_t *state;       ///< ref to the state object for destroy()
} sam_msg_t;


//  --------------------------------------------------------------------------
/// @brief Creates a new sam_msg instance
/// @param name Messaging name, max. ~50 chars | TODO check the need for this
/// @return Handle for inter thread communication
sam_msg_t *
sam_msg_new (
    const char *name);


//  --------------------------------------------------------------------------
/// @brief Destroy an instance, tears down the thread and free's all memory
void
sam_msg_destroy (
    sam_msg_t **self);


//  --------------------------------------------------------------------------
/// @brief Create a new message backend
/// @param self A sam_msg instance
/// @param be_type Type of the backend to create
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
