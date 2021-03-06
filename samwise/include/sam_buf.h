/*  =========================================================================

    sam_buf - Saves messages and handles acknowledgements

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief Handle persisting messages

   This class accepts storage requests, persists un-acknowledged
   messages to disk and re-sends them after some time.

*/

#ifndef __SAM_BUF_H__
#define __SAM_BUF_H__

#ifdef __cplusplus
extern "C" {
#endif


typedef struct sam_buf_t sam_buf_t;


//  --------------------------------------------------------------------------
/// @brief Create a new buf instance
/// @param cfg Samwise configuration
/// @param in To read acknowledgements from
/// @param out To re-send publishing requests to
/// @return A new buf instance.
sam_buf_t *
sam_buf_new (
    sam_cfg_t *cfg,
    zsock_t **in,
    zsock_t **out);


//  --------------------------------------------------------------------------
/// @brief Destroy a buf instance
/// @param self A buf instance
void
sam_buf_destroy (
    sam_buf_t **self);


//  --------------------------------------------------------------------------
/// @brief Hand message over to store, get a key as the receipt
/// @param self A buf instance
/// @param msg A publishing request wrapped by sam_msg_t
/// @param count How many backends must acknowledge the message
/// @return A unique id used to identify the message
int
sam_buf_save (
    sam_buf_t *self,
    sam_msg_t *msg,
    int count);


//  --------------------------------------------------------------------------
/// @brief Self test this class
void *
sam_buf_test ();


#ifdef __cplusplus
}
#endif

#endif
