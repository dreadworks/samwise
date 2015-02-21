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
/// Create a new sam instance.
sam_t *
sam_new ()
{
    sam_t *self = malloc (sizeof (sam_t));
    self->backends = NULL;
    assert (self);

    // request/response multiplexing
    sam_log_infof (
        "bound public endpoint: %s",
        SAM_PUBLIC_ENDPOINT);

    sam_log_info ("created sam");
    return self;
}


//  --------------------------------------------------------------------------
/// Free's all allocated memory and destroys the sam_msg instance if
/// initialized.
void
sam_destroy (sam_t **self)
{
    assert (*self);

    if ((*self)->backends) {
        sam_msg_destroy (&(*self)->backends);
    }

    free (*self);
    *self = NULL;
    sam_log_info ("destroyed sam");
}


//  --------------------------------------------------------------------------
/// Read from a configuration file (TODO #32). Currently just
/// statically creates a sam_msg instance with one rabbitmq broker
/// connection.
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

    sam_log_info ("(re)loaded configuration");
    return 0;
}


//  --------------------------------------------------------------------------
/// TODO analyze message content before passing it on, part of #7.
int
sam_publish (sam_t *self, zmsg_t *msg)
{
    sam_log_trace ("got publishing request");
    return sam_msg_publish (self->backends, msg);
}


//  --------------------------------------------------------------------------
/// Self test this class.
void
sam_test ()
{
    printf ("\n** SAM **\n");

    sam_t *sam = sam_new ();
    assert (sam);

    // TODO sam_init

    sam_destroy (&sam);
    assert (!sam);
}
