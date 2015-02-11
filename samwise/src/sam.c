/*  =========================================================================

    samwise - reliable message publishing

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief public API
   @file sam.c

   TODO description

*/

#include "../include/sam_prelude.h"



//  --------------------------------------------------------------------------
/// Main entry function.
/// Runs the test, currently.
int
main (void)
{
    printf ("running tests\n");
    // sam_log_test ();

    sam_log_t *log = sam_log_new (SAM_LOG_ENDPOINT);
    sam_log_add_handler (log, SAM_LOG_LVL_TRACE, SAM_LOG_HANDLER_STD);

    sam_msg_rabbitmq_test ();

    zclock_sleep (10);
    sam_log_destroy (&log);
    return 0;
}
