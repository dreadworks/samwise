/*  =========================================================================

    samwise - reliable message publishing

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT


    =========================================================================
*/
/**

   @brief API interface for sam

   TODO: description

*/

#ifndef __SAM_H__
#define __SAM_H__


#define SAM_VERSION_MAJOR 0
#define SAM_VERSION_MINOR 0
#define SAM_VERSION_PATCH 1

#define SAM_MAKE_VERSION(major, minor, patch) \
    ((major) *10000 + (minor) * 100 + (patch))

#define SAM_VERSION \
    SAM_MAKE_VERSION(SAM_VERSION_MAJOR, \
                     SAM_VERSION_MINOR, \
                     SAM_VERSION_PATCH)


/// response types
typedef enum {
    SAM_RES_ACK,                ///< acknowledgement from be[i]
    SAM_RES_CONNECTION_LOSS     ///< connection loss registered in be[i]
} sam_res_t;


/// backend types
typedef enum {
    SAM_BE_RABBITMQ     ///< RabbitMQ message backend
} sam_be_t;


/// return type for client responses
typedef struct sam_ret_t {
    int rc;    ///< return code
    char *msg; ///< set for sam_ret_t.rc != 0
} sam_ret_t;

/// state used by the sam actor
typedef struct sam_state_t {
    zsock_t *actor_rep;      ///< reply socket for the internal actor
    zsock_t *backend_pull;   ///< back channel for backend acknowledgments
    sam_backend_t *backend;  ///< reference to a backend (TODO #44)
} sam_state_t;


/// a sam instance
typedef struct sam_t {
    zsock_t *actor_req;           ///< request socket for the internal actor
    char *backend_pull_endpoint;  ///< pull endpoint name for backends to bind
    zactor_t *actor;              ///< thread maintaining broker connections
    sam_state_t *state;           ///< ref to the state object for destroy()
} sam_t;


//  --------------------------------------------------------------------------
/// @brief Creates a new sam instance
/// @return Handle for inter thread communication
sam_t *
sam_new ();


//  --------------------------------------------------------------------------
/// @brief Destroy an instance, tears down the thread and free's all memory
void
sam_destroy (
    sam_t **self);


//  --------------------------------------------------------------------------
/// @brief Create a new message backend
/// @param self A sam instance
/// @param be_type Type of the backend to create
/// @return The calculated delay in ms or -1 in case of error
int
sam_be_create (
    sam_t *self,
    sam_be_t be_type,
    void *opts);


//  --------------------------------------------------------------------------
/// @brief (Re)load configuration file
/// @param conf Location of the configuration file
/// @return 0 in case of success, -1 in case of error
int
sam_init (
    sam_t *self,
    const char *conf);


//  --------------------------------------------------------------------------
/// @brief Instruct sam to analyze and act according to a message
/// @param self A sam instance
/// @param msg Message containing some <action>
/// @return Some sam_ret_t
sam_ret_t *
sam_send_action (
    sam_t *self,
    sam_msg_t **msg);


//  --------------------------------------------------------------------------
/// @brief Self test this class.
void
sam_test ();


#endif
