/*  =========================================================================

    sam_be_rmq - message backend for the RabbitMQ AMQP broker

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief message backend for RabbitMQ
   @file sam_be_rmq.c

   This class is an abstraction of the RabbitMQ-C library maintained
   by Alan Antonuk (https://github.com/alanxz/rabbitmq-c). It offers
   the basic functionality to publish messages, send methods and read
   buffered ACKS. An instance of this class wraps a TCP connection to
   the RabbitMQ broker containing one channel. The channel then can be
   put into confirm mode.

   It is also possible to start an internal actor in a separate thread
   by using the start function. It enables samwise to use this as a
   generic backend. This actor then asynchronously waits for
   publishing requests, heartbeats and acks by using the following
   channels:

         RabbitMQ broker
              ^
              |
   PUSH       o       REP
   >     sam_be_rmq     o
     \        |        /
      \     PIPE      /
       \      |      /
    PULL o  caller < REQ

*/

#include "../include/sam_prelude.h"


/// store item to be able to map the message key -> sequence number
typedef struct store_item {
    unsigned int seq;     ///< amqp sequence number for publisher confirms
    int key;              ///< message key assigned outside of this module
} store_item;


//  --------------------------------------------------------------------------
/// Destructor function for the store.
static void
free_store_item (void **store_item)
{
    free (*store_item);
    *store_item = NULL;
}


//  --------------------------------------------------------------------------
/// Create a new store item.
static store_item *
new_store_item (int key, int seq)
{
    store_item *item = malloc (sizeof (store_item));
    assert (item);

    item->key = key;
    item->seq = seq;

    return item;
}


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
    sam_be_rmq_t *self = args;

    amqp_frame_t frame;
    struct timeval timeout = { .tv_sec = 0, .tv_usec = 0 };

    int rc = amqp_simple_wait_frame_noblock (
        self->amqp.connection, &frame, &timeout);

    while (rc != AMQP_STATUS_TIMEOUT) {

        // retrieve frame contents
        check_amqp_ack_frame (frame);
        amqp_basic_ack_t *props = frame.payload.method.decoded;
        assert (props);
        sam_log_tracef (
            "'%s' received ack no %d", self->name, props->delivery_tag);


        // look the msg key up and update the store
        store_item *item = zlist_first (self->store);
        while (item && item->seq != props->delivery_tag) {
            item = zlist_next (self->store);
        }

        assert (item);
        zsock_send (
            self->psh, "sii", self->name, SAM_RES_ACK, item->key);

        sam_log_tracef (
            "removing %d -> %d from the store", item->key, item->seq);
        zlist_remove (self->store, item);


        // rabbitmq-c acts edge triggered
        rc = amqp_simple_wait_frame_noblock (
            self->amqp.connection, &frame, &timeout);
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Handle publishing request. The frame format contained in the
/// sam_msg must look like this:
///
///    0 | s | <destination exchange>
///    1 | s | <routing key>
///    2 | f | zframe_t * containing the payload
///
static int
handle_publish_req (zloop_t *loop UU, zsock_t *pll, void *args)
{
    sam_be_rmq_t *self = args;

    sam_msg_t *msg;
    int key;

    int rc = zsock_recv (pll, "ip", &key, &msg);
    if (rc) {
        sam_log_error ("receive failed");
    }

    char *exchange, *routing_key;
    zframe_t *payload;

    rc = sam_msg_get (
        msg, "ssf", &exchange, &routing_key, &payload);
    assert (!rc);

    unsigned int seq = self->amqp.seq;
    rc = sam_be_rmq_publish (
        self,
        exchange,
        routing_key,
        zframe_data (payload),
        zframe_size (payload));

    if (rc) {
        return -1;
    }

    sam_log_tracef ("saving message %d -> %d to the store", key, seq);
    zlist_append (self->store, new_store_item (key, seq));

    sam_msg_destroy (&msg);
    free (exchange);
    free (routing_key);
    zframe_destroy (&payload);

    return 0;
}


//  --------------------------------------------------------------------------
/// Handle rpc request. The frame format contained in the sam_msg must
/// look like this:
///
///    0 | s | "exchange.declare", "exchange.delete"
///    1 | s | <exchange name>
///    2 | s | <type> for "exchange.declare"
///
static int
handle_rpc_req (zloop_t *loop UU, zsock_t *rep, void *args)
{
    sam_be_rmq_t *self = args;

    char *action;
    sam_msg_t *msg;

    int rc = zsock_recv (rep, "p", &msg);
    if (rc) {
        sam_log_error ("receive failed");
        return -1;
    }

    rc = sam_msg_get (msg, "s", &action);
    assert (!rc);

    if (!strcmp (action, "exchange.declare")) {
        char *exchange, *type;
        free (action);
        rc = sam_msg_get (msg, "sss", &action, &exchange, &type);
        assert (!rc);
        rc = sam_be_rmq_exchange_declare (self, exchange, type);
        free (exchange);
        free (type);
    }

    else if (!strcmp (action, "exchange.delete")) {
        char *exchange;
        free (action);
        rc = sam_msg_get (msg, "ss", &action, &exchange);
        assert (!rc);
        rc = sam_be_rmq_exchange_delete (self, exchange);
        free (exchange);
    }

    else {
        assert (false);
    }

    free (action);
    rc = zsock_send (rep, "i", rc);
    return rc;
}


//  --------------------------------------------------------------------------
/// Entry point for the actor thread. Starts a loop listening on the
/// PIPE, REP zsock and the AMQP TCP socket.
static void
actor (zsock_t *pipe, void *args)
{
    sam_be_rmq_t *self = args;
    sam_log_info ("started be_rmq actor");

    zmq_pollitem_t amqp_pollitem = {
        .socket = NULL,
        .fd = sam_be_rmq_sockfd (self),
        .events = ZMQ_POLLIN,
        .revents = 0
    };

    zloop_t *loop = zloop_new ();
    self->amqp_pollitem = &amqp_pollitem;

    zloop_reader (loop, pipe, sam_gen_handle_pipe, NULL);
    zloop_reader (loop, self->publish_pll, handle_publish_req, self);
    zloop_reader (loop, self->rpc_rep, handle_rpc_req, self);

    zloop_poller (loop, &amqp_pollitem, handle_amqp, self);

    sam_log_trace ("send ready signal");
    zsock_signal (pipe, 0);
    zloop_start (loop);

    sam_log_info ("stopping be_rmq actor");
    zloop_destroy (&loop);
}


//  --------------------------------------------------------------------------
/// This helper function analyses the return value of an AMQP rpc
/// call. If the return code is anything other than
/// AMQP_RESPONSE_NORMAL, an assertion fails.
static int
try (char const *ctx, amqp_rpc_reply_t x)
{

    switch (x.reply_type) {

    case AMQP_RESPONSE_NORMAL:
        return 0;

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

    return -1;
}


//  --------------------------------------------------------------------------
/// Returns the underlying TCP connections socket file descriptor.
int
sam_be_rmq_sockfd (sam_be_rmq_t *self)
{
    return amqp_get_sockfd (self->amqp.connection);
}


//  --------------------------------------------------------------------------
/// Creates a new instance of be_rmq. This instance wraps an
/// AMQP TCP connection to a RabbitMQ broker. This constructor
/// function creates an AMQP connection state and initializes a TCP
/// socket for the broker connection.
sam_be_rmq_t *
sam_be_rmq_new (const char *name)
{
    sam_log_infof (
        "creating rabbitmq message backend (%s)", name);

    sam_be_rmq_t *self = malloc (sizeof (sam_be_rmq_t));
    assert (self);

    self->name = malloc (strlen (name) * sizeof (char) + 1);
    assert (self->name);
    strcpy (self->name, name);

    // init amqp
    self->amqp.seq = 1;
    self->amqp.message_channel = 1;
    self->amqp.method_channel = 2;
    self->amqp.connection = amqp_new_connection ();
    self->amqp.socket = amqp_tcp_socket_new (self->amqp.connection);

    // init store
    self->store = zlist_new ();
    zlist_set_destructor (self->store, free_store_item);

    return self;
}


//  --------------------------------------------------------------------------
/// Destroy an instance of be_rmq. This destructor function
/// savely closes the TCP connection to the broker and free's all
/// allocated memory.
void
sam_be_rmq_destroy (sam_be_rmq_t **self)
{
    sam_log_trace ("destroying rabbitmq message backend instance");
    zlist_destroy (&(*self)->store);


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

    free ((*self)->name);
    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
/// Establish a connection to the RabbitMQ broker. This function opens
/// the connection on the TCP socket. It then opens a channel for
/// communication, which it sets into confirm mode.
void
sam_be_rmq_connect (
    sam_be_rmq_t *self,
    sam_be_rmq_opts_t *opts)
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
sam_be_rmq_publish (
    sam_be_rmq_t *self,
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
sam_be_rmq_handle_ack (sam_be_rmq_t *self)
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
int
sam_be_rmq_exchange_declare (
    sam_be_rmq_t *self,
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

    return try (
        "declare exchange", amqp_get_rpc_reply(self->amqp.connection));
}


//  --------------------------------------------------------------------------
/// Delete an exchange.
int
sam_be_rmq_exchange_delete (
    sam_be_rmq_t *self,
    const char *exchange UU)
{
    amqp_exchange_delete (
        self->amqp.connection,
        self->amqp.method_channel,
        amqp_cstring_bytes (exchange),
        0);

    return try (
        "delete exchange", amqp_get_rpc_reply(self->amqp.connection));
}


//  --------------------------------------------------------------------------
/// Start an actor handling requests asynchronously. Internally, it
/// starts an zactor with a zloop listening to data on either the
/// actors pipe, reply socket or the AMQP TCP socket. The provided
/// be_rmq instance must already have openend a connection and
/// be logged in to the broker.
sam_backend_t *
sam_be_rmq_start (
    sam_be_rmq_t **self,
    char *pll_endpoint)
{
    char buf [64];
    sam_log_trace ("starting message backend actor");


    // create backend
    sam_backend_t *backend = malloc (sizeof (sam_backend_t));
    assert (backend);
    backend->name = (*self)->name;


    // publish PUSH/PULL
    snprintf (buf, 64, "inproc://be_rmq-%s-publish", (*self)->name);

    (*self)->publish_pll = zsock_new_pull (buf);
    assert ((*self)->publish_pll);

    backend->publish_psh = zsock_new_push (buf);
    assert (backend->publish_psh);

    sam_log_tracef ("generated rmq psh/pull publishing endpoint '%s'", buf);


    // rpc REQ/REP
    snprintf (buf, 64, "inproc://be_rmq-%s-rpc", (*self)->name);

    (*self)->rpc_rep = zsock_new_rep (buf);
    assert ((*self)->rpc_rep);

    backend->rpc_req = zsock_new_req (buf);
    assert (backend->rpc_req);

    sam_log_tracef ("generated rmq req/rep rpc endpoints '%s'", buf);


    // feedback PUSH
    (*self)->psh = zsock_new_push (pll_endpoint);
    assert ((*self)->psh);
    sam_log_tracef ("connected push socket to '%s'", pll_endpoint);


    // change ownership
    backend->_self = *self;
    backend->_actor = zactor_new (actor, *self);
    *self = NULL;

    return backend;
}


//  --------------------------------------------------------------------------
/// Stop the message backend thread. When calling this function, all
/// sockets for communicating with the thread are closed and all
/// memory allocated by sam_be_rmq_start is free'd. By calling
/// this function, ownership of the be_rmq instance is
/// reclaimed.
sam_be_rmq_t *
sam_be_rmq_stop (
    sam_backend_t **backend)
{
    sam_be_rmq_t *self = (*backend)->_self;
    zactor_destroy (&(*backend)->_actor);

    // publish PUSH/PULL
    zsock_destroy (&(*backend)->publish_psh);
    zsock_destroy (&self->publish_pll);

    // rpc REQ/REP
    zsock_destroy (&(*backend)->rpc_req);
    zsock_destroy (&self->rpc_rep);

    // feedback PUSH
    zsock_destroy (&self->psh);

    free (*backend);
    *backend = NULL;

    return self;
}
