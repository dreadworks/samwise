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


// compiler macros
#define UU __attribute__((unused))


// don't define these to show all log levels
// #define LOG_THRESHOLD_TRACE   // show info + error
// #define LOG_THRESHOLD_INFO    // only show error
// #define LOG_THRESHOLD_ERROR   // disable logging


// TODO implement shared config (#32)
#define SAM_PUBLIC_ENDPOINT "ipc://samwise"


//
//  public types
//

/// The state of a logger
typedef struct sam_logger_t {
    zsock_t *psh;            ///< socket pushing log requests
    char *name;              ///< identifier for the logger
} sam_logger_t;


/// Type for sam instances
typedef struct sam_t {
    sam_logger_t *logger;
} sam_t;



//  --------------------------------------------------------------------------
/// @brief Create a new instance of samwise
/// @return A freshly allocated sam instance
sam_t *
sam_new ();


//  --------------------------------------------------------------------------
/// @brief Ends all processes and free's all allocated memory
/// @return A freshly allocated sam instance
void
sam_destroy (
    sam_t **self);


//  --------------------------------------------------------------------------
/// @brief (Re)load configuration file
/// @param conf Location of the configuration file
/// @return 0 in case of success, -1 in case of error
int
sam_config (
    sam_t *self,
    const char *conf);


//  --------------------------------------------------------------------------
/// @brief Publish a (content/method) -message to some backend
/// @param self A sam instance
/// @param msg Message containing backend type, distribution etc.
/// @return The calculated delay in ms or -1 in case of error
int
sam_publish (
    sam_t *self,
    zmsg_t *msg);



//  --------------------------------------------------------------------------
/// @brief TODO to be defined
int
sam_stats (
    sam_t *self);


#endif
