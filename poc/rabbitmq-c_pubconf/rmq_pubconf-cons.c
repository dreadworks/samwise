
#include "util.h"


void *
handle_other (amqp_rpc_reply_t repl)
{
    printf ("handling other\n");
    return NULL;
}


void *
handle_message (amqp_rpc_reply_t repl, amqp_envelope_t *envelope)
{
    printf ("handling message\n");
    
    if (AMQP_RESPONSE_NORMAL != repl.reply_type) {
        handle_other (repl);
    } else {
        printf ("received normal message\n");
        amqp_destroy_envelope (envelope);
    }

    return NULL;
}


void *
get_messages (amqp_connection_state_t conn)
{
    while (1) {
        printf ("waiting for messages\n");

        amqp_envelope_t envelope;
        amqp_maybe_release_buffers (conn);

        handle_message (
            amqp_consume_message (conn, &envelope, NULL, 0),
            &envelope);

    }
    return NULL;
}


int
main (int argc, char *argv [])
{
    if (argc < 3) {
        fprintf (stderr, "usage: %s host port\n", argv [0]);
    }

    const char
        *host = argv[1],
        *user = "guest",
        *pass = "guest";

    int
        port  = atoi (argv [2]);

    printf ("connecting to '%s:%d'\n", host, port);
    amqp_connection_state_t conn = amqp_new_connection ();
    amqp_socket_t *sock = amqp_tcp_socket_new (conn);
    assert (sock);

    int rc = amqp_socket_open (sock, host, port);
    assert (!rc);

    a_login (conn, user, pass, PC_CHAN);
    a_declare_and_bind (conn, PC_CHAN, PC_QUEUE, PC_EXCHANGE, PC_BINDING);
    a_enable_pubconf (conn, PC_CHAN);

    get_messages(conn);

    printf ("closing connection...\n");
    a_logout (conn, PC_CHAN);

    printf ("exiting\n");
    return EXIT_SUCCESS;
}
