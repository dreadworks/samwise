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
   PULL    PUSH         o
    o------> sam_msg_rabbitmq o REP
                   |         /
                 PIPE       /
                   |       /
                  caller < REQ

*/

#include "../include/sam_prelude.h"


//  --------------------------------------------------------------------------
/// Checks if the frame returned by the broker is an acknowledgement.
static void
check_amqp_ack_frame (sam_msg_rabbitmq_t *self, amqp_frame_t frame)
{
    if (frame.frame_type != AMQP_FRAME_METHOD) {

        sam_log_errorf (
            self->logger,
            "amqp: got %d instead of METHOD\n",
            frame.frame_type);

        assert (false);
    }

    if (frame.payload.method.id != AMQP_BASIC_ACK_METHOD) {

        sam_log_errorf (
            self->logger,
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

    if (frame.frame_type == AMQP_FRAME_HEARTBEAT) {
        sam_log_trace (self->logger, "registered heartbeat (outer)");
    }

    while (rc != AMQP_STATUS_TIMEOUT) {
        check_amqp_ack_frame (self, frame);
        amqp_basic_ack_t *props = frame.payload.method.decoded;
        assert (props);

        sam_log_trace (self->logger, "received ack");
        zsock_send (self->psh, "ii", SAM_MSG_RES_ACK, props->delivery_tag);

        rc = amqp_simple_wait_frame_noblock (
            self->amqp.connection, &frame, &timeout);

        if (frame.frame_type == AMQP_FRAME_HEARTBEAT) {
            sam_log_trace (self->logger, "registered heartbeat (inner)");
        }
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Callback for communication arriving on the REP socket. Most of the
/// time, these are publishing requests. It also handles RPC calls.
static int
handle_req (zloop_t *loop UU, zsock_t *rep, void *args)
{
    sam_msg_rabbitmq_t *self = args;

    sam_msg_req_t req_t;
    zmsg_t *msg = zmsg_new ();

    zsock_recv (rep, "im", &req_t, &msg);

    if (req_t != SAM_MSG_REQ_PUBLISH) {
        sam_log_errorf (
            self->logger,
            "req: received unknown command %d",
            req_t);

        return -1;
    }

    sam_log_trace (self->logger, "received publishing request");
    zframe_t *msg_data = zmsg_pop (msg);
    assert (msg_data);

    sam_msg_rabbitmq_message_t *amqp_msg =
        *((void **) zframe_data (msg_data));
    assert (amqp_msg);

    zsock_send (rep, "i", self->amqp.seq);
    int rc = sam_msg_rabbitmq_publish (self, amqp_msg);

    zframe_destroy (&msg_data);
    zmsg_destroy (&msg);

    return rc;
}


//  --------------------------------------------------------------------------
/// Callback for requests on the actor's PIPE. Only handles interrupts
/// and termination commands.
static int
handle_pipe (zloop_t *loop UU, zsock_t *pipe, void *args)
{
    sam_msg_rabbitmq_t *self = args;
    zmsg_t *msg = zmsg_recv (pipe);

    if (!msg) {
        sam_log_info (self->logger, "pipe: interrupted!");
        return -1;
    }

    bool term = false;
    char *cmd = zmsg_popstr (msg);
    if (!strcmp (cmd, "$TERM")) {
        sam_log_info (self->logger, "pipe: terminated");
        term = true;
    }

    free (cmd);
    zmsg_destroy (&msg);

    if (term) {
        return -1;
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Entry point for the actor thread. Starts a loop listening on the
/// PIPE, REP zsock and the AMQP TCP socket.
static void
actor (zsock_t *pipe, void *args)
{
    sam_msg_rabbitmq_t *self = args;
    sam_log_info (self->logger, "started msg_rabbitmq actor");

    zmq_pollitem_t amqp_pollitem = {
        .socket = NULL,
        .fd = sam_msg_rabbitmq_sockfd (self),
        .events = ZMQ_POLLIN,
        .revents = 0
    };

    zloop_t *loop = zloop_new ();

    zloop_reader (loop, pipe, handle_pipe, self);
    zloop_reader (loop, self->rep, handle_req, self);
    zloop_poller (loop, &amqp_pollitem, handle_amqp, self);

    sam_log_trace (self->logger, "send ready signal");
    zsock_signal (pipe, 0);
    zloop_start (loop);

    sam_log_info (self->logger, "stopping msg_rabbitmq actor");
    zloop_destroy (&loop);
}


//  --------------------------------------------------------------------------
/// This helper function analyses the return value of an AMQP rpc
/// call. If the return code is anything other than
/// AMQP_RESPONSE_NORMAL, an assertion fails.
static void
try (sam_msg_rabbitmq_t *self, char const *ctx, amqp_rpc_reply_t x)
{

    switch (x.reply_type) {

    case AMQP_RESPONSE_NORMAL:
        return;

    case AMQP_RESPONSE_NONE:
        sam_log_errorf (
            self->logger,
            "%s: missing RPC reply type",
            ctx);

        break;

    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
        sam_log_errorf (
            self->logger,
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
                self->logger,
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
                self->logger,
                "%s: server channel error %d, message: %.*s",
                ctx,
                m->reply_code,
                (int) m->reply_text.len,
                (char *) m->reply_text.bytes);
            break;
        }

        default:
            sam_log_errorf (
                self->logger,
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
sam_msg_rabbitmq_new ()
{
    sam_logger_t *logger = sam_logger_new ("msg_rabbitmq", SAM_LOG_ENDPOINT);
    sam_log_info (logger, "creating rabbitmq message backend instance");

    sam_msg_rabbitmq_t *self = malloc (sizeof (sam_msg_rabbitmq_t));
    assert (self);

    self->logger = logger;

    // init amqp
    self->amqp.seq = 1;
    self->amqp.message_channel = 1;
    self->amqp.method_channel = 2;
    self->amqp.connection = amqp_new_connection ();
    self->amqp.socket = amqp_tcp_socket_new (self->amqp.connection);

    return self;
}


//  --------------------------------------------------------------------------
/// Destroy an instance of msg_rabbitmq. This destructor function
/// savely closes the TCP connection to the broker and free's all
/// allocated memory.
void
sam_msg_rabbitmq_destroy (sam_msg_rabbitmq_t **self)
{
    sam_log_trace (
        (*self)->logger, "destroying rabbitmq message backend instance");

    sam_logger_destroy (&(*self)->logger);

    try (*self, "closing message channel", amqp_channel_close (
             (*self)->amqp.connection,
             (*self)->amqp.message_channel,
             AMQP_REPLY_SUCCESS));

    try (*self, "closing method channel", amqp_channel_close (
             (*self)->amqp.connection,
             (*self)->amqp.method_channel,
             AMQP_REPLY_SUCCESS));

    try (*self, "closing connection", amqp_connection_close (
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
        self->logger, "connecting to %s:%d", opts->host, opts->port);

    int rc = amqp_socket_open (
        self->amqp.socket,
        opts->host,
        opts->port);

    assert (rc == 0);

    // login
    sam_log_tracef (self->logger, "logging in as user '%s'", opts->user);
    try(self, "logging in", amqp_login(
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
        self->logger,
        "opening message channel (%d)",
        self->amqp.message_channel);

    amqp_channel_open (
        self->amqp.connection,
        self->amqp.message_channel);

    try (self,
         "opening message channel",
         amqp_get_rpc_reply (self->amqp.connection));

    // method channel
    sam_log_tracef (
        self->logger,
        "opening method channel (%d)",
        self->amqp.method_channel);

    amqp_channel_open (
        self->amqp.connection,
        self->amqp.method_channel);

    try (self,
         "opening method channel",
         amqp_get_rpc_reply (self->amqp.connection));

    // enable publisher confirms
    sam_log_trace (self->logger, "set channel in confirm mode");
    amqp_confirm_select_t req;
    req.nowait = 0;

    amqp_simple_rpc_decoded(
        self->amqp.connection,         // state
        self->amqp.message_channel,    // channel
        AMQP_CONFIRM_SELECT_METHOD,    // request id
        AMQP_CONFIRM_SELECT_OK_METHOD, // reply id
        &req);                         // request options

    try (
        self,
        "enable publisher confirms",
        amqp_get_rpc_reply(self->amqp.connection));

}


//  --------------------------------------------------------------------------
/// Publish (basic_publish) a message to the RabbitMQ broker.
int
sam_msg_rabbitmq_publish (
    sam_msg_rabbitmq_t *self,
    sam_msg_rabbitmq_message_t *msg)
{
    amqp_bytes_t msg_bytes = {
        .len = msg->payload_len,
        .bytes = (byte *) msg->payload
    };

    sam_log_tracef (
        self->logger,
        "publishing message %d of size %d",
        self->amqp.seq,
        msg->payload_len);

    int rc = amqp_basic_publish (
        self->amqp.connection,                // connection state
        self->amqp.message_channel,           // virtual connection
        amqp_cstring_bytes(msg->exchange),    // exchange name
        amqp_cstring_bytes(msg->routing_key), // routing key
        0,                                    // mandatory
        0,                                    // immediate
        NULL,                                 // properties
        msg_bytes);                           // body

    if (rc == AMQP_STATUS_HEARTBEAT_TIMEOUT) {
        sam_log_error (
            self->logger,
            "connection lost while publishing!");
        zsock_send (self->psh, "i", SAM_MSG_RES_CONNECTION_LOSS);
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
        check_amqp_ack_frame (self, frame);

        rc = amqp_simple_wait_frame_noblock (
            self->amqp.connection, &frame, &timeout);

        ack_c += 1;
    }

    sam_log_tracef (self->logger, "handled %d acks", ack_c);
}


//  --------------------------------------------------------------------------
/// Start an actor handling requests asynchronously. Internally, it
/// starts an zactor with a zloop listening to data on either the
/// actors pipe, reply socket or the AMQP TCP socket. The provided
/// msg_rabbitmq instance must already have openend a connection and
/// be logged in to the broker.
sam_msg_backend_t *
sam_msg_rabbitmq_start (
    sam_msg_rabbitmq_t **self,
    char *pll_endpoint)
{
    sam_log_trace ((*self)->logger, "starting message backend actor");

    sam_msg_backend_t *backend = malloc (sizeof (sam_msg_backend_t));
    zuuid_t *rep_endpoint_id = zuuid_new ();
    char rep_endpoint [64];

    snprintf (rep_endpoint, 64, "inproc://%s", zuuid_str (rep_endpoint_id));
    zuuid_destroy (&rep_endpoint_id);

    sam_log_tracef (
        (*self)->logger, "generated req/rep endpoint: %s", rep_endpoint);

    (*self)->rep = zsock_new_rep (rep_endpoint);
    assert ((*self)->rep);

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
    sam_msg_backend_t **backend)
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
    printf ("\n** SAM_MSG_RABBIT **\n");

    sam_msg_rabbitmq_t *rabbit = sam_msg_rabbitmq_new ();
    assert (rabbit);

    sam_msg_rabbitmq_opts_t opts = {
        .host = "localhost",
        .port = 5672,
        .user = "guest",
        .pass = "guest",
        .heartbeat = 1
    };

    sam_msg_rabbitmq_connect (rabbit, &opts);

    //
    //   SYNCHRONOUS COMMUNICATION
    //

    sam_msg_rabbitmq_message_t msg = {
        .exchange =  "amq.direct",
        .routing_key = "test",
        .payload = (byte *) "test",
        .payload_len = 5
    };

    int rc = sam_msg_rabbitmq_publish (rabbit, &msg);
    assert (rc == 0);

    zmq_pollitem_t items = {
        .socket = NULL,
        .fd = sam_msg_rabbitmq_sockfd (rabbit),
        .events = ZMQ_POLLIN,
        .revents = 0
    };

    // wait for ack to arrive
    sam_log_trace (rabbit->logger, "waiting for ACK");
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

    sam_msg_backend_t *backend =
        sam_msg_rabbitmq_start (&rabbit, pll_endpoint);

    assert (backend);
    assert (!rabbit);

    // send publishing request
    sam_msg_rabbitmq_message_t amqp_message = {
        .exchange = "amq.direct",
        .routing_key = "",
        .payload = (byte *) "hi!",
        .payload_len = 4
    };

    zsock_send (
        backend->req, "ip",
        SAM_MSG_REQ_PUBLISH,
        &amqp_message);

    // wait for sequence number
    int seq;
    zsock_recv (backend->req, "i", &seq);
    assert (seq == 2);

    // wait for ack
    int ack_seq;
    sam_msg_res_t res_t;
    zsock_recv (pll, "ii", &res_t, &ack_seq);
    assert (res_t == SAM_MSG_RES_ACK);
    assert (ack_seq == seq);

    // reclaim ownership
    rabbit = sam_msg_rabbitmq_stop (&backend);
    assert (rabbit);

    // mr gorbachev tear down this wall
    zsock_destroy (&pll);
    sam_msg_rabbitmq_destroy (&rabbit);
}
