
#include "common.h"


int
main (int argc, char *argv [])
{
    if (argc < 4) {
        fprintf (stderr, "usage: %s host port message_count\n", argv [0]);
        exit (2);
    }
    int msg_c = atoi (argv [3]);

    rabbit_login_t login_data = {
        .host = argv [1],
        .port = atoi (argv [2]),
        .chan = CHAN,
        .user = "guest",
        .pass = "guest"
    };

    printf ("[main] creating msgd\n");
    msgd_t *msgd = msgd_new (&login_data);

    printf ("\npress key to send %d message(s)\n", msg_c);
    getchar ();

    zsock_t *psh = zsock_new_push (MSGD_PLL_ENDPOINT);
    while (msg_c > 0) {
        zsock_send (psh, "i", SEND_MESSAGE);
        msg_c -= 1;
        if (msg_c % 100 == 0) {
            zsock_send (psh, "i", SEND_METHOD);
        }
    }

    getchar ();

    printf ("[main] testing starvation\n");
    zsock_send (psh, "i", SEND_METHOD);

    getchar ();

    // clean up
    zsock_destroy (&psh);
    printf ("[main] destroying msgd\n");
    msgd_destroy (&msgd);

    return EXIT_SUCCESS;
}
