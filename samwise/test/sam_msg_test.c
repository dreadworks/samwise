/*  =========================================================================

    sam_log_test - Test sam_log

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/

#include "../include/sam_prelude.h"


//  --------------------------------------------------------------------------
/// Create and destroy a message.
START_TEST(test_msg_life_basic)
{
    sam_selftest_introduce ("test_msg_life_basic");

    zmsg_t *zmsg = zmsg_new ();
    sam_msg_t *msg = sam_msg_new (&zmsg);

    if (!msg) {
        ck_abort_msg ("could not create message");
    }

    if (zmsg) {
        ck_abort_msg ("zmsg is still reachable");
    }

    sam_msg_destroy (&msg);
    if (msg) {
        ck_abort_msg ("msg is still reachable");
    }
}
END_TEST


//  --------------------------------------------------------------------------
/// Test reference counting with _own ().
START_TEST(test_msg_own)
{
    sam_selftest_introduce ("test_msg_own");

    zmsg_t *zmsg = zmsg_new ();
    sam_msg_t *msg = sam_msg_new (&zmsg);

    sam_msg_own (msg);
    sam_msg_destroy (&msg);
    if (!msg) {
        ck_abort_msg ("msg was deleted but is still referenced");
    }

    sam_msg_destroy (&msg);
    if (msg) {
        ck_abort_msg ("msg is still reachable");
    }
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _pop () an integer.
START_TEST(test_msg_pop_i)
{
    sam_selftest_introduce ("test_msg_pop_i");

    zmsg_t *zmsg = zmsg_new ();
    char *nbr = "1337";
    int rc = zmsg_pushstr (zmsg, nbr);
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    int ref;
    rc = sam_msg_pop (msg, "i", &ref);

    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (ref, atoi (nbr));
    ck_assert_int_eq (sam_msg_size (msg), 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _pop () a char pointer.
START_TEST(test_msg_pop_s)
{
    sam_selftest_introduce ("test_msg_pop_s");

    zmsg_t *zmsg = zmsg_new ();
    char *str = "hi!";
    int rc = zmsg_pushstr (zmsg, str);
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    char *ref;
    rc = sam_msg_pop (msg, "s", &ref);

    ck_assert_int_eq (rc, 0);
    ck_assert_str_eq (ref, str);
    ck_assert_int_eq (sam_msg_size (msg), 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _pop () a zframe pointer.
START_TEST(test_msg_pop_f)
{
    sam_selftest_introduce ("test_msg_pop_f");

    zmsg_t *zmsg = zmsg_new ();
    char payload = 'a';

    zframe_t
        *frame = zframe_new (&payload, sizeof (payload)),
        *ref = zframe_dup (frame);


    int rc = zmsg_push (zmsg, frame);
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    zframe_t *popped;
    rc = sam_msg_pop (msg, "f", &popped);

    ck_assert_int_eq (rc, 0);
    ck_assert (zframe_eq (ref, popped));
    ck_assert_int_eq (sam_msg_size (msg), 0);

    zframe_destroy (&ref);
    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _pop () a void pointer.
START_TEST(test_msg_pop_p)
{
    sam_selftest_introduce ("test_msg_pop_p");

    zmsg_t *zmsg = zmsg_new ();
    void *ptr = (void *) 0xfabfab;
    int rc = zmsg_pushmem (zmsg, &ptr, sizeof (ptr));
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    void *ref;
    rc = sam_msg_pop (msg, "p", &ref);

    ck_assert_int_eq (rc, 0);
    ck_assert (ref == ptr);
    ck_assert_int_eq (sam_msg_size (msg), 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _pop () a zlist_t *
START_TEST(test_msg_pop_l)
{
    sam_selftest_introduce ("test_msg_pop_h");

    zmsg_t *zmsg = zmsg_new ();

    if (zmsg_pushstr (zmsg, "value2") ||
        zmsg_pushstr (zmsg, "value1") ||
        zmsg_pushstr (zmsg, "2")) {

        ck_abort_msg ("could not build zmsg");
    }


    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 3);

    zlist_t *list;
    int rc = sam_msg_pop (msg, "l", &list);

    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 0);

    ck_assert_int_eq (zlist_size (list), 2);
    ck_assert_str_eq (zlist_first (list), "value1");
    ck_assert_str_eq (zlist_next (list), "value2");
    ck_assert (zlist_next (list) == NULL);

    zlist_destroy (&list);
    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _pop () an empty list
START_TEST(test_msg_pop_l_empty)
{
    sam_selftest_introduce ("test_msg_pop_l_empty");

    zmsg_t *zmsg = zmsg_new ();

    if (zmsg_pushstr (zmsg, "0")) {
        ck_abort_msg ("could not build zmsg");
    }

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    zlist_t *list;
    int rc = sam_msg_pop (msg, "l", &list);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 0);
    ck_assert_int_eq (zlist_size (list), 0);

    zlist_destroy (&list);
    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _pop () two succeesding lists
START_TEST(test_msg_pop_l_double)
{
    sam_selftest_introduce ("test_msg_pop_l_double");

    sam_selftest_introduce ("test_msg_get_l_double");
    zmsg_t *zmsg = zmsg_new ();

    if (zmsg_pushstr (zmsg, "two") ||
        zmsg_pushstr (zmsg, "1")   ||
        zmsg_pushstr (zmsg, "one") ||
        zmsg_pushstr (zmsg, "1")) {
        ck_abort_msg ("could not build zmsg");
    }

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 4);

    zlist_t *list1, *list2;
    int rc = sam_msg_pop (msg, "ll", &list1, &list2);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 0);

    ck_assert_int_eq (zlist_size (list1), 1);
    ck_assert_int_eq (zlist_size (list2), 1);

    ck_assert_str_eq (zlist_first (list1), "one");
    ck_assert_str_eq (zlist_first (list2), "two");

    zlist_destroy (&list1);
    zlist_destroy (&list2);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _pop () multiple different values.
START_TEST(test_msg_pop)
{
    sam_selftest_introduce ("test_msg_pop");

    zmsg_t *zmsg = zmsg_new ();

    // data to be pop()'d
    char *nbr = "17";
    char *str = "test";
    char a = 'a';
    zframe_t *char_frame = zframe_new (&a, sizeof (a));
    void *ptr = (void *) 0xbadc0de;


    // compose zmsg out of
    // a number (i)
    int rc = zmsg_pushstr (zmsg, nbr);
    ck_assert_int_eq (rc, 0);

    // a char * (s)
    rc = zmsg_pushstr (zmsg, str);
    ck_assert_int_eq (rc, 0);

    // a frame * (f)
    zframe_t *frame_dup = zframe_dup (char_frame);
    rc = zmsg_prepend (zmsg, &frame_dup);
    ck_assert_int_eq (rc, 0);

    // a void * (p)
    rc = zmsg_pushmem (zmsg, &ptr, sizeof (ptr));
    ck_assert_int_eq (rc, 0);


    // values to be filled
    int pic_nbr;
    char *pic_str;
    zframe_t *pic_frame;
    void *pic_ptr;


    // create message
    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 4);
    rc = sam_msg_pop (msg, "pfsi", &pic_ptr, &pic_frame, &pic_str, &pic_nbr);


    // test data
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 0);

    ck_assert (zframe_eq (char_frame, pic_frame));
    ck_assert_int_eq (pic_nbr, atoi (nbr));
    ck_assert_str_eq (pic_str, str);
    ck_assert_ptr_eq (pic_ptr, ptr);

    // clean up
    zframe_destroy (&char_frame);
    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _pop () when there are not enough frames.
START_TEST(test_msg_pop_insufficient_data)
{
    sam_selftest_introduce ("test_msg_pop_insufficient_data");

    zmsg_t *zmsg = zmsg_new ();
    int rc = zmsg_pushstr (zmsg, "one");
    ck_assert_int_eq (rc, 0);

    char *pic_str, *invalid;

    sam_msg_t *msg = sam_msg_new (&zmsg);
    rc = sam_msg_pop (msg, "ss", &pic_str, &invalid);
    ck_assert_int_eq (rc, -1);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// _pop () multiple times. Mainly used to test the garbage collection.
START_TEST(test_msg_pop_successively)
{
    sam_selftest_introduce ("test_msg_pop_successively");

    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "three");
    zmsg_pushstr (zmsg, "two");
    zmsg_pushstr (zmsg, "one");

    char payload = '0';
    zframe_t *frame = zframe_new (&payload, sizeof (payload));
    zmsg_push (zmsg, frame);

    sam_msg_t *msg = sam_msg_new (&zmsg);

    zframe_t *zero;
    char *one;

    int rc = sam_msg_pop (msg, "fs", &zero, &one);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 2);

    char *two, *three;
    rc = sam_msg_pop (msg, "ss", &two, &three);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test _size ().
START_TEST(test_msg_size)
{
    sam_selftest_introduce ("test_msg_size");

    zmsg_t *zmsg = zmsg_new ();
    sam_msg_t *msg = sam_msg_new (&zmsg);

    ck_assert_int_eq (sam_msg_size (msg), 0);
    sam_msg_destroy (&msg);

    zmsg = zmsg_new ();
    zmsg_addmem (zmsg, NULL, 0);
    zmsg_addmem (zmsg, NULL, 0);
    msg = sam_msg_new (&zmsg);

    ck_assert_int_eq (sam_msg_size (msg), 2);
    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test successive _size () calls in combination with _pop ().
START_TEST(test_msg_size_successively)
{
    sam_selftest_introduce ("test_msg_size_successively");

    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "something");
    zmsg_pushstr (zmsg, "something");

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 2);

    char *buf;
    int rc = sam_msg_pop (msg, "s", &buf);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 1);
    sam_msg_free (msg);

    rc = sam_msg_pop (msg, "s", &buf);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test _free ().
START_TEST(test_msg_free)
{
    sam_selftest_introduce ("test_msg_free");

    zmsg_t *zmsg = zmsg_new ();
    int rc = zmsg_pushstr (zmsg, "one");
    ck_assert_int_eq (rc, 0);

    rc = zmsg_pushstr (zmsg, "two");
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 2);

    char *pic_str;
    rc = sam_msg_pop (msg, "s", &pic_str);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 1);
    ck_assert_str_eq (pic_str, "two");

    sam_msg_free (msg);

    rc = sam_msg_pop (msg, "s", &pic_str);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 0);
    ck_assert_str_eq (pic_str, "one");

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _get () an integer.
START_TEST(test_msg_get_i)
{
    sam_selftest_introduce ("test_msg_get_i");

    zmsg_t *zmsg = zmsg_new ();
    char *nbr = "1337";
    int rc = zmsg_pushstr (zmsg, nbr);
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    int ref;
    rc = sam_msg_get (msg, "i", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 1);
    ck_assert_int_eq (ref, atoi (nbr));

    // check idempotency of _get ()
    ref = -1;
    rc = sam_msg_get (msg, "i", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 1);
    ck_assert_int_eq (ref, atoi (nbr));

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _get () a char pointer.
START_TEST(test_msg_get_s)
{
    sam_selftest_introduce ("test_msg_get_s");

    zmsg_t *zmsg = zmsg_new ();
    char *str = "hi!";
    int rc = zmsg_pushstr (zmsg, str);
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    char *ref;
    rc = sam_msg_get (msg, "s", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 1);
    ck_assert_str_eq (ref, str);
    free (ref);

    // check idempotency of _get ()
    ref = NULL;
    rc = sam_msg_get (msg, "s", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 1);
    ck_assert_str_eq (ref, str);
    free (ref);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _get () a zframe pointer.
START_TEST(test_msg_get_f)
{
    sam_selftest_introduce ("test_msg_get_f");

    zmsg_t *zmsg = zmsg_new ();
    char payload = 'a';
    zframe_t *frame = zframe_new (&payload, sizeof (payload));
    int rc = zmsg_push (zmsg, frame);
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    zframe_t *ref;
    rc = sam_msg_get (msg, "f", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 1);
    ck_assert (zframe_eq (ref, frame));
    zframe_destroy (&ref);

    // check idempotency of _get ()
    rc = sam_msg_get (msg, "f", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 1);
    ck_assert (zframe_eq (ref, frame));
    zframe_destroy (&ref);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _get () a void pointer.
START_TEST(test_msg_get_p)
{
    sam_selftest_introduce ("test_msg_get_p");

    zmsg_t *zmsg = zmsg_new ();
    void *ptr = (void *) 0xfabfab;
    int rc = zmsg_pushmem (zmsg, &ptr, sizeof (ptr));
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    void *ref;
    rc = sam_msg_get (msg, "p", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 1);
    ck_assert (ref == ptr);

    // check idempotency of _get ()
    ref = NULL;
    rc = sam_msg_get (msg, "p", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 1);
    ck_assert (ref == ptr);

    sam_msg_destroy (&msg);
}
END_TEST



//  --------------------------------------------------------------------------
/// Try to _get () a zlist_t
START_TEST(test_msg_get_l)
{
    sam_selftest_introduce ("test_msg_get_l");

    zmsg_t *zmsg = zmsg_new ();

    if (zmsg_pushstr (zmsg, "value2") ||
        zmsg_pushstr (zmsg, "value1") ||
        zmsg_pushstr (zmsg, "2")) {

        ck_abort_msg ("could not build zmsg");
    }

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 3);

    zlist_t *list;
    int rc = sam_msg_get (msg, "l", &list);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 3);

    ck_assert_int_eq (zlist_size (list), 2);
    ck_assert_str_eq (zlist_first (list), "value1");
    ck_assert_str_eq (zlist_next (list), "value2");
    ck_assert (zlist_next (list) == NULL);

    // check idempotency of _get ()
    zlist_destroy (&list);
    rc = sam_msg_get (msg, "l", &list);

    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 3);

    ck_assert_int_eq (zlist_size (list), 2);
    ck_assert_str_eq (zlist_first (list), "value1");
    ck_assert_str_eq (zlist_next (list), "value2");
    ck_assert (zlist_next (list) == NULL);

    zlist_destroy (&list);
    sam_msg_destroy (&msg);
}
END_TEST



//  --------------------------------------------------------------------------
/// Try to _get () a zlist_t * without any values
START_TEST(test_msg_get_l_empty)
{
    sam_selftest_introduce ("test_msg_get_l_empty");

    zmsg_t *zmsg = zmsg_new ();

    if (zmsg_pushstr (zmsg, "0")) {
        ck_abort_msg ("could not build zmsg");
    }

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    zlist_t *list;
    int rc = sam_msg_get (msg, "l", &list);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 1);
    ck_assert_int_eq (zlist_size (list), 0);

    zlist_destroy (&list);
    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _get () two consecutive lists
START_TEST(test_msg_get_l_double)
{
    sam_selftest_introduce ("test_msg_get_l_double");
    zmsg_t *zmsg = zmsg_new ();

    if (zmsg_pushstr (zmsg, "two") ||
        zmsg_pushstr (zmsg, "1")   ||
        zmsg_pushstr (zmsg, "one") ||
        zmsg_pushstr (zmsg, "1")) {
        ck_abort_msg ("could not build zmsg");
    }

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 4);

    zlist_t *list1, *list2;
    int rc = sam_msg_get (msg, "ll", &list1, &list2);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 4);

    ck_assert_int_eq (zlist_size (list1), 1);
    ck_assert_int_eq (zlist_size (list2), 1);

    ck_assert_str_eq (zlist_first (list1), "one");
    ck_assert_str_eq (zlist_first (list2), "two");

    zlist_destroy (&list1);
    zlist_destroy (&list2);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to skip a value in _get () with '?'.
START_TEST(test_msg_get_skipped)
{
    sam_selftest_introduce ("test_msg_get_skipped");

    zmsg_t *zmsg = zmsg_new ();
    int rc = zmsg_pushstr (zmsg, "foo");
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    rc = sam_msg_get (msg, "?");
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    // check idempotency of _get ()
    rc = sam_msg_get (msg, "?");
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Assert that '?' only skips if there is a value
START_TEST(test_msg_get_skipped_nonempty)
{
    sam_selftest_introduce ("test_msg_get_skipped_nonempty");

    zmsg_t *zmsg = zmsg_new ();

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 0);

    int rc = sam_msg_get (msg, "?");
    ck_assert_int_eq (rc, -1);
    ck_assert_int_eq (sam_msg_size (msg), 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _get () a combination of data.
START_TEST(test_msg_get)
{
    sam_selftest_introduce ("test_msg_get");

    zmsg_t *zmsg = zmsg_new ();
    char *str = "str 1", *nbr = "1";

    char c = 'a';
    zframe_t *frame = zframe_new (&c, sizeof (c));

    void *ptr = (void *) 0xbadc0de;
    zframe_t *ptr_f = zframe_new (&ptr, sizeof (ptr));


    // compose zmsg
    zmsg_addstr (zmsg, str);
    zmsg_addstr (zmsg, nbr);
    zmsg_addstr (zmsg, "skipped");

    zframe_t *frame_dup = zframe_dup (frame);
    zmsg_append (zmsg, &frame_dup);

    frame_dup = zframe_dup (ptr_f);
    zmsg_append (zmsg, &frame_dup);


    // create sam_msg
    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 5);


    // test idempotent _get ()
    int round_c;
    for (round_c = 0; round_c < 2; round_c++) {

        char *pic_str = NULL;
        int pic_nbr = -1;
        zframe_t *pic_frame = NULL;
        void *pic_ptr = NULL;

        int rc = sam_msg_get (
            msg, "si?fp", &pic_str, &pic_nbr, &pic_frame, &pic_ptr);
        ck_assert_int_eq (rc, 0);
        ck_assert_int_eq (sam_msg_size (msg), 5);

        ck_assert_str_eq (pic_str, str);
        ck_assert_int_eq (pic_nbr, atoi (nbr));
        ck_assert (zframe_eq (pic_frame, frame));
        ck_assert (pic_ptr == ptr);

        free (pic_str);
        zframe_destroy (&pic_frame);
    }

    zframe_destroy (&frame);
    zframe_destroy (&ptr_f);
    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _get () more data than present.
START_TEST(test_msg_get_insufficient_data)
{
    sam_selftest_introduce ("test_msg_get_insufficient_data");

    zmsg_t *zmsg = zmsg_new ();
    sam_msg_t *msg = sam_msg_new (&zmsg);
    int rc = sam_msg_get (msg, "s");

    ck_assert_int_eq (rc, -1);
    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test encoding and decoding.
START_TEST(test_msg_code)
{
    sam_selftest_introduce ("test_msg_code");

    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "two");
    zmsg_pushstr (zmsg, "one");
    sam_msg_t *msg = sam_msg_new (&zmsg);

    // one byte per frame for frame sizes < 255 and 2 * 4 bytes
    size_t size = sam_msg_encoded_size (msg);
    ck_assert_int_eq (size, 8);

    // encode message to the buffer
    byte *buf = malloc (size);
    assert (buf);
    sam_msg_encode (msg, &buf);
    sam_msg_destroy (&msg);

    msg = sam_msg_decode (buf, size);
    ck_assert_int_eq (sam_msg_size (msg), 2);
    free (buf);

    char *one, *two;
    int rc = sam_msg_pop (msg, "ss", &one, &two);
    ck_assert_int_eq (rc, 0);
    ck_assert_str_eq (one, "one");
    ck_assert_str_eq (two, "two");
    ck_assert_int_eq (sam_msg_size (msg), 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test encoding and decoding when part of the data is already pop'd.
START_TEST(test_msg_code_pop)
{
    sam_selftest_introduce ("test_msg_code_pop");

    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "two");
    zmsg_pushstr (zmsg, "one");
    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 2);

    char *one;
    int rc = sam_msg_pop (msg, "s", &one);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    size_t size = sam_msg_encoded_size (msg);
    byte *buf = malloc (size);
    assert (buf);

    sam_msg_encode (msg, &buf);
    sam_msg_destroy (&msg);
    ck_assert_int_eq (size, 4);

    msg = sam_msg_decode (buf, size);
    ck_assert_int_eq (sam_msg_size (msg), 1);
    free (buf);

    char *two;
    rc = sam_msg_pop (msg, "s", &two);
    ck_assert_str_eq (two, "two");
    ck_assert_int_eq (sam_msg_size (msg), 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Expect at least one non-zero frame.
START_TEST(test_msg_expect_nonzero)
{
    sam_selftest_introduce ("test_msg_expect_nonzero");

    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "one");

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    int rc = sam_msg_expect (msg, 1, SAM_MSG_NONZERO);
    ck_assert_int_eq (rc, 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Expect failure for empty messages.
START_TEST(test_msg_expect_nonzero_noframe)
{
    sam_selftest_introduce ("test_msg_expect_nonzero_noframe");

    zmsg_t *zmsg = zmsg_new ();

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 0);
    int rc = sam_msg_expect (msg, 1, SAM_MSG_NONZERO);
    ck_assert_int_eq (rc, -1);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Expect * frames bundled as a list
START_TEST(test_msg_expect_list)
{
    sam_selftest_introduce ("test_msg_expect_list");

    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "two");
    zmsg_pushstr (zmsg, "one");
    zmsg_pushstr (zmsg, "2");

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 3);

    int rc = sam_msg_expect (msg, 1, SAM_MSG_LIST);
    ck_assert_int_eq (rc, 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Expect failure when there are not enough items
START_TEST(test_msg_expect_list_less)
{
    sam_selftest_introduce ("test_msg_expect_list_less");

    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "one");
    zmsg_pushstr (zmsg, "2");

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 2);

    int rc = sam_msg_expect (msg, 1, SAM_MSG_LIST);
    ck_assert_int_eq (rc, -1);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Expect failure for empty messages.
START_TEST(test_msg_expect_list_noframe)
{
    sam_selftest_introduce ("test_msg_expect_list_noframe");

    zmsg_t *zmsg = zmsg_new ();

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 0);
    int rc = sam_msg_expect (msg, 1, SAM_MSG_LIST);
    ck_assert_int_eq (rc, -1);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Expect failure for empty frames.
START_TEST(test_msg_expect_nonzero_empty)
{
    sam_selftest_introduce ("test_msg_expect_nonzero_empty");

    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "");

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);
    int rc = sam_msg_expect (msg, 1, SAM_MSG_NONZERO);
    ck_assert_int_eq (rc, -1);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Expect at least one frame that may contain zero data.
START_TEST(test_msg_expect_zero)
{
    sam_selftest_introduce ("test_msg_expect_zero");

    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "");

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    int rc = sam_msg_expect (msg, 1, SAM_MSG_ZERO);
    ck_assert_int_eq (rc, 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Expect failure for empty messages.
START_TEST(test_msg_expect_zero_noframe)
{
    sam_selftest_introduce ("test_msg_expect_zero_noframe");

    zmsg_t *zmsg = zmsg_new ();

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 0);

    int rc = sam_msg_expect (msg, 1, SAM_MSG_ZERO);
    ck_assert_int_eq (rc, -1);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test a combination of expectations.
START_TEST(test_msg_expect)
{
    sam_selftest_introduce ("test_msg_expect");

    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "nonzero");
    zmsg_pushstr (zmsg, "");

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 2);

    int rc = sam_msg_expect (msg, 2, SAM_MSG_ZERO, SAM_MSG_NONZERO);
    ck_assert_int_eq (rc, 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test duplicating a sam_msg instance.
START_TEST(test_msg_dup)
{
    sam_selftest_introduce ("test_msg_dup");

    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "payload");

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 1);

    sam_msg_t *dup = sam_msg_dup (msg);

    sam_msg_destroy (&msg);

    char *payload;
    sam_msg_pop (dup, "s", &payload);
    ck_assert_str_eq (payload, "payload");

    sam_msg_destroy (&dup);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test if reference counting is handled independently.
START_TEST(test_msg_dup_refc)
{
    sam_selftest_introduce ("test_msg_dup_refc");

    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "two");
    zmsg_pushstr (zmsg, "one");

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_size (msg), 2);

    sam_msg_t *dup = sam_msg_dup (msg);
    sam_msg_own (dup);

    char *buf;
    sam_msg_pop (dup, "s", &buf);
    ck_assert_str_eq (buf, "one");

    sam_msg_pop (msg, "s", &buf);
    ck_assert_str_eq (buf, "one");

    sam_msg_destroy (&dup);

    sam_msg_pop (dup, "s", &buf);
    ck_assert_str_eq (buf, "two");

    sam_msg_pop (msg, "s", &buf);
    ck_assert_str_eq (buf, "two");

    sam_msg_destroy (&msg);
    sam_msg_destroy (&dup);
}
END_TEST


//  --------------------------------------------------------------------------
/// Self test this class.
void *
sam_msg_test ()
{
    Suite *s = suite_create ("sam_msg");

    TCase *tc = tcase_create("lifecycle (life)");
    tcase_add_test (tc, test_msg_life_basic);
    tcase_add_test (tc, test_msg_own);
    suite_add_tcase (s, tc);

    tc = tcase_create ("pop ()");
    tcase_add_test (tc, test_msg_pop_i);
    tcase_add_test (tc, test_msg_pop_s);
    tcase_add_test (tc, test_msg_pop_f);
    tcase_add_test (tc, test_msg_pop_p);
    tcase_add_test (tc, test_msg_pop_l);
    tcase_add_test (tc, test_msg_pop_l_empty);
    tcase_add_test (tc, test_msg_pop_l_double);
    tcase_add_test (tc, test_msg_pop);
    tcase_add_test (tc, test_msg_pop_insufficient_data);
    tcase_add_test (tc, test_msg_pop_successively);
    suite_add_tcase (s, tc);

    tc = tcase_create ("size ()");
    tcase_add_test (tc, test_msg_size);
    tcase_add_test (tc, test_msg_size_successively);
    suite_add_tcase (s, tc);

    tc = tcase_create ("free ()");
    tcase_add_test (tc, test_msg_free);
    suite_add_tcase (s, tc);

    tc = tcase_create ("get ()");
    tcase_add_test (tc, test_msg_get_i);
    tcase_add_test (tc, test_msg_get_s);
    tcase_add_test (tc, test_msg_get_f);
    tcase_add_test (tc, test_msg_get_p);
    tcase_add_test (tc, test_msg_get_l);
    tcase_add_test (tc, test_msg_get_l_empty);
    tcase_add_test (tc, test_msg_get_l_double);
    tcase_add_test (tc, test_msg_get_skipped);
    tcase_add_test (tc, test_msg_get_skipped_nonempty);
    tcase_add_test (tc, test_msg_get);
    tcase_add_test (tc, test_msg_get_insufficient_data);
    suite_add_tcase (s, tc);

    tc = tcase_create ("encode () and decode ()");
    tcase_add_test (tc, test_msg_code);
    tcase_add_test (tc, test_msg_code_pop);
    suite_add_tcase (s, tc);

    tc = tcase_create ("expect ()");
    tcase_add_test (tc, test_msg_expect_zero_noframe);
    tcase_add_test (tc, test_msg_expect_zero);
    tcase_add_test (tc, test_msg_expect_nonzero_empty);
    tcase_add_test (tc, test_msg_expect_nonzero_noframe);
    tcase_add_test (tc, test_msg_expect_list);
    tcase_add_test (tc, test_msg_expect_list_less);
    tcase_add_test (tc, test_msg_expect_list_noframe);
    tcase_add_test (tc, test_msg_expect);
    tcase_add_test (tc, test_msg_expect_nonzero);
    suite_add_tcase (s, tc);

    tc = tcase_create ("dup ()");
    tcase_add_test (tc, test_msg_dup);
    tcase_add_test (tc, test_msg_dup_refc);
    suite_add_tcase (s, tc);

    return s;
}
