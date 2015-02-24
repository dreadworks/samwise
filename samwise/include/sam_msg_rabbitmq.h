/*  =========================================================================

    sam_msg_rabbit - message backend for RabbitMQ

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

#ifndef __SAM_MSG_RABBIT_H__
#define __SAM_MSG_RABBIT_H__


/// the msg_rabbitmq state
typedef struct sam_msg_rabbitmq_t {
    unsigned int id;

    struct {                                ///< amqp connection
        amqp_connection_state_t connection; ///< internal connection state
        amqp_socket_t *socket;              ///< tcp socket holding the conn
        int message_channel;                ///< channel for messages
        int method_channel;                 ///< channel for rpc calls
        int seq;                            ///< incremented number for acks
    } amqp;

    zsock_t *rep;                  ///< accepting publish/rpc requests
    zsock_t *psh;                  ///< pushing ack's as a generic backend
    zmq_pollitem_t *amqp_pollitem; ///< amqp tcp socket wrapped as pollitem

} sam_msg_rabbitmq_t;


/// option set to pass for backend creation
typedef struct sam_msg_rabbitmq_opts_t {
    char *host;     ///< hostname, mostly some ip address
    int  port;      ///< port the broker listens to
    char *user;     ///< username
    char *pass;     ///< password
    int heartbeat;  ///< heartbeat interval in seconds
} sam_msg_rabbitmq_opts_t;


//  --------------------------------------------------------------------------
/// @brief Returns the underlying socket of the broker connection
/// @return The TCP socket's file descriptor
int
sam_msg_rabbitmq_sockfd (
    sam_msg_rabbitmq_t *self);


//  --------------------------------------------------------------------------
/// @brief Create a new RabbitMQ connection
/// @return New msg_rabbitmq instance
sam_msg_rabbitmq_t *
sam_msg_rabbitmq_new ();


//  --------------------------------------------------------------------------
/// @brief Destroy a RabbitMQ connection
/// @param self A msg_rabbitmq instance
void
sam_msg_rabbitmq_destroy (
    sam_msg_rabbitmq_t **self);


//  --------------------------------------------------------------------------
/// @brief Connect to a RabbitMQ broker
/// @param opts The connection parameters
void
sam_msg_rabbitmq_connect (
    sam_msg_rabbitmq_t *self,
    sam_msg_rabbitmq_opts_t *opts);


//  --------------------------------------------------------------------------
/// @brief Publish a message to the broker
/// @param self A msg_rabbitmq instance
/// @param msg  The message as structured data
int
sam_msg_rabbitmq_publish (
    sam_msg_rabbitmq_t *self,
    const char *exchange,
    const char *routing_key,
    byte *payload,
    int payload_len);


//  --------------------------------------------------------------------------
/// @brief Read all buffered confirm.select acknowledgements
/// @param self A msg_rabbitmq instance
void
sam_msg_rabbitmq_handle_ack (
    sam_msg_rabbitmq_t *self);


//  --------------------------------------------------------------------------
/// @brief Declare an exchange
/// @param self A msg_rabbitmq instance
/// @param exchange Name of the exchange
/// @param type One of the AMQ-exchange types
void
sam_msg_rabbitmq_exchange_declare (
    sam_msg_rabbitmq_t *self,
    const char *exchange,
    const char *type);


//  --------------------------------------------------------------------------
/// @brief Delete an exchange
/// @param self A msg_rabbitmq instance
/// @param exchange Name of the exchange
void
sam_msg_rabbitmq_exchange_delete (
    sam_msg_rabbitmq_t *self,
    const char *exchange);


//  --------------------------------------------------------------------------
/// @brief Start an actor handling requests asynchronously
/// @param self A msg_rabbitmq instance
/// @return Actor handling the internal loop
sam_backend_t *
sam_msg_rabbitmq_start (
    sam_msg_rabbitmq_t **self,
    char *pll_endpoint);


//  --------------------------------------------------------------------------
/// @brief Stop the actor and free all allocated memory
/// @param self A msg_rabbitmq msg_backend instance
/// @return Reclaimed msg_rabbitmq instance
sam_msg_rabbitmq_t *
sam_msg_rabbitmq_stop (
    sam_backend_t **backend);


//  --------------------------------------------------------------------------
/// @brief Self test this class
void
sam_msg_rabbitmq_test ();


#endif
