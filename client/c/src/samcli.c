/*  =========================================================================

    samwise - best effort store and forward message publishing

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief samwise c client
   @file samwise.c


*/


#include "../include/samwise.h"


//  --------------------------------------------------------------------------
/// Print usage information.
static void
usage ()
{
    printf (
        "samcli CMD\n"
        "CMD = ping|publish\n\n");
}


//  --------------------------------------------------------------------------
/// Publish n messages to samd.
static void
publish (
    samwise_t *sam,
    int amount)
{
    assert (amount > 0);


    int count = 1;
    while (count <= amount) {
        char buf [64];
        snprintf (buf, 64, "message no %d", count);

        samwise_pub_t pub = {
            .exchange = "amq.direct",
            .routing_key = "",
            .size = strlen (buf),
            .msg = buf
        };

        printf ("publishing message %d\n", count);
        int rc = samwise_publish (sam, &pub);

        if (rc) {
            fprintf (stderr, "publishing failed\n");
        }

        count += 1;
    }
}


//  --------------------------------------------------------------------------
/// Entry point.
int
main (
    int arg_c,
    char **arg_v)
{
    arg_c -= 1;
    arg_v += 1;

    if (arg_c < 1) {
        usage ();
        return 2;
    }

    samwise_t *sam = samwise_new ();
    if (!sam) {
        fprintf (stderr, "an error occured, exiting\n");
        return 2;
    }

    if (!strcmp (arg_v [0], "ping")) {
        samwise_ping (sam);
    }

    else if (!strcmp (arg_v [0], "publish")) {
        publish (sam, 2);
    }

    else {
        usage ();
    }


    samwise_destroy (&sam);
    return 0;
}
