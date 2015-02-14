/*  =========================================================================

    playground publish loop - to test bursts of message publishing

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief public API
   @file playground_publish_loop.c

   Press a key and publish 10k messages. But don't let the power
   consume you...

   The broker hungers.

*/


#include "../include/sam_prelude.h"


#define BURST 10000


typedef struct state_t {
    sam_logger_t *logger;
    sam_msg_backend_t *backend;
    zactor_t *burster;
} state_t;



//  --------------------------------------------------------------------------
/// An independent thread publishing BURST-times.
static void
publishing_burst (zsock_t *pipe, void *args)
{
    zsock_signal (pipe, 0);
    zsock_t *req = args;
    sam_logger_t *logger = sam_logger_new ("burst", SAM_LOG_ENDPOINT);

    static sam_msg_rabbitmq_message_t amqp_message = {
        .exchange = "amq.direct",
        .routing_key = "",
        .payload = (byte *) "hi!",
        .payload_len = 4
    };

    sam_log_info (logger, "starting publishing burst");
    int64_t start_time = zclock_mono ();
    int p_c = 0;
    for (; p_c < BURST; p_c++) {
        zsock_send (req, "ip", SAM_MSG_REQ_PUBLISH, &amqp_message);

        int seq;
        zsock_recv (req, "i", &seq);
        sam_log_tracef (logger, "received seq %d", seq);
    }

    sam_log_infof (
        logger,
        "finished publishing %d messages in %dms",
        BURST,
        zclock_mono () - start_time);

    printf ("publishing took %zdms\n", zclock_mono () - start_time);

    sam_logger_destroy (&logger);
}


//  --------------------------------------------------------------------------
/// Stdin events, the user pressed a key.
static int
handle_stdin (zloop_t *loop UU, zmq_pollitem_t *poll_stdin UU, void *args)
{
    state_t *state = args;

    if (state->burster) {
        zactor_destroy (&state->burster);
    }

    if (getchar () == 'n') {
        return -1;
    }

    state->burster = zactor_new (publishing_burst, state->backend->req);
    return 0;
}


//  --------------------------------------------------------------------------
/// Callback for msg_rabbitmq answers arriving on the pull socket.
static int
handle_pll (zloop_t *loop UU, zsock_t *pll, void *args)
{
    state_t *state = args;

    sam_msg_res_t res;
    zmsg_t *msg = zmsg_new ();

    zsock_recv (pll, "im", &res, &msg);

    char *seq;

    switch (res) {
    case SAM_MSG_RES_ACK:

        seq = zmsg_popstr (msg);
        sam_log_tracef (state->logger, "received ACK %s", seq);
        free (seq);

        break;


    case SAM_MSG_RES_CONNECTION_LOSS:
        sam_log_error (state->logger, "received CONNECTION LOSS");
        break;

    default:
        sam_log_errorf (state->logger, "received unknown response id: %d", res);
    }

    zmsg_destroy (&msg);
    return 0;
}


//  --------------------------------------------------------------------------
/// Creates a loop that handles ACKS and waits for the user to press a
/// key to publish a bunch of messages.
void
playground_publish_loop ()
{
    state_t state = {
        .logger = sam_logger_new ("publish loop", SAM_LOG_ENDPOINT),
        .backend = NULL
    };

    sam_log_trace (state.logger, "logging trace");
    sam_log_info (state.logger, "logging info");
    sam_log_error (state.logger, "logging error");

    sam_msg_rabbitmq_t *rabbit = sam_msg_rabbitmq_new ();
    sam_msg_rabbitmq_opts_t opts = {
        .host = "localhost",
        .port = 5672,
        .user = "guest",
        .pass = "guest",
        .heartbeat = 5
    };
    sam_msg_rabbitmq_connect (rabbit, &opts);

    char *endpoint = "inproc://test-pll";
    zsock_t *pll = zsock_new_pull (endpoint);
    state.backend = sam_msg_rabbitmq_start (&rabbit, endpoint);

    zmq_pollitem_t poll_stdin = {
        .socket = NULL, .fd = fileno(stdin), .events = ZMQ_POLLIN, .revents = 0
    };

    zloop_t *loop = zloop_new ();
    zloop_reader (loop, pll, handle_pll, &state);
    zloop_poller (loop, &poll_stdin, handle_stdin, &state);

    printf("press any key to publish, 'n' to exit\n");
    zloop_start (loop);
    zloop_destroy (&loop);

    rabbit = sam_msg_rabbitmq_stop (&state.backend);
    zsock_destroy (&pll);

    sam_msg_rabbitmq_destroy (&rabbit);
    sam_logger_destroy (&state.logger);
}
