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

   <code>

   libsam actor | sam_be_rmq actor
   -------------------------------
     PIPE: libsam actor spawns the rmq actor
     REQ/REP: Synchronous RPC requests
     PSH/PLL: Asynchronous publishing requests

   sam_be_rmq_actor | sam_buf
   --------------------------
     PSH/PLL: Asynchronous acknowledgments

   sam_be_rmq | RabbitMQ Broker
   ----------------------------
     raw TCP: AMQP traffic


   Topology:
   ---------
                    o  libsam actor  o
                REQ ^       |       / PUSH
                     \     PIPE    /
                      \     |     /
                   REP v    |    v PULL
          PULL          o   |    o
   sam_buf o <----- o sam_be_rmq o <----------> RabbitMQ Broker
     actor       PUSH    actor

    </code>
*/

#include "../include/sam_prelude.h"


/// store item to be able to map the message key -> sequence number
typedef struct store_item {
    unsigned int seq;     ///< amqp sequence number for publisher confirms
    int key;              ///< message key assigned outside of this module
} store_item;


//  --------------------------------------------------------------------------
/// Return a string representation of the current backend
/// state. Memory must be free'd by the caller.
static char *
to_string (
    sam_backend_t *be)
{
    size_t buf_size = 256;
    char *str = malloc (buf_size * sizeof (char));

    sam_be_rmq_t *self = be->_self;
    sam_be_rmq_opts_t *opts = &self->connection.opts;

    snprintf (str, buf_size,
              "%s (id: 0x%" PRIx64 ") (%s:%d as '%s'):\n"
              "  connected: %s (%d/%d tries every %" PRIu64 "ms)\n"
              "  heartbeat: every %d seconds\n"
              "  current sequence number: %d\n"
              "  store size: %zu",

              self->name, self->id, opts->host, opts->port, opts->user,
              (self->connection.established)? "yep": "nope",
              self->connection.tries, opts->tries, opts->interval,
              opts->heartbeat,
              self->amqp.seq,
              zlist_size (self->store));

    return str;
}



//  --------------------------------------------------------------------------
/// Destructor function for the store.
static void
free_store_item (
    void **store_item)
{
    free (*store_item);
    *store_item = NULL;
}


//  --------------------------------------------------------------------------
/// Create a new store item.
static store_item *
new_store_item (
    int key,
    int seq)
{
    store_item *item = malloc (sizeof (store_item));
    assert (item);

    item->key = key;
    item->seq = seq;

    return item;
}


//  --------------------------------------------------------------------------
/// Create an amqp_bytes_t from a char * (that can be NULL)
static amqp_bytes_t
c_bytes (const char *str)
{
    amqp_bytes_t ab = {
        .len = 0,
        .bytes = NULL
    };

    if (str == NULL) {
        return ab;
    }

    return amqp_cstring_bytes (str);
}


//  --------------------------------------------------------------------------
/// Creates a uint8_t from a string (that can be NULL)
static uint8_t
c_uint8 (const char *str)
{
    if (str == NULL || strlen (str) == 0) {
        return 0;
    }

    int word = atoi (str);
    if (word < 0 || 0xFF < word) {
        sam_log_errorf ("provided value %d does not fit in one word", word);
        return 0;
    }

    return word;
}


// cyclic
static int connection_loss (sam_be_rmq_t *self, zloop_t *loop);


//  --------------------------------------------------------------------------
/// This function reads all necessary information from a frame and
/// publishes the acknowledgement.
static void
handle_ack (
    sam_be_rmq_t *self,
    amqp_frame_t *frame)
{
    // retrieve frame contents
    amqp_basic_ack_t *props = frame->payload.method.decoded;

    assert (props);
    sam_log_tracef (
        "'%s' received ack no %d", self->name, props->delivery_tag);

    // look the msg key up and update the store
    store_item *item = zlist_first (self->store);
    while (item && item->seq != props->delivery_tag) {
        if (props->multiple) {
            zlist_remove (self->store, item);
            item = zlist_first (self->store);
        }

        else {
            // this must not happen
            assert (false);
        }
    }

    assert (item);
    sam_log_tracef ("send () ack for '%d'", item->key);

    zframe_t *id = zframe_new (&self->id, sizeof (self->id));
    zsock_send (self->sock.ack, "fi", id, item->key);
    zframe_destroy (&id);

    sam_log_tracef (
        "'%s' removes %d (seq: %d) from the store",
        self->name, item->key, item->seq);

    zlist_remove (self->store, item);
}


//  --------------------------------------------------------------------------
/// Callback for POLLIN events on the AMQP TCP socket. While the
/// poll-loop works in a level triggered fashion, the AMQP library
/// greedily reads all content from the socket and buffers the frames
/// internally. Because of this, a loop gets started that reads from
/// this data structure with a 0 timeout (NULL as timeout would turn
/// the call into a blocking read).
static int
handle_amqp (
    zloop_t *loop,
    zmq_pollitem_t *amqp UU,
    void *args)
{
    sam_be_rmq_t *self = args;

    amqp_frame_t frame;
    struct timeval timeout = { .tv_sec = 0, .tv_usec = 0 };

    int rc = amqp_simple_wait_frame_noblock (
        self->amqp.connection, &frame, &timeout);


    // rabbitmq-c acts edge triggered, so this loop needs to
    // eat all currently buffered frames
    while (rc == AMQP_STATUS_OK) {

        // this must be handled
        if (frame.payload.method.id != AMQP_BASIC_ACK_METHOD) {
            sam_log_errorf (
                "got something different than an ack: %d",
                frame.payload.method.id);
            assert (false);
        }

        // handle acknowledgement
        handle_ack (self, &frame);
        rc = amqp_simple_wait_frame_noblock (
            self->amqp.connection, &frame, &timeout);
    }

    // handle disconnect
    if (rc != AMQP_STATUS_TIMEOUT) {
        sam_log_errorf (
            "looks like '%s' is no longer available (%d)",
            self->name, rc);

        return connection_loss (self, loop);
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Try to re-connect to a broker. Sets a timer if the re-connect
/// failed and there are remaining tries.
static int
handle_reconnect (
    zloop_t *loop,
    int timer_id UU,
    void *args)
{
    sam_be_rmq_t *self = args;

    // invoke callback immediately
    if (self->connection.tries) {
        if (self->connection.tries > 0) {
            self->connection.tries -= 1;
        }

        sam_log_infof (
            "trying to reconnect '%s' (%d tries remaining)",
            self->name, self->connection.tries);

        // attempt re-connect
        int rc = sam_be_rmq_connect (self, &self->connection.opts);


        // reconnect failed, set timer for next round
        if (rc) {
            uint64_t iv = self->connection.opts.interval;
            sam_log_infof (
                "reconnecting '%s' failed, next try in %u",
                self->name, iv);

            zloop_timer (loop, iv, 1, handle_reconnect, self);
        }


        // re-connect successful; re-register callback
        else {
            sam_log_infof ("successfully reconnected '%s'", self->name);
            self->sock.amqp->fd = sam_be_rmq_sockfd (self);
            zloop_poller (loop, self->sock.amqp, handle_amqp, self);
        }
    }

    // no tries left, shut down backend
    else {
        zsock_send (
            self->sock.sig, "is",
            SAM_BE_SIG_KILL, self->name);
        return -1;
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Either tries to reconnect or prepares for self-destruction. If the
/// re-connect fails and there are tries left, a timer is set to retry
/// at some later time.
static int
connection_loss (
    sam_be_rmq_t *self,
    zloop_t *loop)
{
    assert (self);
    assert (loop);
    assert (self->connection.established);

    self->connection.established = false;
    zsock_send (
        self->sock.sig, "is",
        SAM_BE_SIG_CONNECTION_LOSS, self->name);


    // unsubscribe amqp poller
    zloop_poller_end (loop, self->sock.amqp);


    // attempt re-connect
    zloop_timer (loop, 0, 1, handle_reconnect, self);
    return 0;
}


//  --------------------------------------------------------------------------
/// Handle publishing request. The frame format contained in the
/// sam_msg must look like this:
///
///    0 | s | destination exchange
///    1 | s | routing key
///    2 | i | mandatory
///    3 | i | immediate
///    4 | l | list of options
///    5 | l | list of headers
///    6 | f | zframe_t * containing the payload
///
static int
handle_publish_req (
    zloop_t *loop,
    zsock_t *pll,
    void *args)
{
    sam_be_rmq_t *self = args;

    sam_msg_t *msg;
    int key;

    int rc = zsock_recv (pll, "ip", &key, &msg);
    if (rc) {
        sam_log_errorf ("'%s' receive failed", self->name);
        return 0;
    }


    if (!self->connection.established) {
        sam_log_tracef (
            "backend '%s' not connected, discarding publishing request",
            self->name);

        sam_msg_destroy (&msg);
        return 0;
    }


    // retrieve data and prepare options
    sam_be_rmq_pub_t opts;
    zlist_t
        *props,
        *headers;

    rc = sam_msg_get (
        msg, "ssiillf",

        &opts.exchange,
        &opts.routing_key,
        &opts.mandatory,
        &opts.immediate,

        &props,
        &headers,

        &opts.payload);
    assert (!rc);


    // set props
    uint prop_c = 12;
    assert (zlist_size (props) == prop_c);

    char *prop = zlist_first (props);
    char **prop_ptr = (char **) &opts.props;
    while (prop_c) {
        *prop_ptr = prop;
        prop_ptr += 1;
        prop = zlist_next (props);
        prop_c -= 1;
    }

    opts.headers = headers;

    // publish
    unsigned int seq = self->amqp.seq;
    rc = sam_be_rmq_publish (self, &opts);
    if (rc == SAM_BE_SIG_CONNECTION_LOSS) {
        return connection_loss (self, loop);
    }

    else if (rc) {
        return -1;
    }

    sam_log_tracef (
        "'%s' saves message %d (seq: %d) to the store",
        self->name, key, seq);
    zlist_append (self->store, new_store_item (key, seq));


    // clean up
    sam_msg_destroy (&msg);
    free (opts.exchange);
    free (opts.routing_key);
    zlist_destroy (&props);
    zlist_destroy (&headers);
    zframe_destroy (&opts.payload);

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
handle_rpc_req (
    zloop_t *loop UU,
    zsock_t *rep,
    void *args)
{
    sam_be_rmq_t *self = args;

    char *action;
    sam_msg_t *msg;

    int rc = zsock_recv (rep, "p", &msg);
    if (rc) {
        sam_log_errorf ("'%s' receive failed", self->name);
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
actor (
    zsock_t *pipe,
    void *args)
{
    sam_be_rmq_t *self = args;
    sam_log_infof ("'%s' started be_rmq actor", self->name);

    zmq_pollitem_t amqp_pollitem = {
        .socket = NULL,
        .fd = sam_be_rmq_sockfd (self),
        .events = ZMQ_POLLIN,
        .revents = 0
    };

    self->sock.amqp = &amqp_pollitem;
    zloop_t *loop = zloop_new ();

    zloop_reader (loop, pipe, sam_gen_handle_pipe, NULL);
    zloop_reader (loop, self->sock.pub, handle_publish_req, self);
    zloop_reader (loop, self->sock.rpc, handle_rpc_req, self);
    zloop_poller (loop, self->sock.amqp, handle_amqp, self);

    if (!self->connection.established) {
        uint64_t iv = self->connection.opts.interval;
        sam_log_tracef (
            "starting actor without broker connection, retry in %u", iv);
        zloop_timer (loop, iv, 1, handle_reconnect, self);
    }

    zsock_signal (pipe, 0);
    zloop_start (loop);

    sam_log_infof ("'%s' stopping be_rmq actor", self->name);
    zloop_destroy (&loop);
}


//  --------------------------------------------------------------------------
/// This helper function analyses the return value of an AMQP rpc
/// call. If the return code is anything other than
/// AMQP_RESPONSE_NORMAL, this function returns -1.
static int
try (
    char const *ctx,
    amqp_rpc_reply_t x)
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
sam_be_rmq_sockfd (
    sam_be_rmq_t *self)
{
    return amqp_get_sockfd (self->amqp.connection);
}


//  --------------------------------------------------------------------------
/// Creates a new instance of be_rmq. This instance wraps an
/// AMQP TCP connection to a RabbitMQ broker. This constructor
/// function creates an AMQP connection state and initializes a TCP
/// socket for the broker connection.
sam_be_rmq_t *
sam_be_rmq_new (
    const char *name,
    uint64_t id)
{
    sam_log_infof (
        "creating rabbitmq message backend (%s:%d)", name, id);

    sam_be_rmq_t *self = malloc (sizeof (sam_be_rmq_t));
    assert (self);

    self->name = malloc (strlen (name) * sizeof (char) + 1);
    assert (self->name);
    strcpy (self->name, name);

    self->id = id;
    self->store = NULL;

    // init amqp
    memset (&self->amqp.connection, 0, sizeof (amqp_connection_state_t));
    self->amqp.message_channel = 1;
    self->amqp.method_channel = 2;

    self->connection.established = false;
    return self;
}


//  --------------------------------------------------------------------------
/// Destroy an instance of be_rmq. This destructor function
/// savely closes the TCP connection to the broker and free's all
/// allocated memory.
void
sam_be_rmq_destroy (
    sam_be_rmq_t **self)
{
    sam_log_tracef (
        "destroying rabbitmq message backend instance '%s'",
        (*self)->name);

    zlist_destroy (&(*self)->store);

    if ((*self)->connection.established) {
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
    }

    int rc = amqp_destroy_connection ((*self)->amqp.connection);
    assert (rc >= 0);


    free ((*self)->name);
    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
/// Establish a connection to the RabbitMQ broker. This function opens
/// the connection on the TCP socket. It then opens a channel for
/// communication, which it sets into confirm mode. It will always
/// copy the opts parameter into its internal state for later use.
int
sam_be_rmq_connect (
    sam_be_rmq_t *self,
    sam_be_rmq_opts_t *opts)
{
    assert (self);
    assert (opts);

    sam_log_infof (
        "'%s' connecting to %s:%d",
        self->name,
        opts->host,
        opts->port);


    // save options for reconnects
    memcpy (&self->connection.opts, opts, sizeof (sam_be_rmq_opts_t));
    self->connection.tries = opts->tries;


    // for re-initialize rabbitmq-c
    if (self->amqp.connection) {
        amqp_destroy_connection (self->amqp.connection);
    }

    self->amqp.connection = amqp_new_connection ();
    self->amqp.socket = amqp_tcp_socket_new (self->amqp.connection);

    int rc = amqp_socket_open (
        self->amqp.socket,
        opts->host,
        opts->port);

    if (rc) {
        sam_log_errorf (
            "could not connect to %s:%d (%s)",
            opts->host,
            opts->port,
            self->name);

        return -1;
    }

    //
    //   login
    //

    sam_log_tracef (
        "'%s' logging in as user '%s'",
        self->name,
        opts->user);

    try ("logging in", amqp_login(
            self->amqp.connection,   // state
            "/",                     // vhost
            0,                       // channel max
            AMQP_DEFAULT_FRAME_SIZE, // frame max
            opts->heartbeat,         // hearbeat
            AMQP_SASL_METHOD_PLAIN,  // sasl method
            opts->user,
            opts->pass));

    //
    //   open and configure channels
    //

    // message channel
    amqp_channel_open (
        self->amqp.connection,
        self->amqp.message_channel);

    try ("opening message channel",
         amqp_get_rpc_reply (self->amqp.connection));

    // method channel
    amqp_channel_open (
        self->amqp.connection,
        self->amqp.method_channel);

    try ("opening method channel",
         amqp_get_rpc_reply (self->amqp.connection));

    // enable publisher confirms
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


    //
    // set state properties
    //

    self->connection.established = true;
    sam_log_tracef (
        "successfully connected to %s:%d "
        "(retry %d times every %ums)",
        opts->host, opts->port, opts->tries, opts->interval);


    self->amqp.seq = 1;

    if (self->store) {
        zlist_destroy (&self->store);
    }

    self->store = zlist_new ();
    zlist_set_destructor (self->store, free_store_item);

    return 0;
}


//  --------------------------------------------------------------------------
/// Publish (basic_publish) a message to the RabbitMQ broker.
int
sam_be_rmq_publish (
    sam_be_rmq_t *self,
    sam_be_rmq_pub_t *opts)
{
    sam_log_tracef (
        "'%s' publishing message %d of size %d",
        self->name,
        self->amqp.seq,
        zframe_size (opts->payload));


    // translate headers
    size_t
        num_headers = zlist_size (opts->headers),
        headers_size = sizeof (amqp_table_entry_t) * num_headers;

    amqp_table_entry_t
        *headers = malloc (headers_size),
        *headers_ptr = headers;

    char *key, *val;
    if (num_headers) {
        key = zlist_first (opts->headers);
        val = zlist_next (opts->headers);

        while (key != NULL && val != NULL) {
            headers_ptr->key.len = strlen (key);
            headers_ptr->key.bytes = key;

            headers_ptr->value.kind = AMQP_FIELD_KIND_BYTES;
            headers_ptr->value.value.bytes.len = strlen (val);
            headers_ptr->value.value.bytes.bytes = val;

            headers_ptr += 1;

            key = zlist_next (opts->headers);
            val = zlist_next (opts->headers);
        }
    }


    // translate props
    amqp_basic_properties_t amqp_props = {
        ._flags           = 0,
        .content_type     = c_bytes (opts->props.content_type),
        .content_encoding = c_bytes (opts->props.content_encoding),
        .headers          = { .num_entries = num_headers, .entries = headers },
        .delivery_mode    = c_uint8 (opts->props.delivery_mode),
        .priority         = c_uint8 (opts->props.priority),
        .correlation_id   = c_bytes (opts->props.correlation_id),
        .reply_to         = c_bytes (opts->props.reply_to),
        .expiration       = c_bytes (opts->props.expiration),
        .message_id       = c_bytes (opts->props.message_id),
        .timestamp        = 0,
        .type             = c_bytes (opts->props.type),
        .user_id          = c_bytes (opts->props.user_id),
        .app_id           = c_bytes (opts->props.app_id),
        .cluster_id       = c_bytes (opts->props.cluster_id)
    };


    amqp_bytes_t payload = {
        .len = zframe_size (opts->payload),
        .bytes = zframe_data (opts->payload)
    };

    int rc = amqp_basic_publish (
        self->amqp.connection,
        self->amqp.message_channel,
        c_bytes (opts->exchange),
        c_bytes (opts->routing_key),
        opts->mandatory,
        opts->immediate,
        &amqp_props,
        payload);

    free (headers);

    if (rc == AMQP_STATUS_HEARTBEAT_TIMEOUT) {
        sam_log_errorf (
            "'%s' connection lost while publishing!",
            self->name);

        return SAM_BE_SIG_CONNECTION_LOSS;
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
sam_be_rmq_handle_ack (
    sam_be_rmq_t *self)
{
    amqp_frame_t frame;
    struct timeval timeout = { .tv_sec = 0, .tv_usec = 0 };

    int rc = amqp_simple_wait_frame_noblock (
        self->amqp.connection, &frame, &timeout);

    int ack_c = 0;

    while (rc != AMQP_STATUS_TIMEOUT) {
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
        "'%s' declaring exchange '%s' (%s)",
        self->name,
        exchange,
        type);

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
    const char *exchange)
{
    sam_log_infof (
        "'%s' deleting exchange '%s'",
        self->name,
        exchange);

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
    char *ack_endpoint)
{
    char buf [64];
    sam_log_tracef (
        "'%s' starting message backend actor",
        (*self)->name);


    // create backend
    sam_backend_t *backend = malloc (sizeof (sam_backend_t));
    assert (backend);
    backend->name = (*self)->name;
    backend->id = (*self)->id;
    backend->str = to_string;


    // signals
    snprintf (buf, 64, "inproc://be_rmq-%s-signal", (*self)->name);
    (*self)->sock.sig = zsock_new_push (buf);
    backend->sock_sig = zsock_new_pull (buf);

    assert ((*self)->sock.sig);
    assert (backend->sock_sig);
    sam_log_tracef (
        "'%s' created pair sockets on '%s'",
        (*self)->name, buf);


    // publish PUSH/PULL
    snprintf (buf, 64, "inproc://be_rmq-%s-publish", (*self)->name);
    (*self)->sock.pub = zsock_new_pull (buf);
    backend->sock_pub = zsock_new_push (buf);

    assert ((*self)->sock.pub);
    assert (backend->sock_pub);
    sam_log_tracef (
        "'%s' created psh/pull pair on '%s'",
        (*self)->name, buf);


    // rpc REQ/REP
    snprintf (buf, 64, "inproc://be_rmq-%s-rpc", (*self)->name);
    (*self)->sock.rpc = zsock_new_rep (buf);
    backend->sock_rpc = zsock_new_req (buf);

    assert ((*self)->sock.rpc);
    assert (backend->sock_rpc);
    sam_log_tracef (
        "'%s' created req/rep pair for rpc '%s'",
        (*self)->name, buf);


    // ack PUSH
    (*self)->sock.ack = zsock_new_push (ack_endpoint);
    assert ((*self)->sock.ack);
    sam_log_tracef (
        "'%s' connected push socket to '%s'",
        (*self)->name, ack_endpoint);


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

    // signals
    zsock_destroy (&(*backend)->sock_sig);
    zsock_destroy (&self->sock.sig);

    // publish PUSH/PULL
    zsock_destroy (&(*backend)->sock_pub);
    zsock_destroy (&self->sock.pub);

    // rpc REQ/REP
    zsock_destroy (&(*backend)->sock_rpc);
    zsock_destroy (&self->sock.rpc);

    // ack PUSH
    zsock_destroy (&self->sock.ack);

    free (*backend);
    *backend = NULL;

    return self;
}
