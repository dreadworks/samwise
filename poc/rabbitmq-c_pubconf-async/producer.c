
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
        zsock_send (psh, "z");
        msg_c -= 1;
    }
    zsock_destroy (&psh);

    getchar ();

    printf ("[main] destroying msgd\n");
    msgd_destroy (&msgd);

    return EXIT_SUCCESS;
}
