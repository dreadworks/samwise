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


/// option set to pass for backend creation
typedef struct sam_be_rmq_opts_t {
    char *host;           ///< hostname, mostly some ip address
    int  port;            ///< port the broker listens to
    char *user;           ///< username
    char *pass;           ///< password
    int heartbeat;        ///< heartbeat interval in seconds

    int tries;            ///< number of re-connect tries
    uint64_t interval;    ///< interval of re-connect tries
} sam_be_rmq_opts_t;


/// the be_rmq state
typedef struct sam_be_rmq_t {
    char *name;        ///< identifier assigned by the user
    uint64_t id;       ///< identifier used by sam_buf
    zlist_t *store;    ///< maps message keys to sequence numbers


    struct {                                ///< amqp connection
        amqp_connection_state_t connection; ///< internal connection state
        amqp_socket_t *socket;              ///< tcp socket holding the conn
        int message_channel;                ///< channel for messages
        int method_channel;                 ///< channel for rpc calls
        unsigned int seq;                   ///< incremented number for acks
    } amqp;


    struct {
        bool established;        ///< indicator needed for destroy ()
        sam_be_rmq_opts_t opts;  ///< for re-connecting tries
        int tries;
    } connection;


    struct {
        zsock_t *sig;             ///< send signals to the be maintainer
        zsock_t *pub;             ///< accepting publishing requests
        zsock_t *rpc;             ///< accepting rpc requests
        zsock_t *ack;             ///< pushing ack's as a generic backend
        zmq_pollitem_t *amqp;    ///< socket maintaining broker connection
    } sock;

} sam_be_rmq_t;


/// per-message publishing options
typedef struct sam_be_rmq_pub_t {
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
    } props;
    zlist_t *headers;
    zframe_t *payload;
} sam_be_rmq_pub_t;


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
sam_be_rmq_new (
    const char *name,
    uint64_t id);


//  --------------------------------------------------------------------------
/// @brief Destroy a RabbitMQ connection
/// @param self A be_rmq instance
void
sam_be_rmq_destroy (
    sam_be_rmq_t **self);


//  --------------------------------------------------------------------------
/// @brief Connect to a RabbitMQ broker
/// @param opts The connection parameters
/// @return 0 for succes, -1 for error
int
sam_be_rmq_connect (
    sam_be_rmq_t *self,
    sam_be_rmq_opts_t *opts);


//  --------------------------------------------------------------------------
/// @brief Publish a message to the broker
/// @param self A be_rmq instance
/// @param TODO
int
sam_be_rmq_publish (
    sam_be_rmq_t *self,
    sam_be_rmq_pub_t *opts);


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
    char *ack_endpoint);


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
