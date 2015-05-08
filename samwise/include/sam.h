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


#define SAM_VERSION_MAJOR 0
#define SAM_VERSION_MINOR 0
#define SAM_VERSION_PATCH 1

#define SAM_MAKE_VERSION(major, minor, patch) \
    ((major) *10000 + (minor) * 100 + (patch))

#define SAM_VERSION \
    SAM_MAKE_VERSION(SAM_VERSION_MAJOR, \
                     SAM_VERSION_MINOR, \
                     SAM_VERSION_PATCH)


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


typedef struct sam_t sam_t;


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
/// @brief Create a new message backend
/// @param self A sam instance
/// @param be_type Type of the backend to create
/// @return The calculated delay in ms or -1 in case of error
sam_backend_t *
sam_be_create (
    sam_t *self,
    const char *name,
    void *opts);


//  --------------------------------------------------------------------------
/// @brief Remove a message backend
/// @param self A sam instance
/// @param be_type Type of the backend to create
/// @return The calculated delay in ms or -1 in case of error
int
sam_be_remove (
    sam_t *self,
    const char *name);


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


#endif
