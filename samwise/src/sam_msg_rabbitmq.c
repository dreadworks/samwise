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

   This class is an abstraction of the RabbitMQ-C library maintained
   by Alan Antonuk (https://github.com/alanxz/rabbitmq-c). It offers
   the basic functionality to publish messages, send methods and read
   buffered ACKS. An instance of this class wraps a TCP connection to
   the RabbitMQ broker containing one channel. The channel then can be
   put into confirm mode.

   It is also possible to start an internal actor in a separate thread
   by using the start function. This actor then asynchronously waits
   for publishing requests, heartbeats and acks by using the following
   channels:

             RabbitMQ broker
                  ^
                  |
   PUSH           o
   > sam_msg_rabbitmq o REP
     \        |        /
      \     PIPE      /
       \      |      /
    PULL o  caller < REQ

*/

#include "../include/sam_prelude.h"


//  --------------------------------------------------------------------------
/// Checks if the frame returned by the broker is an acknowledgement.
static void
check_amqp_ack_frame (amqp_frame_t frame)
{
    if (frame.frame_type != AMQP_FRAME_METHOD) {

        sam_log_errorf (
            "amqp: got %d instead of METHOD\n",
            frame.frame_type);

        assert (false);
    }

    if (frame.payload.method.id != AMQP_BASIC_ACK_METHOD) {

        sam_log_errorf (
            "amqp: got %d instead of BASIC_ACK",
            frame.payload.properties.class_id);

        assert (false);
    }
}


//  --------------------------------------------------------------------------
/// Callback for POLLIN events on the AMQP TCP socket. While the
/// poll-loop works in a level triggered fashion, the AMQP library
/// greedily reads all content from the socket and buffers the frames
/// internally. Because of this, a loop gets started that reads from
/// this data structure with a 0 timeout (NULL as timeout would turn
/// the call into a blocking read).
static int
handle_amqp (zloop_t *loop UU, zmq_pollitem_t *amqp UU, void *args)
{
    sam_msg_rabbitmq_t *self = args;

    amqp_frame_t frame;
    struct timeval timeout = { .tv_sec = 0, .tv_usec = 0 };

    int rc = amqp_simple_wait_frame_noblock (
        self->amqp.connection, &frame, &timeout);

    while (rc != AMQP_STATUS_TIMEOUT) {
        check_amqp_ack_frame (frame);
        amqp_basic_ack_t *props = frame.payload.method.decoded;
        assert (props);

        sam_log_trace ("received ack");
        zsock_send (
            self->psh, "iii", self->id, SAM_RES_ACK, props->delivery_tag);

        rc = amqp_simple_wait_frame_noblock (
            self->amqp.connection, &frame, &timeout);
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Callback for communication arriving on the REP socket. Most of the
/// time, these are publishing requests. It also handles RPC calls.
static int
handle_req (zloop_t *loop, zsock_t *rep, void *args)
{
    sam_msg_rabbitmq_t *self = args;

    char *action;
    zmsg_t *msg = zmsg_new ();

    zsock_recv (rep, "sm", &action, &msg);
    sam_log_tracef ("handling %s request", action);
    int rc = 0;

    // publish
    if (!strcmp (action, "publish")) {
        sam_log_trace ("publishing message");
        char *exchange = zmsg_popstr (msg);
        char *routing_key = zmsg_popstr (msg);
        zframe_t *payload = zmsg_pop (msg);

        zsock_send (rep, "i", self->amqp.seq);
        rc = sam_msg_rabbitmq_publish (
            self,
            exchange,
            routing_key,
            zframe_data (payload),
            zframe_size (payload));

        free (exchange);
        free (routing_key);
        zframe_destroy (&payload);
    }

    // exchange.declare
    else if (!strcmp (action, "exchange.declare")) {
        sam_log_info ("declare exchange");

        char
            *exchange = zmsg_popstr (msg),
            *type = zmsg_popstr (msg);

        sam_msg_rabbitmq_exchange_declare (self, exchange, type);
        zsock_send (rep, "i", self->id);

        free (exchange);
        free (type);
        rc = handle_amqp (loop, self->amqp_pollitem, self);
    }

    // exchange.delete
    else if (!strcmp (action, "exchange.delete")) {
        sam_log_info ("delete exchange");
        char *exchange = zmsg_popstr (msg);

        sam_msg_rabbitmq_exchange_delete (self, exchange);
        zsock_send (rep, "i", self->id);

        free(exchange);
        rc = handle_amqp (loop, self->amqp_pollitem, self);
    }

    else {
        sam_log_errorf (
            "req: received unknown command %d",
            action);

        rc = -1;
    }

    free (action);
    zmsg_destroy (&msg);
    return rc;
}


//  --------------------------------------------------------------------------
/// Entry point for the actor thread. Starts a loop listening on the
/// PIPE, REP zsock and the AMQP TCP socket.
static void
actor (zsock_t *pipe, void *args)
{
    sam_msg_rabbitmq_t *self = args;
    sam_log_info ("started msg_rabbitmq actor");

    zmq_pollitem_t amqp_pollitem = {
        .socket = NULL,
        .fd = sam_msg_rabbitmq_sockfd (self),
        .events = ZMQ_POLLIN,
        .revents = 0
    };

    zloop_t *loop = zloop_new ();
    self->amqp_pollitem = &amqp_pollitem;

    zloop_reader (loop, pipe, sam_gen_handle_pipe, NULL);
    zloop_reader (loop, self->rep, handle_req, self);
    zloop_poller (loop, &amqp_pollitem, handle_amqp, self);

    sam_log_trace ("send ready signal");
    zsock_signal (pipe, 0);
    zloop_start (loop);

    sam_log_info ("stopping msg_rabbitmq actor");
    zloop_destroy (&loop);
}


//  --------------------------------------------------------------------------
/// This helper function analyses the return value of an AMQP rpc
/// call. If the return code is anything other than
/// AMQP_RESPONSE_NORMAL, an assertion fails.
static void
try (char const *ctx, amqp_rpc_reply_t x)
{

    switch (x.reply_type) {

    case AMQP_RESPONSE_NORMAL:
        return;

    case AMQP_RESPONSE_NONE:
        sam_log_errorf (
            "%s: missing RPC reply type",
            ctx);

        break;

    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
        sam_log_errorf (
            "%s: %s\n",
            ctx,
            amqp_error_string2(x.library_error));

        break;

    case AMQP_RESPONSE_SERVER_EXCEPTION:
        switch (x.reply.id) {
        case AMQP_CONNECTION_CLOSE_METHOD: {
            amqp_connection_close_t *m =
                (amqp_connection_close_t *) x.reply.decoded;

            sam_log_errorf (
                "%s: server connection error %d, message: %.*s",
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
                "%s: server channel error %d, message: %.*s",
                ctx,
                m->reply_code,
                (int) m->reply_text.len,
                (char *) m->reply_text.bytes);
            break;
        }

        default:
            sam_log_errorf (
                "%s: unknown server error, method id 0x%08X",
                ctx,
                x.reply.id);

            break;
        }

        break;
    }

    assert (false);
}


//  --------------------------------------------------------------------------
/// Returns the underlying TCP connections socket file descriptor.
int
sam_msg_rabbitmq_sockfd (sam_msg_rabbitmq_t *self)
{
    return amqp_get_sockfd (self->amqp.connection);
}


//  --------------------------------------------------------------------------
/// Creates a new instance of msg_rabbitmq. This instance wraps an
/// AMQP TCP connection to a RabbitMQ broker. This constructor
/// function creates an AMQP connection state and initializes a TCP
/// socket for the broker connection.
sam_msg_rabbitmq_t *
sam_msg_rabbitmq_new (unsigned int id)
{
    sam_log_infof (
        "creating rabbitmq message backend (%d)", id);

    sam_msg_rabbitmq_t *self = malloc (sizeof (sam_msg_rabbitmq_t));
    assert (self);

    // init amqp
    self->amqp.seq = 1;
    self->amqp.message_channel = 1;
    self->amqp.method_channel = 2;
    self->amqp.connection = amqp_new_connection ();
    self->amqp.socket = amqp_tcp_socket_new (self->amqp.connection);

    self->id = id;
    return self;
}


//  --------------------------------------------------------------------------
/// Destroy an instance of msg_rabbitmq. This destructor function
/// savely closes the TCP connection to the broker and free's all
/// allocated memory.
void
sam_msg_rabbitmq_destroy (sam_msg_rabbitmq_t **self)
{
    sam_log_trace ("destroying rabbitmq message backend instance");

    try ("closing message channel", amqp_channel_close (
             (*self)->amqp.connection,
             (*self)->amqp.message_channel,
             AMQP_REPLY_SUCCESS));

    try ("closing method channel", amqp_channel_close (
             (*self)->amqp.connection,
             (*self)->amqp.method_channel,
             AMQP_REPLY_SUCCESS));

    try ("closing connection", amqp_connection_close (
             (*self)->amqp.connection,
             AMQP_REPLY_SUCCESS));

    int rc = amqp_destroy_connection ((*self)->amqp.connection);
    assert (rc >= 0);

    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
/// Establish a connection to the RabbitMQ broker. This function opens
/// the connection on the TCP socket. It then opens a channel for
/// communication, which it sets into confirm mode.
void
sam_msg_rabbitmq_connect (
    sam_msg_rabbitmq_t *self,
    sam_msg_rabbitmq_opts_t *opts)
{
    sam_log_infof (
        "connecting to %s:%d", opts->host, opts->port);

    int rc = amqp_socket_open (
        self->amqp.socket,
        opts->host,
        opts->port);

    assert (rc == 0);

    // login
    sam_log_tracef ("logging in as user '%s'", opts->user);
    try ("logging in", amqp_login(
            self->amqp.connection,   // state
            "/",                     // vhost
            0,                       // channel max
            AMQP_DEFAULT_FRAME_SIZE, // frame max
            opts->heartbeat,         // hearbeat
            AMQP_SASL_METHOD_PLAIN,  // sasl method
            opts->user,
            opts->pass));

    // message channel
    sam_log_tracef (
        "opening message channel (%d)",
        self->amqp.message_channel);

    amqp_channel_open (
        self->amqp.connection,
        self->amqp.message_channel);

    try ("opening message channel",
         amqp_get_rpc_reply (self->amqp.connection));

    // method channel
    sam_log_tracef (
        "opening method channel (%d)",
        self->amqp.method_channel);

    amqp_channel_open (
        self->amqp.connection,
        self->amqp.method_channel);

    try ("opening method channel",
         amqp_get_rpc_reply (self->amqp.connection));

    // enable publisher confirms
    sam_log_trace ("set channel in confirm mode");
    amqp_confirm_select_t req;
    req.nowait = 0;

    amqp_simple_rpc_decoded(
        self->amqp.connection,         // state
        self->amqp.message_channel,    // channel
        AMQP_CONFIRM_SELECT_METHOD,    // request id
        AMQP_CONFIRM_SELECT_OK_METHOD, // reply id
        &req);                         // request options

    try ("enable publisher confirms",
         amqp_get_rpc_reply(self->amqp.connection));

}


//  --------------------------------------------------------------------------
/// Publish (basic_publish) a message to the RabbitMQ broker.
int
sam_msg_rabbitmq_publish (
    sam_msg_rabbitmq_t *self,
    const char *exchange,
    const char *routing_key,
    byte *payload,
    int payload_len)
{
    amqp_bytes_t msg_bytes = {
        .len = payload_len,
        .bytes = payload
    };

    sam_log_tracef (
        "publishing message %d of size %d",
        self->amqp.seq,
        payload_len);

    int rc = amqp_basic_publish (
        self->amqp.connection,                // connection state
        self->amqp.message_channel,           // virtual connection
        amqp_cstring_bytes(exchange),         // exchange name
        amqp_cstring_bytes(routing_key),      // routing key
        0,                                    // mandatory
        0,                                    // immediate
        NULL,                                 // properties
        msg_bytes);                           // body

    if (rc == AMQP_STATUS_HEARTBEAT_TIMEOUT) {
        sam_log_error ("connection lost while publishing!");
        zsock_send (self->psh, "i", SAM_RES_CONNECTION_LOSS);
        return -1;
    }

    assert (rc == 0);
    self->amqp.seq += 1;
    return rc;
}


//  --------------------------------------------------------------------------
/// Try to receive one or more buffered ACK's from the channel. This
/// function currently just serves to "eat" frames and might be
/// removed.
void
sam_msg_rabbitmq_handle_ack (sam_msg_rabbitmq_t *self)
{
    amqp_frame_t frame;
    struct timeval timeout = { .tv_sec = 0, .tv_usec = 0 };

    int rc = amqp_simple_wait_frame_noblock (
        self->amqp.connection, &frame, &timeout);

    int ack_c = 0;

    while (rc != AMQP_STATUS_TIMEOUT) {
        check_amqp_ack_frame (frame);

        rc = amqp_simple_wait_frame_noblock (
            self->amqp.connection, &frame, &timeout);

        ack_c += 1;
    }

    sam_log_tracef ("handled %d acks", ack_c);
}


//  --------------------------------------------------------------------------
/// Declare an exchange.
void
sam_msg_rabbitmq_exchange_declare (
    sam_msg_rabbitmq_t *self,
    const char *exchange,
    const char *type)
{
    sam_log_infof (
        "declaring exchange '%s' (%s)", exchange, type);

    amqp_exchange_declare (
        self->amqp.connection,         // connection state
        self->amqp.method_channel,     // virtual connection
        amqp_cstring_bytes (exchange), // exchange name
        amqp_cstring_bytes (type),     // type
        0,                             // passive
        0,                             // durable
        amqp_empty_table);             // arguments

    try ("declare exchange", amqp_get_rpc_reply(self->amqp.connection));
}


//  --------------------------------------------------------------------------
/// Delete an exchange.
void
sam_msg_rabbitmq_exchange_delete (
    sam_msg_rabbitmq_t *self,
    const char *exchange UU)
{
    amqp_exchange_delete (
        self->amqp.connection,
        self->amqp.method_channel,
        amqp_cstring_bytes (exchange),
        0);

    try ("delete exchange", amqp_get_rpc_reply(self->amqp.connection));
}


//  --------------------------------------------------------------------------
/// Start an actor handling requests asynchronously. Internally, it
/// starts an zactor with a zloop listening to data on either the
/// actors pipe, reply socket or the AMQP TCP socket. The provided
/// msg_rabbitmq instance must already have openend a connection and
/// be logged in to the broker.
sam_backend_t *
sam_msg_rabbitmq_start (
    sam_msg_rabbitmq_t **self,
    char *pll_endpoint)
{
    sam_log_trace ("starting message backend actor");

    sam_backend_t *backend = malloc (sizeof (sam_backend_t));
    zuuid_t *rep_endpoint_id = zuuid_new ();
    char rep_endpoint [64];

    snprintf (rep_endpoint, 64, "inproc://%s", zuuid_str (rep_endpoint_id));
    zuuid_destroy (&rep_endpoint_id);

    sam_log_trace ("generated internal req/rep endpoint");

    (*self)->rep = zsock_new_rep (rep_endpoint);
    assert ((*self)->rep);

    sam_log_tracef ("connected push socket to '%s'", pll_endpoint);
    (*self)->psh = zsock_new_push (pll_endpoint);
    assert ((*self)->psh);

    // change ownership
    backend->self = *self;
    backend->req = zsock_new_req (rep_endpoint);
    backend->actor = zactor_new (actor, *self);

    *self = NULL;
    return backend;
}


//  --------------------------------------------------------------------------
/// Stop the message backend thread. When calling this function, all
/// sockets for communicating with the thread are closed and all
/// memory allocated by sam_msg_rabbitmq_start is free'd. By calling
/// this function, ownership of the msg_rabbitmq instance is
/// reclaimed.
sam_msg_rabbitmq_t *
sam_msg_rabbitmq_stop (
    sam_backend_t **backend)
{
    sam_msg_rabbitmq_t *self = (*backend)->self;

    zsock_destroy (&(*backend)->req);
    zactor_destroy (&(*backend)->actor);

    free (*backend);
    *backend = NULL;

    zsock_destroy (&self->rep);
    zsock_destroy (&self->psh);

    return self;
}


//  --------------------------------------------------------------------------
/// Self test this class. Tests the synchronous amqp wrapper and the
/// asynchronous handling as a generic message backend.
void
sam_msg_rabbitmq_test ()
{
    printf ("\n** RABBITMQ BACKEND **\n");

    int id = 0;
    sam_msg_rabbitmq_t *rabbit = sam_msg_rabbitmq_new (id);
    assert (rabbit);

    sam_msg_rabbitmq_opts_t opts = {
        .host = "localhost",
        .port = 5672,
        .user = "guest",
        .pass = "guest",
        .heartbeat = 1
    };

    sam_msg_rabbitmq_connect (rabbit, &opts);
    sam_msg_rabbitmq_exchange_declare (rabbit, "x-test", "direct");
    sam_msg_rabbitmq_exchange_delete (rabbit, "x-test");

    //
    //   SYNCHRONOUS COMMUNICATION
    //
    int rc = sam_msg_rabbitmq_publish (
        rabbit, "amq.direct", "", (byte *) "hi!", 3);

    assert (rc == 0);

    zmq_pollitem_t items = {
        .socket = NULL,
        .fd = sam_msg_rabbitmq_sockfd (rabbit),
        .events = ZMQ_POLLIN,
        .revents = 0
    };

    // wait for ack to arrive
    sam_log_trace ("waiting for ACK");
    rc = zmq_poll (&items, 1, 500);
    assert (rc == 1);

    // TODO: this must either be removed
    // or replaced by something more useful
    sam_msg_rabbitmq_handle_ack (rabbit);


    //
    //   ASYNCHRONOUS COMMUNICATION
    //
    char *pll_endpoint = "inproc://test-pll";
    zsock_t *pll = zsock_new_pull (pll_endpoint);
    assert (pll);

    sam_backend_t *backend =
        sam_msg_rabbitmq_start (&rabbit, pll_endpoint);

    assert (backend);
    assert (!rabbit);

    // send method request
    zsock_send (
        backend->req, "sss",
        "exchange.declare",  // action
        "x-test-async",      // exchange name
        "direct");           // type

    int backend_id;
    zsock_recv (backend->req, "i", &backend_id);
    assert (backend_id == id);

    zsock_send (
        backend->req, "ss",
        "exchange.delete",  // action
        "x-test-async");    // exchange name

    backend_id = -1;
    zsock_recv (backend->req, "i", &backend_id);
    assert (backend_id == id);

    zsock_send (
        backend->req, "ssss",
        "publish",     // action
        "amq.direct",  // exchange
        "",            // routing key
        "hi!");        // payload

    // wait for sequence number
    int seq;
    zsock_recv (backend->req, "i", &seq);
    assert (seq == 2);

    // wait for ack
    int ack_seq;
    sam_res_t res_t;

    backend_id = -1;
    zsock_recv (pll, "iii", &backend_id, &res_t, &ack_seq);
    assert (backend_id == id);
    assert (res_t == SAM_RES_ACK);
    assert (ack_seq == seq);

    // reclaim ownership
    rabbit = sam_msg_rabbitmq_stop (&backend);
    assert (rabbit);

    // mr gorbachev tear down this wall
    zsock_destroy (&pll);
    sam_msg_rabbitmq_destroy (&rabbit);
}
