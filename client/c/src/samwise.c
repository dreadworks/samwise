/*  =========================================================================

    samwise - best effort store and forward message publishing

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief samwise c client library
   @file samwise.c


*/


#include "../include/samwise.h"
#include <czmq.h>


/// samwise state
struct samwise_t {
    zsock_t *req;   ///< request socket communicating with samd
};


//  --------------------------------------------------------------------------
/// Read the response from samd and print any errors that may arise.
static int
handle_response (
    samwise_t *self)
{
    int code;
    char *msg;

    zsock_recv (self->req, "is", &code, &msg);
    if (code) {
        fprintf (stderr, "received error '%d': %s\n", code, msg);
    }

    free (msg);
    return code;
}


//  --------------------------------------------------------------------------
/// Creates an empty message and appends the protocol version.
static zmsg_t *
create_msg ()
{
    zmsg_t *msg = zmsg_new ();
    zmsg_addstr (msg, "100");
    return msg;
}


//  --------------------------------------------------------------------------
/// Publish a message to samd.
int
samwise_publish (
    samwise_t *self,
    samwise_pub_t *pub)
{
    zmsg_t *msg = create_msg ();
    zmsg_addstr (msg, "publish");


    // distribution type
    if (pub->disttype == SAMWISE_ROUNDROBIN) {
        zmsg_addstr (msg, "round robin");
    }
    else if (pub->disttype == SAMWISE_REDUNDANT) {
        zmsg_addstr (msg, "redundant");

        char buf [128];
        snprintf (buf, 128, "%d", pub->distcount);

        zmsg_addstr (msg, buf);
    }
    else {
        assert (false);
    }


    // amqp options
    zmsg_addstr (msg, pub->exchange);
    zmsg_addstr (msg, pub->routing_key);
    zmsg_addstr (msg, "0"); // mandatory
    zmsg_addstr (msg, "0"); // immediate


    // rabbitmq options
    zmsg_addstr (msg, "12");
    zmsg_addmem (msg, NULL, 0);
    zmsg_addmem (msg, NULL, 0);
    zmsg_addmem (msg, NULL, 0);
    zmsg_addmem (msg, NULL, 0);
    zmsg_addmem (msg, NULL, 0);
    zmsg_addmem (msg, NULL, 0);
    zmsg_addmem (msg, NULL, 0);
    zmsg_addmem (msg, NULL, 0);
    zmsg_addmem (msg, NULL, 0);
    zmsg_addmem (msg, NULL, 0);
    zmsg_addmem (msg, NULL, 0);
    zmsg_addmem (msg, NULL, 0);


    // header
    zmsg_addstr (msg, "0");


    // payload
    zmsg_addmem (msg, pub->msg, pub->size);

    zmsg_send (&msg, self->req);
    return handle_response (self);
}


//  --------------------------------------------------------------------------
/// Ping samd.
int
samwise_ping (
    samwise_t *self)
{
    zmsg_t *msg = create_msg ();
    zmsg_addstr (msg, "ping");

    zmsg_send (&msg, self->req);
    return handle_response (self);
}


//  --------------------------------------------------------------------------
/// Create a new samwise instance and connect to samd's endpoint.
samwise_t *
samwise_new (
    const char *endpoint)
{
    assert (endpoint);

    samwise_t *self = malloc (sizeof (samwise_t));
    assert (self);

    self->req = zsock_new_req (endpoint);
    if (!self->req) {
        fprintf (stderr, "could not connect to endpoint\n");
        return NULL;
    }

    return self;
}


//  --------------------------------------------------------------------------
/// destroy a samwise instance.
void
samwise_destroy (
    samwise_t **self)
{
    assert (*self);

    zsock_destroy (&(*self)->req);

    free (*self);
    *self = NULL;
}
