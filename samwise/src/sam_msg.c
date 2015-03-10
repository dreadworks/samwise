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
refs_s_destructor (void **item)
{
    char **ref =  (char **) item;
    free (*ref);
    *ref = NULL;
}


//  --------------------------------------------------------------------------
/// Used by zlist_purge () and zlist_destroy () for zframe_t * list items
static void
refs_f_destructor (void **item)
{
    zframe_t **frame = (zframe_t **) item;
    zframe_destroy (frame);
    *frame = NULL;
}


//  --------------------------------------------------------------------------
/// Create a new sam_msg instance. Wraps a zmsg instance for
/// convenience.
sam_msg_t *
sam_msg_new (zmsg_t **zmsg)
{
    assert (zmsg);

    sam_msg_t *self = malloc (sizeof (sam_msg_t));
    assert (self);

    // change ownership
    self->zmsg = *zmsg;
    *zmsg = NULL;

    // store for popped values
    self->refs.s = zlist_new ();
    self->refs.f = zlist_new ();

    zlist_set_destructor (self->refs.s, refs_s_destructor);
    zlist_set_destructor (self->refs.f, refs_f_destructor);

    // store for contained values
    self->container = zlist_new ();

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
/// Destroys the sam_msg instance. All allocated memory gets
/// free'd. This includes all pop ()'d strings.
void
sam_msg_destroy (sam_msg_t **self)
{
    assert (*self);

    // please make sure no one tries to _own after the first
    // _destroys, @see sam_msg_own
    pthread_mutex_lock (&(*self)->owner_lock);
    (*self)->owner_refs -= 1;
    pthread_mutex_unlock (&(*self)->owner_lock);

    if ((*self)->owner_refs) {
        return;
    }

    zmsg_destroy (&(*self)->zmsg);

    zlist_destroy (&(*self)->refs.s);
    zlist_destroy (&(*self)->refs.f);
    zlist_destroy (&(*self)->container);

    pthread_mutex_destroy (&(*self)->owner_lock);
    pthread_mutex_destroy (&(*self)->contain_lock);

    free (*self);
    *self = NULL;
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
/// used in safe way by own the messages as many times it gets
/// destroyed independently before handing it to other threads.
void
sam_msg_own (sam_msg_t *self)
{
    pthread_mutex_lock (&self->owner_lock);
    self->owner_refs += 1;
    pthread_mutex_unlock (&self->owner_lock);
}

//  --------------------------------------------------------------------------
/// Returns the number of remaining readable frames.
int
sam_msg_frames (sam_msg_t *self)
{
    assert (self);
    return zmsg_size (self->zmsg);
}



//  --------------------------------------------------------------------------
/// Free's all recently allocated memory. Everytime the pop ()
/// function is called with one or more 's' in the picture, the
/// sam_msg instance keeps track of these allocations. This function
/// free's all recently allocated memory.
void
sam_msg_free (sam_msg_t *self)
{
    assert (self);
    zlist_purge (self->refs.s);
    zlist_purge (self->refs.f);
    zlist_purge (self->container);
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
///
/// Pop'd or contained 's' and 'f' are automatically garbage collected
/// with sam_msg_destroy () or manually by invoking sam_msg_free ()
/// Proper destruction of 'p' must be handled by the
/// caller. Obviously, 'i' must not be garbage collected.
int
sam_msg_pop (sam_msg_t *self, const char *pic, ...)
{
    assert (self);
    assert (pic);

    int rc = 0;
    va_list arg_p;
    va_start (arg_p, pic);

    while (*pic) {

        // frame
        if (*pic == 'f') {
            zframe_t *frame = zmsg_pop (self->zmsg);
            if (!frame) {
                rc = -1;
                break;
            }

            zframe_t **va_p = va_arg (arg_p, zframe_t **);
            if (va_p) {
                *va_p = frame;
                zlist_append (self->refs.f, *va_p);
                pic += 1;
                continue;
            }
            else {
                rc = -1;
                zframe_destroy (&frame);
                break;
            }
        }

        // pointer
        if (*pic == 'p') {
            zframe_t *frame = zmsg_pop (self->zmsg);
            if (!frame) {
                rc = -1;
                break;
            }

            void **va_p = va_arg (arg_p, void **);
            if (va_p) {
                if (zframe_size (frame) == sizeof (void *)) {
                    *va_p = *((void **) zframe_data (frame));
                    pic += 1;
                    zframe_destroy (&frame);
                    continue;
                }
                else {
                    rc = -1;
                }
            }
            zframe_destroy (&frame);

            if (rc == -1) {
                break;
            }
        }

        char *str = zmsg_popstr (self->zmsg);
        if (!str) {
            rc = -1;
            break;
        }

        // char *
        if (*pic == 's') {
            char **va_p = va_arg (arg_p, char **);
            if (va_p) {
                *va_p = str;
                zlist_append (self->refs.s, *va_p);
            }
            else {
                rc = -1;
            }
        }

        // integer
        else if (*pic == 'i') {
            int nbr = atoi (str);
            int *va_p = va_arg (arg_p, int *);
            if (va_p) {
                *va_p = nbr;
                free (str);
            }
            else {
                rc = -1;
            }
        }

        if (rc == -1) {
            free (str);
            break;
        }

        pic += 1;
    }

    va_end (arg_p);
    return rc;
}


//  --------------------------------------------------------------------------
/// Contain a number of frames internally. This is used for fast
/// access to frames in a thread safe way.
int
sam_msg_contain (sam_msg_t *self, const char *pic)
{
    assert (self);
    assert (pic);

    int rc = 0;
    while (*pic) {
        void *item;

        // save 'i' as 's', convert with atoi in _contained ()
        // lets zeromq handle all heap allocations
        if (*pic == 'i' || *pic == 's') {
            rc = sam_msg_pop (self, "s", &item);
        }
        else if (*pic == 'f' || *pic == 'p') {
            char buf [2] = { *pic, '\0' };
            rc = sam_msg_pop (self, buf, &item);
        }
        else {
            rc = -1;
        }

        if (rc) {
            return rc;
        }

        zlist_append (self->container, item);
        pic += 1;
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Return contained data. Thread safe function.
int
sam_msg_contained (sam_msg_t *self, const char *pic, ...)
{
    pthread_mutex_lock (&self->contain_lock);
    zlist_t *container = zlist_dup (self->container);
    pthread_mutex_unlock (&self->contain_lock);

    int rc = 0;
    va_list arg_p;
    va_start (arg_p, pic);

    while (*pic) {
        void *item = zlist_pop (container);
        if (!item) {
            sam_log_trace ("no item left");
            rc = -1;
        }
        else {
            if (*pic == 'i') {
                int *va_p = va_arg (arg_p, int *);
                if (va_p) {
                    *va_p = atoi ((char *) item);
                }
                else {
                    rc = -1;
                }
            }
            else if (*pic == 's') {
                char **va_p = va_arg (arg_p, char **);
                if (va_p) {
                    *va_p = (char *) item;
                }
                else {
                    rc = -1;
                }
            }
            else if (*pic == 'f') {
                zframe_t **va_p = va_arg (arg_p, zframe_t **);
                if (va_p) {
                    *va_p = (zframe_t *) item;
                }
                else {
                    rc = -1;
                }
            }
            else if (*pic == 'p') {
                void **va_p = va_arg (arg_p, void **);
                if (va_p) {
                    *va_p = (void *) item;
                }
                else {
                    rc = -1;
                }
            }
            else {
                rc = -1;
            }
        }

        if (rc) {
            sam_log_trace (
                "not enough variadic arguments provided or unknown pic");
            zlist_destroy (&container);
            return rc;
        }

        pic += 1;
    }   // end while

    zlist_destroy (&container);
    return rc;
}
