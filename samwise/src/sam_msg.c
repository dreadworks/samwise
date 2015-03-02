/*  =========================================================================

    sam_msg - A wrapper for zmsg

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief wrapper for zmsg
   @file samd.c

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

    return self;
}


//  --------------------------------------------------------------------------
/// Destroys the sam_msg instance. All allocated memory gets
/// free'd. This includes all pop ()'d strings.
void
sam_msg_destroy (sam_msg_t **self)
{
    assert (*self);

    zmsg_destroy (&(*self)->zmsg);
    zlist_destroy (&(*self)->refs.s);
    zlist_destroy (&(*self)->refs.f);
    zlist_destroy (&(*self)->container);

    free (*self);
    *self = NULL;
}



//  --------------------------------------------------------------------------
/// Returns the number of remaining frames.
int
sam_msg_size (sam_msg_t *self)
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
        else if (*pic == 'f') {
            rc = sam_msg_pop (self, "f", &item);
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
/// Return contained data. This is going to be thread safe in the future.
int
sam_msg_contained (sam_msg_t *self, const char *pic, ...)
{
    // TODO #55 zlist_dup () is not thread safe
    zlist_t *container = zlist_dup (self->container);

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


//  --------------------------------------------------------------------------
/// Self test this class.
void
sam_msg_test ()
{
    printf ("\n ** SAM_MSG **\n");

    // construct/destruct
    zmsg_t *zmsg = zmsg_new ();
    sam_msg_t *msg = sam_msg_new (&zmsg);
    assert (msg);
    assert (!zmsg);
    assert (sam_msg_size (msg) == 0);

    sam_msg_destroy (&msg);
    assert (!msg);


    // test pop ()
    zmsg = zmsg_new ();

    char *nbr = "17";
    char *str = "test";
    char a = 'a';
    zframe_t *frame = zframe_new (&a, sizeof (a));

    int rc = zmsg_pushstr (zmsg, nbr);
    assert (!rc);

    rc = zmsg_pushstr (zmsg, str);
    assert (!rc);

    rc = zmsg_push (zmsg, frame);
    assert (!rc);

    int pic_nbr;
    char *pic_str;
    zframe_t *pic_frame;

    msg = sam_msg_new (&zmsg);
    assert (sam_msg_size (msg) == 3);

    rc = sam_msg_pop (msg, "fsi", &pic_frame, &pic_str, &pic_nbr);
    assert (sam_msg_size (msg) == 0);

    assert (rc == 0);
    assert (zframe_eq (frame, pic_frame));
    assert (pic_nbr == atoi (nbr));
    assert (!strcmp (pic_str, str));
    sam_msg_destroy (&msg);


    // pop(): not enough data in message
    zmsg = zmsg_new ();
    rc = zmsg_pushstr (zmsg, "one");
    assert (!rc);

    msg = sam_msg_new (&zmsg);
    char *invalid;
    rc = sam_msg_pop (msg, "ss", &pic_str, &invalid);
    assert (rc == -1);
    sam_msg_destroy (&msg);


    // pop (): not enough varargs
    // there is no way to solve this?
    /* zmsg = zmsg_new (); */
    /* rc = zmsg_pushstr (zmsg, "one"); */
    /* assert (!rc); */
    /* rc = zmsg_pushstr (zmsg, "two"); */

    /* printf ("\nsending too few varargs, just %p\n", (void *) &pic_str); */
    /* msg = sam_msg_new (&zmsg); */
    /* rc = sam_msg_pop (msg, "ss", &pic_str); */
    /* assert (rc == -1); */
    /* sam_msg_destroy (&msg); */


    // free ()
    zmsg = zmsg_new ();
    rc = zmsg_pushstr (zmsg, "one");
    assert (!rc);
    rc = zmsg_pushstr (zmsg, "two");

    msg = sam_msg_new (&zmsg);
    rc = sam_msg_pop (msg, "s", &pic_str);
    assert (!rc);
    assert (!strcmp (pic_str, "two"));

    sam_msg_free (msg);

    rc = sam_msg_pop (msg, "s", &pic_str);
    assert (!rc);
    assert (!strcmp (pic_str, "one"));
    sam_msg_destroy (&msg);


    // contain[er] ()
    zmsg = zmsg_new ();
    char
        *str1 = "str 1",
        *str2 = "str 2",
        *nbr1 = "1",
        *nbr2 = "2";

    char
        char1 = 'a',
        char2 = 'b';

    zframe_t
        *frame1 = zframe_new (&char1, sizeof (char1)),
        *frame2 = zframe_new (&char2, sizeof (char2));

    // add first chunk
    zmsg_addstr (zmsg, str1);
    zmsg_addstr (zmsg, nbr1);
    zmsg_add (zmsg, frame1);

    // add second chunk
    zmsg_addstr (zmsg, str2);
    zmsg_addstr (zmsg, nbr2);
    zmsg_add (zmsg, frame2);

    msg = sam_msg_new (&zmsg);
    assert (!zmsg);
    assert (sam_msg_size (msg) == 6);

    sam_msg_contain (msg, "sif");
    assert (sam_msg_size (msg) == 3);

    int round_c;
    char *pic_str1;
    int pic_nbr1;
    zframe_t *pic_frame1;

    // test initial 3
    for (round_c = 0; round_c < 2; round_c++) {
        rc = sam_msg_contained (
            msg, "sif", &pic_str1, &pic_nbr1, &pic_frame1);
        assert (!rc);

        assert (!strcmp (pic_str1, str1));
        assert (pic_nbr1 == atoi (nbr1));
        assert (zframe_eq (pic_frame1, frame1));
    }

    // pop one more
    char *pic_str2 = NULL;
    pic_str1 = NULL;
    pic_nbr1 = -1;
    pic_frame1 = NULL;

    rc = sam_msg_contain (msg, "s");
    assert (!rc);

    rc = sam_msg_contained (
        msg, "sifs", &pic_str1, &pic_nbr1, &pic_frame1, &pic_str2);
    assert (!rc);

    assert (!strcmp (pic_str1, str1));
    assert (pic_nbr1 == atoi (nbr1));
    assert (zframe_eq (pic_frame1, frame1));
    assert (!strcmp (pic_str2, str2));

    // clear current and pop last two
    int pic_nbr2;
    zframe_t *pic_frame2;

    sam_msg_free (msg);
    rc = sam_msg_contain (msg, "if");
    assert (!rc);

    rc = sam_msg_contained (
        msg, "if", &pic_nbr2, &pic_frame2);
    assert (!rc);

    assert (pic_nbr2 == atoi (nbr2));
    assert (zframe_eq (pic_frame2, frame2));


    // check that no more can be popped
    sam_msg_free (msg);
    rc = sam_msg_contain (msg, "s");
    assert (rc == -1);


    // check that no more can be returned
    rc = sam_msg_contained (
        msg, "if", &pic_nbr2, &pic_frame2);
    assert (rc == -1);


    sam_msg_destroy (&msg);
}
