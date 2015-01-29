
#include "util.h"


void *
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
        "sending %d messages to '%s' - routing key '%s'\n",
        msg_c, exch, routing_key);

    while (msg_c) {
        int rc = a_publish (conn, chan, exch, routing_key, msg);
        assert (!rc);
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
    send_messages (conn, PC_CHAN, PC_EXCHANGE, PC_ROUTING_KEY, msg_c);

    printf ("closing connection...\n");
    a_logout (conn, PC_CHAN);

    printf ("exiting\n");
    return EXIT_SUCCESS;
}
