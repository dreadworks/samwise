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


// to be removed before shipping. for debugging
static void
sigabrt (int signo UU)
{
    sam_logger_t *logger = sam_logger_new ("sigabrt", SAM_LOG_ENDPOINT);
    sam_log_error (logger, "catched SIGABRT");
    sam_logger_destroy (&logger);
    zclock_sleep (10);
}


//  --------------------------------------------------------------------------
/// Main entry function.
int
main (void)
{
    sam_log_t *log = sam_log_new (SAM_LOG_ENDPOINT);
    sam_log_add_handler (log, SAM_LOG_LVL_TRACE, SAM_LOG_HANDLER_STD);

    signal (SIGABRT, sigabrt);

    // playground
    //playground_publish_loop ();

    // tests
    sam_msg_rabbitmq_test ();
    //sam_msg_test ();

    zclock_sleep (100);
    sam_log_destroy (&log);
    return 0;
}
