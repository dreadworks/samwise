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


typedef struct sam_stat_t sam_stat_t;
typedef struct sam_stat_handle_t sam_stat_handle_t;


sam_stat_t *
sam_stat_new ();


void
sam_stat_destroy (
    sam_stat_t **self);


sam_stat_handle_t *
sam_stat_handle_new ();


void
sam_stat_handle_destroy (
    sam_stat_handle_t **handle);


void
sam_stat_ (
    sam_stat_handle_t *handle,
    const char *id,
    int difference);


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



#endif
