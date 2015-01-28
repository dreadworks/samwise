
#include <amqp_tcp_socket.h>

#include "util.h"


void *
send_messages (
    amqp_connection_state_t conn,
    const int chan,
    const char *queue_name,
    int msg_c)
{
    char msg[256];

    int i;
    for (i = 0; i < msg_c; i++) {
        msg[i] = i & 0xff;
    }

    printf ("sending %d messages\n", msg_c);
    while (msg_c) {
        int rc = a_publish (conn, chan, queue_name, msg);
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
        *pass = "guest",
        *queue_name = "pubconf";

    const int chan = 1;

    int
        port  = atoi (argv [2]),
        msg_c = atoi (argv [3]),
        rc;

    amqp_rpc_reply_t status;

    
    printf ("connecting to '%s:%d'...\n", host, port);
    amqp_connection_state_t conn = amqp_new_connection ();
    amqp_socket_t *sock = amqp_tcp_socket_new (conn);
    assert (sock);

    rc = amqp_socket_open (sock, host, port);
    assert (!rc);

    a_try ("logging in", a_login (conn, user, pass));

    amqp_channel_open (conn, chan);
    a_try ("opening channel", amqp_get_rpc_reply (conn));

    send_messages (conn, chan, queue_name, msg_c);

    printf ("closing connection...\n");
    a_try ("closing channel",
           amqp_channel_close (conn, chan, AMQP_REPLY_SUCCESS));

    a_try ("closing connection",
           amqp_connection_close (conn, AMQP_REPLY_SUCCESS));

    printf ("ending connection\n");
    rc = amqp_destroy_connection (conn);
    assert (rc >= 0);

    printf ("exiting\n");
    return EXIT_SUCCESS;
}
