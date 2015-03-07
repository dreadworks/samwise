/*  =========================================================================

    sam_msg - zmsg wrapper

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT


    =========================================================================
*/
/**

   @brief Wrapper for zmsg

   Wraps zmsg instances and offers garbage collection.

*/

#ifndef __SAM_MSG_H__
#define __SAM_MSG_H__


/// a zmsg wrapper
typedef struct sam_msg_t {
    int owner_refs;                ///< reference counting by _own ()
    pthread_mutex_t owner_lock;    ///< used in _own ()
    pthread_mutex_t contain_lock;  ///< used in _contained ()

    zmsg_t *zmsg;                  ///< the wrapped message
    zlist_t *container;            ///< reference list for contained () calls

    struct refs {
        zlist_t *s;                ///< for allocated strings
        zlist_t *f;                ///< for allocated frames
    } refs;

} sam_msg_t;


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
/// @brief Become one owner of the msg instance, used for reference counting
/// @param self A sam_msg_instance
void
sam_msg_own (
    sam_msg_t *self);


//  --------------------------------------------------------------------------
/// @brief Destroy a sam_msg, free's all recently allocated memory
/// @param self A sam_msg instance
int
sam_msg_size (
    sam_msg_t *self);


//  --------------------------------------------------------------------------
/// @brief Free's all memory allocated by the last pop() calls
/// @param self A sam_msg instance
void
sam_msg_free (sam_msg_t *self);


//  --------------------------------------------------------------------------
/// @brief Pop next frames with a picture
/// @param self A sam_msg instance
/// @param pic Describes the type of the frames content
/// @param ... Empty pointers for the popped values
/// @return 0 if okay, -1 for errors
int
sam_msg_pop (sam_msg_t *self, const char *pic, ...);


//  --------------------------------------------------------------------------
/// @brief Contain some data internally
/// @param self A sam_msg instance
/// @param pic Describes the frame contents
/// @return 0 if okay, -1 for errors
int
sam_msg_contain (sam_msg_t *self, const char *pic);


//  --------------------------------------------------------------------------
/// @brief Retrieve a shallow copy of the currently contained items
/// @param self A sam_msg instance
int
sam_msg_contained (sam_msg_t *self, const char *pic, ...);


//  --------------------------------------------------------------------------
/// @ Self test this class
void *
sam_msg_test ();


#endif
