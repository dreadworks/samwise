/*  =========================================================================

    sam_gen_test - Test sam_gen

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/

#include "../include/sam_prelude.h"


//  --------------------------------------------------------------------------
/// Actor entry function to test the generic handle_pipe.
static void
test_actor (zsock_t *pipe, void *args UU)
{
    zloop_t *loop = zloop_new ();
    zloop_reader (loop, pipe, sam_gen_handle_pipe, NULL);
    zsock_signal (pipe, 0);
    zloop_start (loop);
    zloop_destroy (&loop);
}


//  --------------------------------------------------------------------------
/// Test if the actor exits cleanly by using the gen_pipe callback.
START_TEST(test_sam_handle_pipe)
{
    sam_selftest_introduce ("test_sam_handle_pipe");

    zactor_t *actor = zactor_new (test_actor, NULL);
    if (!actor) {
        ck_abort_msg ("no actor created");
    }

    zactor_destroy (&actor);
    if (actor) {
        ck_abort_msg ("actor was not destroyed");
    }
}
END_TEST


//  --------------------------------------------------------------------------
/// Self test sam_gen.
void *
sam_gen_test ()
{
    Suite *s = suite_create ("sam_gen");
    TCase *tc_handle_pipe = tcase_create("handle pipe");

    tcase_add_test (tc_handle_pipe, test_sam_handle_pipe);
    suite_add_tcase (s, tc_handle_pipe);

    return s;
}
