/*  =========================================================================

    sam_msg - zmsg wrapper

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT


    =========================================================================
*/
/**

   @brief Wrapper for zmsg

   Wraps zmsg instances. Offers convenience functions to access the
   data, encode or decode messages, garbage collection and reference
   counting for concurrent access.

*/

#ifndef __SAM_MSG_H__
#define __SAM_MSG_H__

#ifdef __cplusplus
extern "C" {
#endif


// rules for expect ()'s variadic arguments
typedef enum {
    SAM_MSG_ZERO,     ///< frame must exist, but can be of zero length
    SAM_MSG_NONZERO,  ///< frame must exist and contain data
    SAM_MSG_LIST      ///< n+1 frames, where n is encoded in the first frame
} sam_msg_rule_t;



//  --------------------------------------------------------------------------
/// @brief Creates a new sam_msg instance from an existing zmsg
/// @return New sam_msg instance
sam_msg_t *
sam_msg_new (zmsg_t **zmsg);


//  --------------------------------------------------------------------------
/// @brief Destroy a sam_msg, free's all recently allocated memory
/// @param self A sam_msg instance
void
sam_msg_destroy (
    sam_msg_t **self);


//  --------------------------------------------------------------------------
/// @brief Create a deep copy of the message object
/// @self A sam_msg instance
/// @return A copy of the sam_msg instance
sam_msg_t *
sam_msg_dup (
    sam_msg_t *self);


//  --------------------------------------------------------------------------
/// @brief Become one owner of the msg instance, used for reference counting
/// @param self A sam_msg_instance
void
sam_msg_own (
    sam_msg_t *self);


//  --------------------------------------------------------------------------
/// @brief Return the count of saved frames
/// @param self A sam_msg instance
int
sam_msg_size (
    sam_msg_t *self);


//  --------------------------------------------------------------------------
/// @brief Free's all memory allocated by the last pop() calls
/// @param self A sam_msg instance
void
sam_msg_free (
    sam_msg_t *self);


//  --------------------------------------------------------------------------
/// @brief Pop next frames with a picture
/// @param self A sam_msg instance
/// @param pic Describes the type of the frames content
/// @param ... Empty pointers for the popped values
/// @return 0 if okay, -1 for errors
int
sam_msg_pop (
    sam_msg_t *self,
    const char *pic,
    ...);


//  --------------------------------------------------------------------------
/// @brief Retrieve a copy of the currently contained items
/// @param self A sam_msg instance
/// @param pic Describes the type of the frames content
/// @param ... Pointers to be set
/// @returns 0 if okay, -1 for errors
int
sam_msg_get (
    sam_msg_t *self,
    const char *pic,
    ...);


//  --------------------------------------------------------------------------
/// @brief Check if at least <size> frames are available
/// @param self A sem_msg instance
/// @param size At least <size> frames must be available
/// @param ... sam_msg_rule_t's constraints
/// @return 0 in case of succes, -1 for errors
int
sam_msg_expect (
    sam_msg_t *self,
    int size,
    ...);


//  --------------------------------------------------------------------------
/// @brief Return the size of the buffer needed to store the encoded message
/// @param self A sam_msg instance
/// @return Required buffer size
size_t
sam_msg_encoded_size (
    sam_msg_t *self);


//  --------------------------------------------------------------------------
/// @brief Encode all non-popped frames into an opaque buffer
/// @param self A sam_msg instance
/// @param buf Is goint to point to the malloc'd buffer
/// @return 0 for success, -1 otherwise
void
sam_msg_encode (
    sam_msg_t *self,
    byte **buf);


//  --------------------------------------------------------------------------
/// @brief Decode a buffer to a sam_msg
/// @param buf The buffer containing a encoded sam_msg
/// @param size Size of the buffer
/// @return Freshly decoded sam_msg instance
sam_msg_t *
sam_msg_decode (
    byte *buf,
    size_t size);



//  --------------------------------------------------------------------------
/// @brief Self test this class
void *
sam_msg_test ();


#ifdef __cplusplus
}
#endif

#endif
