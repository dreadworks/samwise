/*  =========================================================================

    sam_stat - gather internal statistics

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT


    =========================================================================
*/
/**

   @brief central hub for statistics

*/

#ifndef __SAM_BE_STAT_H__
#define __SAM_BE_STAT_H__

#ifdef __cplusplus
extern "C" {
#endif


typedef struct sam_stat_t sam_stat_t;
typedef struct sam_stat_handle_t sam_stat_handle_t;


//  --------------------------------------------------------------------------
/// @brief Create a new statistics aggregator
/// @return A stat instance
sam_stat_t *
sam_stat_new ();


//  --------------------------------------------------------------------------
/// @brief Destroy a stat instance
/// @param self Reference to a stat instance
void
sam_stat_destroy (
    sam_stat_t **self);


//  --------------------------------------------------------------------------
/// @brief Create a new handle to send metrics
/// @return A stat handle
sam_stat_handle_t *
sam_stat_handle_new ();


//  --------------------------------------------------------------------------
/// @brief Destroy a handle
/// @param handle Reference to a stat handle
void
sam_stat_handle_destroy (
    sam_stat_handle_t **handle);


//  --------------------------------------------------------------------------
/// @brief Send a request to update the metrics
/// @param handle A stat handle
/// @param id A string describing which stats to update
/// @param difference Negative or positive number to be added
void
sam_stat_ (
    sam_stat_handle_t *handle,
    const char *id,
    int difference);


//  --------------------------------------------------------------------------
/// @brief Get a summary of the statistics as a string
/// @param handle A stat handle
/// @return All metrics as a string
char *
sam_stat_str_ (
    sam_stat_handle_t *handle);


//
//   PREPROCESSOR MACROS
//


#if defined(SAM_STAT)
    #define sam_stat(handle, id, val) sam_stat_(handle, id, val)
    #define sam_stat_str(handle) sam_stat_str_(handle)
#else
    #define sam_stat(handle, id, val)
    #define sam_stat_str(handle) NULL
#endif


#ifdef __cplusplus
}
#endif

#endif
