
#include "util.h"


static void *
handle_message (
    amqp_connection_state_t conn,
    amqp_rpc_reply_t repl,
    amqp_envelope_t *envelope)
{
    printf ("handling message\n");
    amqp_frame_t frame;

    if (repl.reply_type != AMQP_RESPONSE_NORMAL) {

        if (repl.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION
            && repl.library_error == AMQP_STATUS_UNEXPECTED_STATE) {

            if (amqp_simple_wait_frame (conn, &frame) != AMQP_STATUS_OK) {
                fprintf (stderr, "unexpected server response\n");
                assert (0);
            } else {

                if (frame.payload.method.id == AMQP_BASIC_ACK_METHOD) {
                    printf ("received ACK!\n");
                } else {
                    printf ("received other method: %d\n",
                            frame.payload.method.id);
                }
            }

        } else {
            fprintf (stderr, "unexcepted reply %d\n", repl.reply_type);
            assert(0);
        }

    } else {
        // since there is no subscription to a
        // specific queue this should never be called
        assert (0);
    }

    return NULL;
}


static void *
send_messages (
    amqp_connection_state_t conn,
    const int chan,
    const char *exch,
    const char *routing_key,
    int msg_c)
{
    char msg[256];

    int i;
    for (i = 0; i < msg_c; i++) {
        msg[i] = i & 0xff;
    }

    printf (
        "sending %d messages to '%s', routing key '%s'\n",
        msg_c, exch, routing_key);

    while (msg_c) {
        printf ("  | publish\n");
        int rc = a_publish (conn, chan, exch, routing_key, msg);
        assert (!rc);

        amqp_envelope_t envl;
        amqp_maybe_release_buffers (conn);

        printf ("  | consume\n");
        amqp_rpc_reply_t repl = amqp_consume_message (conn, &envl, NULL, 0);
        handle_message (conn, repl, &envl);
        msg_c -= 1;
    }

}


int
main (int argc, char *argv [])
{
    if (argc < 4) {
        fprintf (stderr, "usage: %s host port message_count\n", argv [0]);
    }

    const char
        *host = argv[1],
        *user = "guest",
        *pass = "guest";

    int
        port  = atoi (argv [2]),
        msg_c = atoi (argv [3]);

    printf ("connecting to '%s:%d'\n", host, port);
    amqp_connection_state_t conn = amqp_new_connection ();
    amqp_socket_t *sock = amqp_tcp_socket_new (conn);
    assert (sock);

    int rc = amqp_socket_open (sock, host, port);
    assert (!rc);

    a_login (conn, user, pass, PC_CHAN);
    a_enable_pubconf (conn, PC_CHAN);

    send_messages (conn, PC_CHAN, PC_EXCHANGE, PC_ROUTING_KEY, msg_c);

    printf ("press key to close connection\n");
    getchar ();

    printf ("closing connection...\n");
    a_logout (conn, PC_CHAN);

    printf ("exiting\n");
    return EXIT_SUCCESS;
}
