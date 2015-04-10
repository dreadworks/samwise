/*  =========================================================================

    sam_msg - A wrapper for zmsg

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief wrapper for zmsg
   @file sam_msg.c

   This is a wrapper around zmsg to enable convenient frame access,
   some memory management help and general error handling.

*/

#include "../include/sam_prelude.h"


//  --------------------------------------------------------------------------
/// Used by zlist_purge () and zlist_destroy () for char * list items
static void
refs_s_destructor (
    void **item)
{
    char **ref =  (char **) item;
    free (*ref);
    *ref = NULL;
}


//  --------------------------------------------------------------------------
/// Used by zlist_purge () and zlist_destroy () for zframe_t * list items
static void
refs_f_destructor (
    void **item)
{
    zframe_t **frame = (zframe_t **) item;
    zframe_destroy (frame);
    *frame = NULL;
}


//  --------------------------------------------------------------------------
/// Converter function that accepts a type character and a variadic
/// argument to be set.
static void *
resolve (
    zframe_t *frame,
    char type,
    va_list arg_p)
{
    // frames
    if (type == 'f') {
        zframe_t **va_p = va_arg (arg_p, zframe_t **);
        if (!va_p) {
            return NULL;
        }

        *va_p = zframe_dup (frame);
        return va_p;
    }


    // pointer
    else if (type == 'p') {
        void **va_p = va_arg (arg_p, void **);
        if (!va_p || zframe_size (frame) != sizeof (void *)) {
            return NULL;
        }

        *va_p = *((void **) zframe_data (frame));
        return va_p;
    }


    // string
    else if (type == 's') {
        char *str = zframe_strdup (frame);
        char **va_p = va_arg (arg_p, char **);
        if (!va_p) {
            return NULL;
        }

        *va_p = str;
        return va_p;
    }


    // integer
    else if (type == 'i') {
        char *str = zframe_strdup (frame);
        int nbr = atoi (str);
        int *va_p = va_arg (arg_p, int *);
        if (!va_p) {
            return NULL;
        }

        *va_p = nbr;
        free (str);
        return va_p;
    }

    // unknown type
    return NULL;
}


//  --------------------------------------------------------------------------
/// Get n char * elements.
static int
resolve_l (
    sam_msg_t *self,
    zframe_t *frame,
    zlist_t *frames,
    zlist_t **list,
    bool remove)
{
    int amount = atoi ((char *) zframe_data (frame));

    if (remove) {
        zframe_destroy (&frame);
    }

    *list = zlist_new ();

    while (amount > 0) {

        frame = (remove)? zlist_pop (frames): zlist_next (frames);
        if (frame == NULL) {
            return -1;
        }

        char *str = zframe_strdup (frame);

        if (remove) {
            zlist_append (self->refs.s, str);
            zframe_destroy (&frame);
        }

        zlist_append (*list, str);
        amount -= 1;
    }

    if (!remove) {
        zlist_set_destructor (*list, refs_s_destructor);
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Constructor function used by both _new () and _decode ().
static sam_msg_t *
new ()
{
    sam_msg_t *self = malloc (sizeof (sam_msg_t));
    assert (self);

    // store for frames
    self->frames = zlist_new ();
    zlist_set_destructor (self->frames, refs_f_destructor);

    // store for popped values
    self->refs.s = zlist_new ();
    self->refs.f = zlist_new ();

    zlist_set_destructor (self->refs.s, refs_s_destructor);
    zlist_set_destructor (self->refs.f, refs_f_destructor);

    // mutexes
    int rc = pthread_mutex_init (&self->owner_lock, NULL);
    assert (!rc);

    rc = pthread_mutex_init (&self->contain_lock, NULL);
    assert (!rc);

    // reference counting
    self->owner_refs = 1;

    return self;
}


//  --------------------------------------------------------------------------
/// Create a new sam_msg instance.
sam_msg_t *
sam_msg_new (
    zmsg_t **zmsg)
{
    assert (zmsg);
    sam_msg_t *self = new ();

    // optimally, this could just access zmsg->frames...
    zframe_t *frame = zmsg_pop (*zmsg);
    while (frame) {
        zlist_append(self->frames, frame);
        frame = zmsg_pop (*zmsg);
    }
    zmsg_destroy (zmsg);

    return self;
}


//  --------------------------------------------------------------------------
/// Destroys the sam_msg instance. All allocated memory gets
/// free'd. This includes all pop ()'d strings.
void
sam_msg_destroy (
    sam_msg_t **self)
{
    assert (*self);

    // please make sure no one tries to _own after the first
    // _destroys, @see sam_msg_own
    pthread_mutex_lock (&(*self)->owner_lock);
    (*self)->owner_refs -= 1;
    int refs = (*self)->owner_refs;
    pthread_mutex_unlock (&(*self)->owner_lock);

    if (refs) {
        return;
    }


    zlist_destroy (&(*self)->frames);

    zlist_destroy (&(*self)->refs.s);
    zlist_destroy (&(*self)->refs.f);

    pthread_mutex_destroy (&(*self)->owner_lock);
    pthread_mutex_destroy (&(*self)->contain_lock);

    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
/// Duplicate a message.
sam_msg_t *
sam_msg_dup (
    sam_msg_t *self)
{
    sam_msg_t *dup = new ();

    zframe_t *frame = zlist_first (self->frames);
    while (frame) {
        zlist_append (dup->frames, zframe_dup (frame));
        frame = zlist_next (self->frames);
    }

    return dup;
}


//  --------------------------------------------------------------------------
/// Own a sam_msg instance. This is used for reference counting. If
/// many entities work with a reference to a message, nobody can tell
/// when it is safe to destroy the message. So every try to destroy
/// the message is used to decrement the reference counter until only
/// one instance holds a reference and the msg objects memoray can be
/// free'd.
///
/// You have to take care to not introduce race conditions. Assume two
/// threads: t1 and t2.
///
/// t1: create msg
/// t1: fork and hand msg over to t2
/// t2: own
/// t1: destroy msg
/// t2: destroy msg
///
/// The behaviour here is obviously undefined. This function can be
/// used in safe way by owning the messages as many times it gets
/// destroyed independently, before handing it to other threads.
void
sam_msg_own (
    sam_msg_t *self)
{
    pthread_mutex_lock (&self->owner_lock);
    self->owner_refs += 1;
    pthread_mutex_unlock (&self->owner_lock);
}


//  --------------------------------------------------------------------------
/// Return the amount of frames left.
int
sam_msg_size (
    sam_msg_t *self)
{
    return zlist_size (self->frames);
}


//  --------------------------------------------------------------------------
/// Free's all recently allocated memory. Everytime the pop ()
/// function is called with one or more 's' or 'f' in the picture, the
/// sam_msg instance keeps track of these allocations. This function
/// free's all recently allocated memory.
void
sam_msg_free (
    sam_msg_t *self)
{
    assert (self);
    zlist_purge (self->refs.s);
    zlist_purge (self->refs.f);
}


//  --------------------------------------------------------------------------
/// Pop one or more frames from the message. A picture must be
/// provided describing the data to expect. The picture is a string
/// where each character represents a data type. Currently the
/// following data types are supported:
///
///   'i': for int
///   's': for char *
///   'f': for zframe_t *
///   'p': for void *
///   'l': for zlist_t * containing char *
///
/// Pop'd or contained 's' and 'f' are automatically garbage collected
/// with sam_msg_destroy () or manually by invoking sam_msg_free ()
/// Proper destruction of 'p' must be handled by the
/// caller. Obviously, 'i' must not be garbage collected.
int
sam_msg_pop (
    sam_msg_t *self,
    const char *pic,
    ...)
{
    assert (self);
    assert (pic);

    va_list arg_p;
    va_start (arg_p, pic);

    while (*pic) {
        zframe_t *frame = zlist_pop (self->frames);
        if (frame == NULL) {
            return -1;
        }

        // handle 'h'
        if (*pic == 'l') {
            zlist_t **list = va_arg (arg_p, zlist_t **);
            if (!list) {
                return -1;
            }

            if (resolve_l (self, frame, self->frames, list, true)) {
                return -1;
            }
        }

        // handle others
        else {
            void *ptr = resolve (frame, *pic, arg_p);
            zframe_destroy (&frame);

            if (ptr == NULL) {
                return -1;
            }

            if (*pic == 'f') {
                zlist_append (self->refs.f, *(zframe_t **) ptr);
            }
            else if (*pic == 's') {
                zlist_append (self->refs.s, *(char **) ptr);
            }
        }

        pic += 1;
    }

    va_end (arg_p);
    return 0;
}


//  --------------------------------------------------------------------------
/// Get data from the message without removing it. Caller is
/// responsible for freeing all allocated memory ('s', 'f', 'l'). For
/// 'l', a correct destructor function is set for items, so a call to
/// zlist_destroy is sufficient to free all memory.
int
sam_msg_get (
    sam_msg_t *self,
    const char *pic,
    ...)
{
    assert (self);
    assert (pic);

    // copy list for thread safety
    pthread_mutex_lock (&self->contain_lock);
    zlist_t *frames = zlist_dup (self->frames);
    pthread_mutex_unlock (&self->contain_lock);
    zlist_set_destructor (frames, NULL);

    va_list arg_p;
    va_start (arg_p, pic);

    zframe_t *frame = zlist_first (frames);
    while (*pic && frame != NULL) {

        if (*pic != '?') {

            if (*pic == 'l') {
                zlist_t **list = va_arg (arg_p, zlist_t **);
                if (!list) {
                    return -1;
                }

                if (resolve_l (self, frame, frames, list, false)) {
                    return -1;
                }
            }

            else {
                void *ptr = resolve (frame, *pic, arg_p);
                if (ptr == NULL) {
                    return -1;
                }
            }
        }

        frame = zlist_next (frames);
        pic += 1;
    }

    // clean up
    zlist_destroy (&frames);
    va_end (arg_p);

    // insufficient frames
    if (*pic) {
        return -1;
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Tests the message frames if some provided constraints are satisfied.
int
sam_msg_expect (
    sam_msg_t *self,
    int size,
    ...)
{
    if (sam_msg_size (self) < size) {
        return -1;
    }

    va_list arg_p;
    va_start (arg_p, size);

    zframe_t *frame = zlist_first (self->frames);
    while (size > 0 && frame != NULL) {
        sam_msg_rule_t rule = va_arg (arg_p, sam_msg_rule_t);
        if (rule == SAM_MSG_NONZERO && zframe_size (frame) == 0) {
            return -1;
        }

        frame = zlist_next (self->frames);
        size -= 1;
    }

    va_end (arg_p);
    return 0;
}


//  --------------------------------------------------------------------------
/// Returns the number of bytes needed to store the fully encoded message.
size_t
sam_msg_encoded_size (
    sam_msg_t *self)
{
    assert (self);

    size_t buf_size = 0;
    zframe_t *frame = zlist_first (self->frames);

    while (frame != NULL) {
        size_t frame_size = zframe_size (frame);

        if (frame_size < 0xFF) {
            buf_size += frame_size + 1;
        }
        else {
            buf_size += frame_size + 1 + 4;
        }

        frame = zlist_next (self->frames);
    }

    return buf_size;
}


//  --------------------------------------------------------------------------
/// Encode a sam_msg object into a buffer.
void
sam_msg_encode (
    sam_msg_t *self,
    byte **buf)
{
    assert (self);
    assert (*buf);

    // taken from zmsg_encode
    byte *dest = *buf;
    zframe_t *frame = zlist_first (self->frames);

    while (frame) {
        size_t frame_size = zframe_size (frame);
        if (frame_size < 0xFF) {
            *dest++ = (byte) frame_size;
            memcpy (dest, zframe_data (frame), frame_size);
            dest += frame_size;
        }
        else {
            *dest++ = 0xFF;
            *dest++ = (frame_size >> 24) & 255;
            *dest++ = (frame_size >> 16) & 255;
            *dest++ = (frame_size >>  8) & 255;
            *dest++ = frame_size        & 255;
            memcpy (dest, zframe_data (frame), frame_size);
            dest += frame_size;
        }
        frame = zlist_next (self->frames);
    }
}


//  --------------------------------------------------------------------------
/// Decode a sam_msg object from a buffer.
sam_msg_t *
sam_msg_decode (
    byte *buf,
    size_t size)
{
    // taken from zmsg_decode
    sam_msg_t *self = new ();
    if (!self) {
        return NULL;
    }

    byte *source = buf;
    byte *limit = buf + size;

    while (source < limit) {
        size_t frame_size = *source++;
        if (frame_size == 0xFF) {
            if (source > limit - 4) {
                sam_msg_destroy (&self);
                break;
            }

            frame_size = (source [0] << 24)
                         + (source [1] << 16)
                         + (source [2] << 8)
                         +  source [3];
            source += 4;
        }

        if (source > limit - frame_size) {
            sam_msg_destroy (&self);
            break;
        }

        zframe_t *frame = zframe_new (source, frame_size);
        if (frame) {
            if (zlist_append (self->frames, frame)) {
                sam_msg_destroy (&self);
                break;
            }
            source += frame_size;
        }
        else {
            sam_msg_destroy (&self);
            break;
        }
    }

    return self;
}
