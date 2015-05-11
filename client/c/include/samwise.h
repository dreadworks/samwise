/*  =========================================================================

    samwise - reliable message publishing

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT


    =========================================================================
*/
/**

   @brief Samwise API interface

   Offers all public functions necessary to store and forward
   messages.

*/

#ifndef __SAMWISE_H__
#define __SAMWISE_H__


typedef struct samwise_t samwise_t;


samwise_t *
samwise_new ();


void
samwise_destroy (
    samwise_t **self);


#endif
