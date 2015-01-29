
#include "util.h"


void
a_login (
    amqp_connection_state_t conn,
    const char *user,
    const char *pass,
    const int chan)
{
    a_try("logging in", amqp_login(
        conn, "/", 0, 131072, 0,
        AMQP_SASL_METHOD_PLAIN,
        user, pass));

    amqp_channel_open (conn, chan);
    a_try ("opening channel", amqp_get_rpc_reply (conn));
}


void
a_logout (amqp_connection_state_t conn, const int chan)
{
    a_try ("closing channel",
           amqp_channel_close (conn, chan, AMQP_REPLY_SUCCESS));

    a_try ("closing connection",
           amqp_connection_close (conn, AMQP_REPLY_SUCCESS));

    int rc = amqp_destroy_connection (conn);
    assert (rc >= 0);
}


void
a_declare_and_bind (
    amqp_connection_state_t conn,
    const int chan,
    const char *queue,
    const char *exchange,
    const char *binding)
{
    amqp_queue_declare_ok_t *r = amqp_queue_declare (
        conn, chan, amqp_cstring_bytes (queue), 0, 0, 0, 1, amqp_empty_table);

    a_try ("declaring queue", amqp_get_rpc_reply (conn));

    printf ("declared new queue: %.*s\n", (int) r->queue.len, (char *) r->queue.bytes);
    amqp_bytes_t queue_name = amqp_bytes_malloc_dup (r->queue);
    // TODO this must be free'd? nothing to see in the examples...

    if (queue_name.bytes == NULL) {
        fprintf (stderr, "ran out of memory while copying queue name\n");
        assert (0);
    }

    printf ("binding queue to '%s' with key '%s'\n", PC_EXCHANGE, PC_BINDING);
    // bind the queue
    amqp_queue_bind (
        conn, 1, queue_name,
        amqp_cstring_bytes (exchange),
        amqp_cstring_bytes (queue),
        amqp_empty_table);
    a_try ("binding queue", amqp_get_rpc_reply (conn));

    amqp_basic_consume (
        conn, 1, queue_name, amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
    a_try ("consuming", amqp_get_rpc_reply (conn));
}

int
a_publish (
    amqp_connection_state_t conn,
    const int chan,
    const char *exch,
    const char *routing_key,
    char *msg)
{
    amqp_bytes_t msg_bytes;
    msg_bytes.len = sizeof (msg);
    msg_bytes.bytes = msg;

    return amqp_basic_publish (
        conn, chan,
        amqp_cstring_bytes(exch),
        amqp_cstring_bytes(routing_key),
        0, 0, NULL, msg_bytes);
}

void *
a_try (char const *ctx, amqp_rpc_reply_t x)
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
