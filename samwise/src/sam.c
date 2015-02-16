/*  =========================================================================

    samwise - best effort store and forward message publishing

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
sam_t *
sam_new ()
{
    sam_t *self = malloc (sizeof (sam_t));
    assert (self);

    self->logger = sam_logger_new ("sam", SAM_LOG_ENDPOINT);
    return self;
}


//  --------------------------------------------------------------------------
void
sam_destroy (sam_t **self)
{
    assert (*self);
    sam_logger_destroy (&(*self)->logger);
    *self = NULL;
}


//  --------------------------------------------------------------------------
int
sam_config (sam_t *self, const char *conf UU)
{
    sam_log_error (self->logger, "sam_config is not yet implemented");
    return 0;
}


//  --------------------------------------------------------------------------
int
sam_publish (sam_t *self, zmsg_t *msg UU)
{
    sam_log_trace (self->logger, "got publishing request");
    return 0;
}


//  --------------------------------------------------------------------------
int
sam_stats (sam_t *self)
{
    sam_log_error (self->logger, "sam_stats is not yet implemented!");
    return 0;
}


//  --------------------------------------------------------------------------
/// Invokes the test functions of all components and finally tests itself.
void
sam_test ()
{
    // sam_log_test ();
    // sam_msg_rabbitmq_test ();
    // sam_msg_test ();

    sam_t *sam = sam_new ();
    assert (sam);

    sam_destroy (&sam);
    assert (!sam);
}


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
    // sam_msg_test ();

    zclock_sleep (100);
    sam_log_destroy (&log);
    return 0;
}
