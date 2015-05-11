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
#include <czmq.h>


struct samwise_t {
    int TMP;
};


samwise_t *
samwise_new ()
{
    samwise_t *self = malloc (sizeof (samwise_t));
    assert (self);

    self->TMP = 0;

    return self;
}


void
samwise_destroy (
    samwise_t **self)
{
    assert (*self);

    free (*self);
    *self = NULL;
}


int
main (void)
{
    return 0;
}

