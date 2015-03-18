/*  =========================================================================

    sam_buf - Saves messages and handles acknowledgements

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief Handle persisting messages

*/

#ifndef __SAM_BUF_H__
#define __SAM_BUF_H__


/// buf instance wrapping the buffer
typedef struct sam_buf_t {
    int seq;
    DB *dbp;

    zsock_t *in;
    zsock_t *out;

    zactor_t *actor;
} sam_buf_t;



//  --------------------------------------------------------------------------
/// @brief Create a new buf instance
/// @param fname Name of the file to save the data in
/// @param in To read acknowledgements from
/// @param out To re-send publishing requests to
/// @return A new buf instance.
sam_buf_t *
sam_buf_new (
    const char *fname,
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
int
sam_buf_save (
    sam_buf_t *self,
    sam_msg_t *msg);


//  --------------------------------------------------------------------------
/// @brief Self test this class
void *
sam_buf_test ();


#endif
