/*  =========================================================================

    samwise - reliable message publishing

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT


    =========================================================================
*/
/**

   @brief Samwise API interface

   Offers all public functions necessary to store and forward
   messages.

*/

#ifndef __SAM_H__
#define __SAM_H__

#ifdef __cplusplus
extern "C" {
#endif


#define SAM_VERSION_MAJOR 0
#define SAM_VERSION_MINOR 0
#define SAM_VERSION_PATCH 1

#define SAM_MAKE_VERSION(major, minor, patch) \
    ((major) *10000 + (minor) * 100 + (patch))

#define SAM_VERSION \
    SAM_MAKE_VERSION(SAM_VERSION_MAJOR, \
                     SAM_VERSION_MINOR, \
                     SAM_VERSION_PATCH)



//
//   DEPENDENCIES
//

#ifdef __SAM_TEST
#include <check.h>
#endif

#include <db.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>

#include <czmq.h>
#if CZMQ_VERSION < 30000
#  error "sam needs at least CZMQ 3.0.0"
#endif


//
//   MACROS
//
#define UU __attribute__((unused))

// global configuration
#define SAM_PROTOCOL_VERSION 120
#define SAM_RET_RESTART 0x10

// enable stats
#define SAM_STAT

// don't define these to show all log levels
// TODO offer an option for ./configure
// #define LOG_THRESHOLD_TRACE   // show info + error
// #define LOG_THRESHOLD_INFO    // only show error
// #define LOG_THRESHOLD_ERROR   // disable logging



// forward references
typedef struct sam_t sam_t;
typedef struct sam_cfg_t sam_cfg_t;
typedef struct sam_msg_t sam_msg_t;


/// backend types
typedef enum {
    SAM_BE_RMQ     ///< RabbitMQ message backend
} sam_be_t;


/// signals sent by messaging backends
typedef enum {
    SAM_BE_SIG_CONNECTION_LOSS = 0x10, ///< if a backend was split
    SAM_BE_SIG_RECONNECTED,            ///< if the backend re-connected
    SAM_BE_SIG_KILL                    ///< backend no longer re-connects
} sam_be_sig_t;


/// return type for client responses
typedef struct sam_ret_t {
    int rc;          ///< return code
    char *msg;       ///< additional info, either static or allocated
    bool allocated;  ///< indicates if ret.msg must be free'd
} sam_ret_t;



//  --------------------------------------------------------------------------
/// @brief Creates a new sam instance
/// @param be_type Backend type
/// @return Handle for inter thread communication
sam_t *
sam_new (sam_be_t be_type);


//  --------------------------------------------------------------------------
/// @brief Destroy an instance, tears down the thread and free's all memory
void
sam_destroy (
    sam_t **self);


//  --------------------------------------------------------------------------
/// @brief (Re)load configuration file
/// @param conf Location of the configuration file
/// @return 0 in case of success, -1 in case of error
int
sam_init (
    sam_t *self,
    sam_cfg_t **cfg);


//  --------------------------------------------------------------------------
/// @brief Instruct sam to analyze and act according to a message
/// @param self A sam instance
/// @param msg Message containing some <action>
/// @return Some sam_ret_t
sam_ret_t *
sam_eval (
    sam_t *self,
    sam_msg_t *msg);


//  --------------------------------------------------------------------------
/// @brief Self test this class.
void *
sam_test ();


#ifdef __cplusplus
}
#endif

#endif
