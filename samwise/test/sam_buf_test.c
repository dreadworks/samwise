/*  =========================================================================

    sam_buf_test - Test the buffer

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/


#include "../include/sam_prelude.h"


zsock_t
    *backend_push,  // messages arriving from backends
    *frontend_pull; // messages distributed by libsam

sam_buf_t *buf;


static void
setup ()
{
    char *endpoint = "inproc://test-buf_be";
    zsock_t *backend_pull = zsock_new_pull (endpoint);
    backend_push = zsock_new_push (endpoint);

    endpoint = "inproc://test-buf_fe";
    zsock_t *frontend_push = zsock_new_push (endpoint);
    frontend_pull = zsock_new_pull (endpoint);

    buf = sam_buf_new ("test.db", &backend_pull, &frontend_push);

    if (!buf) {
        ck_abort_msg ("buf instance was not created");
    }

    if (backend_pull) {
        ck_abort_msg ("backend pull socket still reachable");
    }

    if (frontend_push) {
        ck_abort_msg ("frontend push still reachable");
    }

}


static void
destroy ()
{
    sam_buf_destroy (&buf);
    zsock_destroy (&backend_push);
    zsock_destroy (&frontend_pull);
}



START_TEST(test_buf_save)
{
    // int key = sam_buf_save ();
}
END_TEST


void *
sam_buf_test ()
{
    Suite *s = suite_create ("sam_buf");

    TCase *tc = tcase_create("config");
    tcase_add_unchecked_fixture (tc, setup, destroy);
    // tcase_add_test (tc, test_buf_save);
    suite_add_tcase (s, tc);

    return s;
}
