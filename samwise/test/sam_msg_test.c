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
    zmsg_t *zmsg = zmsg_new ();
    char *nbr = "1337";
    int rc = zmsg_pushstr (zmsg, nbr);
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);

    int ref;
    rc = sam_msg_pop (msg, "i", &ref);

    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (ref, atoi (nbr));

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _pop () a char pointer.
START_TEST(test_msg_pop_s)
{
    zmsg_t *zmsg = zmsg_new ();
    char *str = "hi!";
    int rc = zmsg_pushstr (zmsg, str);
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);

    char *ref;
    rc = sam_msg_pop (msg, "s", &ref);

    ck_assert_int_eq (rc, 0);
    ck_assert_str_eq (ref, str);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _pop () a zframe pointer.
START_TEST(test_msg_pop_f)
{
    zmsg_t *zmsg = zmsg_new ();
    char payload = 'a';
    zframe_t *frame = zframe_new (&payload, sizeof (payload));
    int rc = zmsg_push (zmsg, frame);
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);

    zframe_t *ref;
    rc = sam_msg_pop (msg, "f", &ref);

    ck_assert_int_eq (rc, 0);
    ck_assert (zframe_eq (ref, frame));

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _pop () a void pointer.
START_TEST(test_msg_pop_p)
{
    zmsg_t *zmsg = zmsg_new ();
    void *ptr = (void *) 0xfabfab;
    int rc = zmsg_pushmem (zmsg, &ptr, sizeof (ptr));
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);

    void *ref;
    rc = sam_msg_pop (msg, "p", &ref);

    ck_assert_int_eq (rc, 0);
    ck_assert (ref == ptr);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _pop () multiple different values.
START_TEST(test_msg_pop)
{
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
    ck_assert_int_eq (sam_msg_frames (msg), 4);
    rc = sam_msg_pop (msg, "pfsi", &pic_ptr, &pic_frame, &pic_str, &pic_nbr);


    // test data
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_frames (msg), 0);
    ck_assert (zframe_eq (char_frame, pic_frame));
    ck_assert_int_eq (pic_nbr, atoi (nbr));
    ck_assert_str_eq (pic_str, str);
    // ck_assert_ptr_eq (pic_ptr, ptr);
    ck_assert (pic_ptr == ptr);


    // clean up
    zframe_destroy (&char_frame);
    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _pop () when there are not enough frames.
START_TEST(test_msg_pop_insufficient_data)
{
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

    char *two, *three;
    rc = sam_msg_pop (msg, "ss", &two, &three);
    ck_assert_int_eq (rc, 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test _size ().
START_TEST(test_msg_size)
{
    zmsg_t *zmsg = zmsg_new ();
    sam_msg_t *msg = sam_msg_new (&zmsg);

    ck_assert_int_eq (sam_msg_frames (msg), 0);
    sam_msg_destroy (&msg);

    zmsg = zmsg_new ();
    zmsg_addmem (zmsg, NULL, 0);
    zmsg_addmem (zmsg, NULL, 0);
    msg = sam_msg_new (&zmsg);

    ck_assert_int_eq (sam_msg_frames (msg), 2);
    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test successive _size () calls in combination with _pop ().
START_TEST(test_msg_size_successively)
{
    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "something");
    zmsg_pushstr (zmsg, "something");

    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_frames (msg), 2);

    char *buf;
    int rc = sam_msg_pop (msg, "s", &buf);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_frames (msg), 1);
    sam_msg_free (msg);

    rc = sam_msg_pop (msg, "s", &buf);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_frames (msg), 0);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test _free ().
START_TEST(test_msg_free)
{
    zmsg_t *zmsg = zmsg_new ();
    int rc = zmsg_pushstr (zmsg, "one");
    ck_assert_int_eq (rc, 0);

    rc = zmsg_pushstr (zmsg, "two");
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);

    char *pic_str;
    rc = sam_msg_pop (msg, "s", &pic_str);
    ck_assert_int_eq (rc, 0);
    ck_assert_str_eq (pic_str, "two");

    sam_msg_free (msg);

    rc = sam_msg_pop (msg, "s", &pic_str);
    ck_assert_int_eq (rc, 0);
    ck_assert_str_eq (pic_str, "one");

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _contain () an integer.
START_TEST(test_msg_contain_i)
{
    zmsg_t *zmsg = zmsg_new ();
    char *nbr = "1337";
    int rc = zmsg_pushstr (zmsg, nbr);
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);

    rc = sam_msg_contain (msg, "i");
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (sam_msg_frames (msg), 0);

    int ref;
    rc = sam_msg_contained (msg, "i", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (ref, atoi (nbr));

    // check idempotency of _contained ()
    ref = -1;
    rc = sam_msg_contained (msg, "i", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (ref, atoi (nbr));

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _contain () a char pointer.
START_TEST(test_msg_contain_s)
{
    zmsg_t *zmsg = zmsg_new ();
    char *str = "hi!";
    int rc = zmsg_pushstr (zmsg, str);
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);

    rc = sam_msg_contain (msg, "s");
    ck_assert_int_eq (rc, 0);

    char *ref;
    rc = sam_msg_contained (msg, "s", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert_str_eq (ref, str);

    // check idempotency of _contained ()
    ref = NULL;
    rc = sam_msg_contained (msg, "s", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert_str_eq (ref, str);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _contain () a zframe pointer.
START_TEST(test_msg_contain_f)
{
    zmsg_t *zmsg = zmsg_new ();
    char payload = 'a';
    zframe_t *frame = zframe_new (&payload, sizeof (payload));
    int rc = zmsg_push (zmsg, frame);
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);

    rc = sam_msg_contain (msg, "f");
    ck_assert_int_eq (rc, 0);

    zframe_t *ref;
    rc = sam_msg_contained (msg, "f", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert (zframe_eq (ref, frame));

    // check idempotency of _contained ()
    ref = NULL;
    rc = sam_msg_contained (msg, "f", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert (zframe_eq (ref, frame));

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _contain () a void pointer.
START_TEST(test_msg_contain_p)
{
    zmsg_t *zmsg = zmsg_new ();
    void *ptr = (void *) 0xfabfab;
    int rc = zmsg_pushmem (zmsg, &ptr, sizeof (ptr));
    ck_assert_int_eq (rc, 0);

    sam_msg_t *msg = sam_msg_new (&zmsg);

    rc = sam_msg_contain (msg, "p");
    ck_assert_int_eq (rc, 0);

    void *ref;
    rc = sam_msg_contained (msg, "p", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert (ref == ptr);

    // check idempotency of _contained ()
    ref = NULL;
    rc = sam_msg_contained (msg, "p", &ref);
    ck_assert_int_eq (rc, 0);
    ck_assert (ref == ptr);

    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _contain () a combination of data.
START_TEST(test_msg_contain)
{
    zmsg_t *zmsg = zmsg_new ();
    char *str = "str 1", *nbr = "1";

    char c = 'a';
    zframe_t *frame = zframe_new (&c, sizeof (c));

    void *ptr = (void *) 0xbadc0de;
    zframe_t *ptr_f = zframe_new (&ptr, sizeof (ptr));


    // compose zmsg
    zmsg_addstr (zmsg, str);
    zmsg_addstr (zmsg, nbr);

    zframe_t *frame_dup = zframe_dup (frame);
    zmsg_append (zmsg, &frame_dup);

    frame_dup = zframe_dup (ptr_f);
    zmsg_append (zmsg, &frame_dup);


    // create sam_msg
    sam_msg_t *msg = sam_msg_new (&zmsg);
    ck_assert_int_eq (sam_msg_frames (msg), 4);

    sam_msg_contain (msg, "sifp");
    ck_assert_int_eq (sam_msg_frames (msg), 0);


    // test idempotent _contained ()
    int round_c;
    for (round_c = 0; round_c < 2; round_c++) {

        char *pic_str = NULL;
        int pic_nbr = -1;
        zframe_t *pic_frame = NULL;
        void *pic_ptr = NULL;

        int rc = sam_msg_contained (
            msg, "sifp", &pic_str, &pic_nbr, &pic_frame, &pic_ptr);
        ck_assert_int_eq (rc, 0);

        ck_assert_str_eq (pic_str, str);
        ck_assert_int_eq (pic_nbr, atoi (nbr));
        ck_assert (zframe_eq (pic_frame, frame));
        ck_assert (pic_ptr == ptr);
    }

    zframe_destroy (&frame);
    zframe_destroy (&ptr_f);
    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to _contain () more data than present.
START_TEST(test_msg_contain_insufficient_data)
{
    zmsg_t *zmsg = zmsg_new ();
    sam_msg_t *msg = sam_msg_new (&zmsg);
    int rc = sam_msg_contain (msg, "s");

    ck_assert_int_eq (rc, -1);
    sam_msg_destroy (&msg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Try to retrieve more data than _contained ().
START_TEST(test_msg_contained_insufficient_data)
{
    zmsg_t *zmsg = zmsg_new ();
    zmsg_pushstr (zmsg, "hi!");
    sam_msg_t *msg = sam_msg_new (&zmsg);

    int rc = sam_msg_contain (msg, "s");
    ck_assert_int_eq (rc, 0);

    char *one, *two;
    rc = sam_msg_contained (msg, "ss", &one, &two);
    ck_assert_int_eq (rc, -1);

    sam_msg_destroy (&msg);
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

    tc = tcase_create ("contain ()");
    tcase_add_test (tc, test_msg_contain_i);
    tcase_add_test (tc, test_msg_contain_s);
    tcase_add_test (tc, test_msg_contain_f);
    tcase_add_test (tc, test_msg_contain_p);
    tcase_add_test (tc, test_msg_contain);
    tcase_add_test (tc, test_msg_contain_insufficient_data);
    tcase_add_test (tc, test_msg_contained_insufficient_data);
    suite_add_tcase (s, tc);

    return s;
}
