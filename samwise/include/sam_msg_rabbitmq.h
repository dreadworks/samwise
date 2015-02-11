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
/// @brief
/// @return
int
sam_msg_rabbitmq_sockfd (
    sam_msg_rabbitmq_t *self);


//  --------------------------------------------------------------------------
/// @brief
/// @return
sam_msg_rabbitmq_t *
sam_msg_rabbitmq_new ();


//  --------------------------------------------------------------------------
/// @brief
/// @return
void
sam_msg_rabbitmq_destroy (
    sam_msg_rabbitmq_t **self);


//  --------------------------------------------------------------------------
/// @brief
/// @return
void
sam_msg_rabbitmq_connect (
    sam_msg_rabbitmq_t *self,
    sam_msg_rabbitmq_opts_t *opts);


//  --------------------------------------------------------------------------
/// @brief
/// @return
bool
sam_msg_rabbitmq_connected (
    sam_msg_rabbitmq_t *self);


//  --------------------------------------------------------------------------
/// @brief
/// @return
void
sam_msg_rabbitmq_publish (
    sam_msg_rabbitmq_t *self,
    sam_msg_rabbitmq_message_t *msg);


//  --------------------------------------------------------------------------
/// @brief
void
sam_msg_rabbitmq_handle_ack (
    sam_msg_rabbitmq_t *self);


//  --------------------------------------------------------------------------
/// @brief
void
sam_msg_rabbitmq_test ();

