
#include "common.h"



static void *
try (char const *ctx, amqp_rpc_reply_t x)
{
    
    switch (x.reply_type) {

    case AMQP_RESPONSE_NORMAL:
        return 0;
        break;

    case AMQP_RESPONSE_NONE:
        fprintf (stderr, "%s: missing RPC reply type\n", ctx);
        break;

    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
        fprintf (stderr, "%s: %s\n", ctx, amqp_error_string2(x.library_error));
        break;

    case AMQP_RESPONSE_SERVER_EXCEPTION:
        switch (x.reply.id) {
        case AMQP_CONNECTION_CLOSE_METHOD: {
            amqp_connection_close_t *m =
                (amqp_connection_close_t *) x.reply.decoded;

            fprintf (
                stderr,
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
            fprintf (
                stderr,
                "%s: server channel error %d, message: %.*s\n",
                ctx,
                m->reply_code,
                (int) m->reply_text.len,
                (char *) m->reply_text.bytes);
                
            break;
        }

        default:
            fprintf (
                stderr,
                "%s: unknown server error, method id 0x%08X\n",
                ctx,
                x.reply.id);

            break;
        }

        break;
    }

    assert (0);
}



rabbit_t *
rabbit_new (rabbit_login_t *login_data)
{
    printf (
        "[rabbit] connecting to '%s:%d'\n",
        login_data->host,
        login_data->port);

    rabbit_t *self = malloc (sizeof (rabbit_t));
    assert (self);

    self->chan = login_data->chan;
    self->conn = amqp_new_connection ();
    self->sock = amqp_tcp_socket_new (self->conn);
    assert (self->sock);

    int rc = amqp_socket_open (
        self->sock,
        login_data->host,
        login_data->port);
    
    assert (rc == 0);

    try("logging in", amqp_login(
            self->conn,              // state
            "/",                     // vhost
            0,                       // channel max
            AMQP_DEFAULT_FRAME_SIZE, // frame max
            0,                       // hearbeat
            AMQP_SASL_METHOD_PLAIN,  // sasl method
            login_data->user,
            login_data->pass));

    amqp_channel_open (self->conn, self->chan);
    try ("opening channel", amqp_get_rpc_reply (self->conn));

    return self;
}


void
rabbit_destroy (rabbit_t **self)
{
    try ("closing channel",
         amqp_channel_close (
             (*self)->conn, (*self)->chan, AMQP_REPLY_SUCCESS));

    try ("closing connection",
         amqp_connection_close (
             (*self)->conn, AMQP_REPLY_SUCCESS));

    int rc = amqp_destroy_connection ((*self)->conn);
    assert (rc >= 0);

    free (*self);
    *self = NULL;
}


void
rabbit_declare_exchange (
    rabbit_t *self,
    const char *exchange,
    const char *type)
{
    amqp_exchange_declare (
        self->conn,                    // connection state
        self->chan,                    // virtual connection
        amqp_cstring_bytes (exchange), // exchange name
        amqp_cstring_bytes (type),     // type
        0,                             // passive
        0,                             // durable
        amqp_empty_table);             // arguments

    try ("declare exchange", amqp_get_rpc_reply(self->conn));
}


void
rabbit_declare_and_bind (
    rabbit_t *self,
    const char *queue,
    const char *exchange,
    const char *binding)
{
    amqp_queue_declare_ok_t *r = amqp_queue_declare (
        self->conn,                 // connection state
        self->chan,                 // virtual connection
        amqp_cstring_bytes (queue), // queue name
        0,                          // passive
        0,                          // durable
        0,                          // exclusive
        1,                          // auto-delete
        amqp_empty_table);          // arguments

    try ("declaring queue", amqp_get_rpc_reply (self->conn));

    printf ("[rabbit] declared new queue: %.*s\n",
            (int) r->queue.len,
            (char *) r->queue.bytes);

    amqp_bytes_t queue_name = amqp_bytes_malloc_dup (r->queue);

    if (queue_name.bytes == NULL) {
        fprintf (stderr, "ran out of memory while copying queue name\n");
        assert (0);
    }

    printf ("[rabbit] binding queue to '%s' with key '%s'\n",
            exchange,
            binding);

    amqp_queue_bind (
        self->conn, self->chan, queue_name,
        amqp_cstring_bytes (exchange),
        amqp_cstring_bytes (binding),
        amqp_empty_table);

    try ("binding queue", amqp_get_rpc_reply (self->conn));

    amqp_basic_consume (
        self->conn,        // connection state
        self->chan,        // virtual connection
        queue_name,        // queue name 
        amqp_empty_bytes,  // consumer tag
        0,                 // no local
        1,                 // no ack
        0,                 // exclusive
        amqp_empty_table); // arguments

    try ("consuming", amqp_get_rpc_reply (self->conn));
    amqp_bytes_free (queue_name);
}


void
rabbit_enable_pubconf (rabbit_t *self)
{
    printf ("[rabbit] enabling publisher confirms\n");

    amqp_confirm_select_t req;
    req.nowait = 0;

    amqp_simple_rpc_decoded(
        self->conn,
        self->chan,
        AMQP_CONFIRM_SELECT_METHOD,
        AMQP_CONFIRM_SELECT_OK_METHOD,
        &req);

    try ("enable publisher confirms",
         amqp_get_rpc_reply(self->conn));
}


void
rabbit_publish (
    rabbit_t *self,
    const char *exch,
    const char *rkey,
    char *msg)
{
    amqp_bytes_t msg_bytes;
    msg_bytes.len = sizeof (msg);
    msg_bytes.bytes = msg;

    int rc = amqp_basic_publish (
        self->conn,               // connection state
        self->chan,               // virtual connection
        amqp_cstring_bytes(exch), // exchange name
        amqp_cstring_bytes(rkey), // routing key
        0,                        // mandatory
        0,                        // immediate
        NULL,                     // properties
        msg_bytes);               // body

    assert (rc == 0);
}


int
rabbit_get_sockfd (rabbit_t *self)
{
    return amqp_get_sockfd (self->conn);
}


int
rabbit_handle_ack (rabbit_t *self)
{
    amqp_frame_t frame;

    struct timeval timeout = { .tv_sec = 0, .tv_usec = 0 };
    int rc = amqp_simple_wait_frame_noblock (self->conn, &frame, &timeout);
    int ack_c = 0;

    while (rc != AMQP_STATUS_TIMEOUT) {
        if (frame.frame_type != AMQP_FRAME_METHOD) {
            fprintf (stderr, "[rabbit] handle_ack: got %d instead of METHOD\n",
                     frame.frame_type);
            return -1;
        }

        if (frame.payload.method.id != AMQP_BASIC_ACK_METHOD) {
            fprintf (
                stderr,
                "[rabbit] handle_ack: got %d instead of BASIC_ACK",
                frame.payload.properties.class_id);
            return -1;
        }

        rc = amqp_simple_wait_frame_noblock (self->conn, &frame, &timeout);
        ack_c += 1;
    }

    printf ("[rabbit] consumed %d acks\n", ack_c);
    return ack_c;
}
