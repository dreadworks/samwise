/*  =========================================================================

    sam_be_rmq - message backend for RabbitMQ

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT


    =========================================================================
*/
/**

   @brief message backend for RabbitMQ

   This class should mainly be used by invoking it's start
   function. It then handles publishing requests, acknowledgements
   from the channel and heartbeats.

*/

#ifndef __SAM_BE_RMQ_H__
#define __SAM_BE_RMQ_H__


/// the be_rmq state
typedef struct sam_be_rmq_t {
    char *name;

    struct {                                ///< amqp connection
        amqp_connection_state_t connection; ///< internal connection state
        amqp_socket_t *socket;              ///< tcp socket holding the conn
        int message_channel;                ///< channel for messages
        int method_channel;                 ///< channel for rpc calls
        int seq;                            ///< incremented number for acks
    } amqp;

    zsock_t *publish_pll;          ///< accepting publishing requests
    zsock_t *rpc_rep;              ///< accepting rpc requests
    zsock_t *psh;                  ///< pushing ack's as a generic backend
    zmq_pollitem_t *amqp_pollitem; ///< amqp tcp socket wrapped as pollitem

} sam_be_rmq_t;


/// option set to pass for backend creation
typedef struct sam_be_rmq_opts_t {
    char *host;     ///< hostname, mostly some ip address
    int  port;      ///< port the broker listens to
    char *user;     ///< username
    char *pass;     ///< password
    int heartbeat;  ///< heartbeat interval in seconds
} sam_be_rmq_opts_t;


//  --------------------------------------------------------------------------
/// @brief Returns the underlying socket of the broker connection
/// @return The TCP socket's file descriptor
int
sam_be_rmq_sockfd (
    sam_be_rmq_t *self);


//  --------------------------------------------------------------------------
/// @brief Create a new RabbitMQ connection
/// @return New be_rmq instance
sam_be_rmq_t *
sam_be_rmq_new (const char *name);


//  --------------------------------------------------------------------------
/// @brief Destroy a RabbitMQ connection
/// @param self A be_rmq instance
void
sam_be_rmq_destroy (
    sam_be_rmq_t **self);


//  --------------------------------------------------------------------------
/// @brief Connect to a RabbitMQ broker
/// @param opts The connection parameters
void
sam_be_rmq_connect (
    sam_be_rmq_t *self,
    sam_be_rmq_opts_t *opts);


//  --------------------------------------------------------------------------
/// @brief Publish a message to the broker
/// @param self A be_rmq instance
/// @param msg  The message as structured data
int
sam_be_rmq_publish (
    sam_be_rmq_t *self,
    const char *exchange,
    const char *routing_key,
    byte *payload,
    int payload_len);


//  --------------------------------------------------------------------------
/// @brief Read all buffered confirm.select acknowledgements
/// @param self A be_rmq instance
void
sam_be_rmq_handle_ack (
    sam_be_rmq_t *self);


//  --------------------------------------------------------------------------
/// @brief Declare an exchange
/// @param self A be_rmq instance
/// @param exchange Name of the exchange
/// @param type One of the AMQ-exchange types
/// @return 0 for success, -1 for error
int
sam_be_rmq_exchange_declare (
    sam_be_rmq_t *self,
    const char *exchange,
    const char *type);


//  --------------------------------------------------------------------------
/// @brief Delete an exchange
/// @param self A be_rmq instance
/// @param exchange Name of the exchange
/// @return 0 for success, -1 for error
int
sam_be_rmq_exchange_delete (
    sam_be_rmq_t *self,
    const char *exchange);


//  --------------------------------------------------------------------------
/// @brief Start an actor handling requests asynchronously
/// @param self A be_rmq instance
/// @return Actor handling the internal loop
sam_backend_t *
sam_be_rmq_start (
    sam_be_rmq_t **self,
    char *pll_endpoint);


//  --------------------------------------------------------------------------
/// @brief Stop the actor and free all allocated memory
/// @param self A be_rmq msg_backend instance
/// @return Reclaimed be_rmq instance
sam_be_rmq_t *
sam_be_rmq_stop (
    sam_backend_t **backend);


//  --------------------------------------------------------------------------
/// @brief Self test this class
void *
sam_be_rmq_test ();


#endif
