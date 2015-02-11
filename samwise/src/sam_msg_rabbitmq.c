/*  =========================================================================

    sam_msg_rabbit - message backend for the RabbitMQ AMQ broker

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief message backend for RabbitMQ
   @file sam_msg_rabbit.c

   TODO description

*/

#include "../include/sam_prelude.h"


//  --------------------------------------------------------------------------
/// TODO
static void
try (sam_msg_rabbitmq_t *self, char const *ctx, amqp_rpc_reply_t x)
{

    switch (x.reply_type) {

    case AMQP_RESPONSE_NORMAL:
        return;

    case AMQP_RESPONSE_NONE:
        sam_log_errorf (self->logger, "%s: missing RPC reply type\n", ctx);
        break;

    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
        sam_log_errorf (self->logger, "%s: %s\n", ctx, amqp_error_string2(x.library_error));
        break;

    case AMQP_RESPONSE_SERVER_EXCEPTION:
        switch (x.reply.id) {
        case AMQP_CONNECTION_CLOSE_METHOD: {
            amqp_connection_close_t *m =
                (amqp_connection_close_t *) x.reply.decoded;

            sam_log_errorf (
                self->logger,
                "%s: server connection error %d, message: %.*s\n",
                ctx,
                m->reply_code,
                (int) m->reply_text.len,
                (char *) m->reply_text.bytes);

            break;
        }

        case AMQP_CHANNEL_CLOSE_METHOD: {
            amqp_channel_close_t *m =
                (amqp_channel_close_t *) x.reply.decoded;

            sam_log_errorf (
                self->logger,
                "%s: server channel error %d, message: %.*s\n",
                ctx,
                m->reply_code,
                (int) m->reply_text.len,
                (char *) m->reply_text.bytes);
            break;
        }

        default:
            sam_log_errorf (
                self->logger,
                "%s: unknown server error, method id 0x%08X\n",
                ctx,
                x.reply.id);

            break;
        }

        break;
    }

    assert (false);
}


//  --------------------------------------------------------------------------
/// TODO
int
sam_msg_rabbitmq_sockfd (sam_msg_rabbitmq_t *self)
{
    return amqp_get_sockfd (self->amqp.conn);
}


//  --------------------------------------------------------------------------
/// TODO
sam_msg_rabbitmq_t *
sam_msg_rabbitmq_new ()
{
    sam_logger_t *logger = sam_logger_new ("msg_rabbitmq", SAM_LOG_ENDPOINT);
    sam_log_info (logger, "creating rabbitmq message backend instance");

    sam_msg_rabbitmq_t *self = malloc (sizeof (sam_msg_rabbitmq_t));
    assert (self);

    self->logger = logger;

    // init amqp
    self->amqp.chan = 1;
    self->amqp.conn = amqp_new_connection ();
    self->amqp.sock = amqp_tcp_socket_new (self->amqp.conn);
    self->amqp.connected = false;

    return self;
}


//  --------------------------------------------------------------------------
/// TODO
void
sam_msg_rabbitmq_destroy (sam_msg_rabbitmq_t **self)
{
    assert (*self);
    sam_log_info (
        (*self)->logger, "destroying rabbitmq message backend instance");

    sam_logger_destroy (&(*self)->logger);

    // close amqp connection
    try (*self, "closing channel", amqp_channel_close (
             (*self)->amqp.conn,
             (*self)->amqp.chan,
             AMQP_REPLY_SUCCESS));

    try (*self, "closing connection", amqp_connection_close (
             (*self)->amqp.conn,
             AMQP_REPLY_SUCCESS));

    int rc = amqp_destroy_connection ((*self)->amqp.conn);
    assert (rc >= 0);

    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
/// TODO
void
sam_msg_rabbitmq_connect (
    sam_msg_rabbitmq_t *self,
    sam_msg_rabbitmq_opts_t *opts)
{
    sam_log_infof (
        self->logger, "connecting to %s:%d", opts->host, opts->port);

    int rc = amqp_socket_open (
        self->amqp.sock,
        opts->host,
        opts->port);

    assert (rc == 0);

    // login
    sam_log_tracef (self->logger, "logging in as user '%s'", opts->user);
    try(self, "logging in", amqp_login(
            self->amqp.conn,         // state
            "/",                     // vhost
            0,                       // channel max
            AMQP_DEFAULT_FRAME_SIZE, // frame max
            0,                       // hearbeat
            AMQP_SASL_METHOD_PLAIN,  // sasl method
            opts->user,
            opts->pass));

    // opening channel
    sam_log_tracef (self->logger, "opening channel %d", self->amqp.chan);
    amqp_channel_open (self->amqp.conn, self->amqp.chan);
    try (self, "opening channel", amqp_get_rpc_reply (self->amqp.conn));
    self->amqp.connected = true;

    // enable publisher confirms
    sam_log_trace (self->logger, "set channel in confirm mode");
    amqp_confirm_select_t req;
    req.nowait = 0;

    amqp_simple_rpc_decoded(
        self->amqp.conn,               // state
        self->amqp.chan,               // channel
        AMQP_CONFIRM_SELECT_METHOD,    // request id
        AMQP_CONFIRM_SELECT_OK_METHOD, // reply id
        &req);                         // request options

    try (
        self,
        "enable publisher confirms",
        amqp_get_rpc_reply(self->amqp.conn));

}


//  --------------------------------------------------------------------------
/// TODO
bool
sam_msg_rabbitmq_connected (sam_msg_rabbitmq_t *self)
{
    assert (self);
    return self->amqp.connected;
}


//  --------------------------------------------------------------------------
/// TODO
void
sam_msg_rabbitmq_publish (
    sam_msg_rabbitmq_t *self,
    sam_msg_rabbitmq_message_t *msg)
{
    amqp_bytes_t msg_bytes = {
        .len = msg->payload_len,
        .bytes = (byte *) msg->payload
    };

    sam_log_tracef (
        self->logger, "publishing message of size %d", msg->payload_len);

    int rc = amqp_basic_publish (
        self->amqp.conn,                      // connection state
        self->amqp.chan,                      // virtual connection
        amqp_cstring_bytes(msg->exchange),    // exchange name
        amqp_cstring_bytes(msg->routing_key), // routing key
        0,                                    // mandatory
        0,                                    // immediate
        NULL,                                 // properties
        msg_bytes);                           // body

    assert (rc == 0);

}


//  --------------------------------------------------------------------------
/// TODO
void
sam_msg_rabbitmq_handle_ack (sam_msg_rabbitmq_t *self)
{
    amqp_frame_t frame;

    struct timeval timeout = { .tv_sec = 0, .tv_usec = 0 };

    int rc = amqp_simple_wait_frame_noblock (
        self->amqp.conn, &frame, &timeout);

    int ack_c = 0;

    while (rc != AMQP_STATUS_TIMEOUT) {
        if (frame.frame_type != AMQP_FRAME_METHOD) {

            sam_log_errorf (
                self->logger,
                "[rabbit] handle_ack: got %d instead of METHOD\n",
                frame.frame_type);

            assert (false);
        }

        if (frame.payload.method.id != AMQP_BASIC_ACK_METHOD) {

            sam_log_errorf (
                self->logger,
                "[rabbit] handle_ack: got %d instead of BASIC_ACK",
                frame.payload.properties.class_id);

            assert (false);
        }

        rc = amqp_simple_wait_frame_noblock (
            self->amqp.conn, &frame, &timeout);

        ack_c += 1;
    }

    sam_log_tracef (self->logger, "handled %d acks", ack_c);
}


//  --------------------------------------------------------------------------
/// TODO
void
sam_msg_rabbitmq_test ()
{
    printf ("\n** SAM_MSG_RABBIT **\n");

    sam_msg_rabbitmq_t *rabbit = sam_msg_rabbitmq_new ();
    assert (rabbit);

    bool connected = sam_msg_rabbitmq_connected (rabbit);
    assert (!connected);

    sam_msg_rabbitmq_opts_t opts = {
        .host = "localhost",
        .port = 5672,
        .user = "guest",
        .pass = "guest"
    };

    sam_msg_rabbitmq_connect (rabbit, &opts);
    connected = sam_msg_rabbitmq_connected (rabbit);
    assert (connected);

    sam_msg_rabbitmq_message_t msg = {
        .exchange =  "amq.direct",
        .routing_key = "test",
        .payload = (byte *) "test",
        .payload_len = 5
    };

    sam_msg_rabbitmq_publish (rabbit, &msg);

    zmq_pollitem_t items = {
        .socket = NULL,
        .fd = sam_msg_rabbitmq_sockfd (rabbit),
        .events = ZMQ_POLLIN,
        .revents = 0
    };

    // wait for ack to arrive
    sam_log_trace (rabbit->logger, "waiting for ACK");
    int rc = zmq_poll (&items, 1, 500);
    assert (rc == 1);

    sam_msg_rabbitmq_handle_ack (rabbit);
    sam_msg_rabbitmq_destroy (&rabbit);
}
