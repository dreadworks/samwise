
#include "util.h"


amqp_rpc_reply_t
a_login (amqp_connection_state_t conn, const char *user, const char *pass)
{
    return amqp_login(
        conn, "/", 0, 131072, 0,
        AMQP_SASL_METHOD_PLAIN,
        user, pass);
}


int
a_publish (
    amqp_connection_state_t conn,
    const int chan,
    const char *queue,
    char *msg)
{
    amqp_bytes_t msg_bytes;
    msg_bytes.len = sizeof (msg);
    msg_bytes.bytes = msg;

    return amqp_basic_publish (
        conn, chan,
        amqp_cstring_bytes("amq.direct"),
        amqp_cstring_bytes(queue),
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
