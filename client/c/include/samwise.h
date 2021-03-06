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
#define SAMWISE_PROTOCOL_VERSION "120"


typedef struct samwise_t samwise_t;


typedef enum {
    SAMWISE_ROUNDROBIN,
    SAMWISE_REDUNDANT
} samwise_disttype_t;


/// publishing request options
typedef struct samwise_pub_t {

    samwise_disttype_t disttype;    ///< either round robin or redundant
    int distcount;                  ///< for disttype=SAMWISE_REDUNDANT

    char *exchange;
    char *routing_key;
    int mandatory;
    int immediate;

    struct {
        char *content_type;
        char *content_encoding;
        char *delivery_mode;
        char *priority;
        char *correlation_id;
        char *reply_to;
        char *expiration;
        char *message_id;
        char *type;
        char *user_id;
        char *app_id;
        char *cluster_id;
    } options;

    struct {
        char **keys;
        char **values;
        size_t amount;
    } headers;

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
/// @param endpoint Public endpoint of a samd instance
/// @return The newly created instance
samwise_t *
samwise_new (
    const char *endpoint);


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
