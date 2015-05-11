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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


#ifdef __cplusplus
extern "C" {
#endif


#define UU __attribute__((unused))


typedef struct samwise_t samwise_t;


/// publishing request options
typedef struct samwise_pub_t {

    char *exchange;
    char *routing_key;

    char *msg;
    size_t size;

} samwise_pub_t;


//  --------------------------------------------------------------------------
/// @brief Publish a message to samd
/// @param self A samwise instance
/// @param pub Publishing options
/// @return 0 if success, -1 otherwise
int
samwise_publish (
    samwise_t *self,
    samwise_pub_t *pub);


//  --------------------------------------------------------------------------
/// @brief Send a ping to samwise
/// @param self A samwise instance
/// @return 0 if success, -1 otherwise
int
samwise_ping (
    samwise_t *self);


//  --------------------------------------------------------------------------
/// @brief Create a new samwise instance
/// @return The newly created instance
samwise_t *
samwise_new ();


// --------------------------------------------------------------------------
/// @brief Destroy a samwise instance
/// @param self Pointer to be free'd and nullified
void
samwise_destroy (
    samwise_t **self);


#ifdef __cplusplus
}
#endif

#endif
