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
/// Used by zlist_purge () and zlist_destroy ()
static void
refs_destructor (void **item)
{
    if (zframe_is (*item)) {
        zframe_t **frame = (zframe_t **) item;
        zframe_destroy (frame);
        *frame = NULL;
    }
    else {
        char **ref =  (char **) item;
        free (*ref);
        *ref = NULL;
    }
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
    self->refs = zlist_new ();
    zlist_set_destructor (self->refs, refs_destructor);

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
    zlist_destroy (&(*self)->refs);
    free (*self);
    *self = NULL;
}



//  --------------------------------------------------------------------------
/// Returns the number of remaining frames.
int
sam_msg_size (sam_msg_t *self)
{
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
    zlist_purge (self->refs);
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
                zlist_append (self->refs, *va_p);
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
                zlist_append (self->refs, *va_p);
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
    zmsg = zmsg_new ();
    rc = zmsg_pushstr (zmsg, "one");
    assert (!rc);
    rc = zmsg_pushstr (zmsg, "two");

    msg = sam_msg_new (&zmsg);
    rc = sam_msg_pop (msg, "ss", &pic_str);
    assert (rc == -1);
    sam_msg_destroy (&msg);


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
}
