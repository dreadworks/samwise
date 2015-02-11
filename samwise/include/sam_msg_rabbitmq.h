/*  =========================================================================

    sam_msg_rabbit - message backend for RabbitMQ

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT


    =========================================================================
*/
/**

   @brief message backend for RabbitMQ

   TODO

*/


typedef struct sam_msg_rabbitmq_t {

    sam_logger_t *logger;

    struct {
        bool connected;
        amqp_connection_state_t conn;
        amqp_socket_t *sock;
        int chan;
    } amqp;

} sam_msg_rabbitmq_t;


typedef struct sam_msg_rabbitmq_message_t {
    const char *exchange;
    const char *routing_key;
    const byte *payload;
    const int payload_len;
} sam_msg_rabbitmq_message_t;


typedef struct sam_msg_rabbitmq_opts_t {

    char *host;
    int  port;
    char *user;
    char *pass;

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
/// @brief Returns a boolean indicating the connection status
/// @param self A msg_rabbitmq instance
/// @return True if connected, otherwise false
bool
sam_msg_rabbitmq_connected (
    sam_msg_rabbitmq_t *self);


//  --------------------------------------------------------------------------
/// @brief Publish a message to the broker
/// @param self A msg_rabbitmq instance
/// @param msg  The message as structured data
void
sam_msg_rabbitmq_publish (
    sam_msg_rabbitmq_t *self,
    sam_msg_rabbitmq_message_t *msg);


//  --------------------------------------------------------------------------
/// @brief Read all buffered confirm.select acknowledgements
/// @param self A msg_rabbitmq instance
void
sam_msg_rabbitmq_handle_ack (
    sam_msg_rabbitmq_t *self);


//  --------------------------------------------------------------------------
/// @brief Self test this class
void
sam_msg_rabbitmq_test ();

