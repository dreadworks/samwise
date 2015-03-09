/*  =========================================================================

    sam_cfg - easy access to the configuration

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief Loads samwise configuration files

*/

#ifndef __SAM_CFG_H__
#define __SAM_CFG_H__


/// cfg instance, wraps a zconfig instance
typedef struct sam_cfg_t {
    zconfig_t *zcfg; ///< points to a loaded configuration file
} sam_cfg_t;



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
//  BUFFER CONFIGURATION
//

//  --------------------------------------------------------------------------
/// @brief Load the buffers file location
int
sam_cfg_buf_file (
    sam_cfg_t *self,
    char **fname);


//  --------------------------------------------------------------------------
/// @brief Returns the buffer size in bytes
int
sam_cfg_buf_size (
    sam_cfg_t *self,
    int *size);


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
