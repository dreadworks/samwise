/*  =========================================================================

    sam_cfg - easy access to the configuration

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief Loads samwise configuration files

   Offers convenience functions to read samwise configuration files
   and automatically converts values to internally used types.

*/

#ifndef __SAM_CFG_H__
#define __SAM_CFG_H__



//  --------------------------------------------------------------------------
/// @brief Create a new cfg instance
/// @param cfg_file Location of a ZPL conformant configuration file
/// @return A new cfg instance.
sam_cfg_t *
sam_cfg_new (
    const char *cfg_file);


//  --------------------------------------------------------------------------
/// @brief Destroy a cfg instance
/// @param self A cfg instance
void
sam_cfg_destroy (
    sam_cfg_t **self);



//
//  GENERAL
//

//  --------------------------------------------------------------------------
/// @brief Retrieve a zconfig subset of the loaded configuration
/// @param self A cfg instance
/// @param path Determines which subset to load
/// @param conf Pointer to be set
/// @return -1 in case of error, 0 on success
int
sam_cfg_get (
    sam_cfg_t *self,
    const char *path,
    zconfig_t **conf);


//
//  BUFFER CONFIGURATION
//

//  --------------------------------------------------------------------------
/// @brief Returns the buffer size in bytes
/// @param self A cfg instance
/// @param size Pointer to the value set
/// @return -1 in case of error, 0 on success
int
sam_cfg_buf_size (
    sam_cfg_t *self,
    uint64_t *size);


//  --------------------------------------------------------------------------
/// @brief Returns the maximum number of retries
/// @param self A cfg instance
/// @param interval Pointer to the value set
/// @return -1 in case of error, 0 on success
int
sam_cfg_buf_retry_count (
    sam_cfg_t *self,
    int *count);


//  --------------------------------------------------------------------------
/// @brief Returns the retry interval in milliseconds.
/// @param self A cfg instance
/// @param interval Pointer to the value set
/// @return -1 in case of error, 0 on success
int
sam_cfg_buf_retry_interval (
    sam_cfg_t *self,
    uint64_t *interval);


//  --------------------------------------------------------------------------
/// @brief Returns the retry threshold in milliseconds.
/// @param self A cfg instance
/// @param threshold Pointer to the value set
/// @return -1 in case of error, 0 on success
int
sam_cfg_buf_retry_threshold (
    sam_cfg_t *self,
    uint64_t *threshold);


//
//  BACKEND CONFIGURATION
//

//  --------------------------------------------------------------------------
/// @brief Load the public endpoint
/// @param self A cfg instance
/// @param endpoint Pointer to point to the data
/// @return 0 for success, -1 for errors
int
sam_cfg_endpoint (
    sam_cfg_t *self,
    char **endpoint);


//  --------------------------------------------------------------------------
/// @brief Load the backend type
/// @param self A cfg instance
/// @param be_type Pointer to point to the data
/// @return 0 for success, -1 for errors
int
sam_cfg_be_type (
    sam_cfg_t *self,
    sam_be_t *be_type);


//  --------------------------------------------------------------------------
/// @brief Loads an array of backend options
/// @param self A cfg instance
/// @param be_type Decides the format to load
/// @param be_c Number of backends loaded
/// @param names Is going to point to a buffer containing all names
/// @param opts Is going to point to a buffer containing all options
/// @return 0 for success, -1 for errors
int
sam_cfg_be_backends (
    sam_cfg_t *self,
    sam_be_t be_type,
    int *be_c,
    char ***names,
    void **opts);


//  --------------------------------------------------------------------------
/// @brief Self test this class
void *
sam_cfg_test ();


#endif
