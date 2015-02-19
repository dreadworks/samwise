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

    // request/response multiplexing
    sam_log_infof (
        "bound public endpoint: %s",
        SAM_PUBLIC_ENDPOINT);

    sam_log_info ("created sam");
    return self;
}


//  --------------------------------------------------------------------------
void
sam_destroy (sam_t **self)
{
    assert (*self);

    if ((*self)->backends) {
        sam_msg_destroy (&(*self)->backends);
    }

    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
int
sam_init (sam_t *self, const char *conf UU)
{
    // TODO create shared config (#32)
    // for now, create one backend pool
    // containing one rabbitmq backend instance
    self->backends = sam_msg_new ("rabbitmq");
    assert (self->backends);

    sam_msg_rabbitmq_opts_t rabbitmq_opts = {
        .host = "localhost",
        .port = 5672,
        .user = "guest",
        .pass = "guest",
        .heartbeat = 3
    };

    int rc = sam_msg_create_backend (
        self->backends, SAM_MSG_BE_RABBITMQ, &rabbitmq_opts);

    if (rc != 0) {
        sam_log_error ("could not create rabbitmq backend");
    }

    return 0;
}


//  --------------------------------------------------------------------------
int
sam_publish (sam_t *self, zmsg_t *msg)
{
    sam_log_trace ("got publishing request");
    return sam_msg_publish (self->backends, msg);
}


//  --------------------------------------------------------------------------
int
sam_stats (sam_t *self UU)
{
    sam_log_error ("sam_stats is not yet implemented!");
    return 0;
}


//  --------------------------------------------------------------------------
/// Invokes the test functions of all components and finally tests itself.
void
sam_test ()
{
    sam_msg_rabbitmq_test ();
    sam_msg_test ();

    sam_t *sam = sam_new ();
    assert (sam);

    sam_destroy (&sam);
    assert (!sam);
}
