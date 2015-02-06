
#include "common.h"


static int
pipe_callback (zloop_t *loop, zsock_t *pipe, void *args)
{
    printf ("[msgd] pipe callback invoked\n");
    zmsg_t *msg = zmsg_recv (pipe);
    char *cmd = zmsg_popstr (msg);
    bool term = false;

    if (!strcmp (cmd, "$TERM")) {
        printf ("[msgd] terminating\n");
        term = true;
    }

    free (cmd);
    zmsg_destroy (&msg);
    
    if (term) {
        return -1;
    }

    return 0;
}



static int
amqp_callback (zloop_t *loop, zmq_pollitem_t *amqp, void *args)
{
    msgd_state *state = args;
    int rc = rabbit_handle_ack (state->rabbit);
    state->ack_c += rc;
    return 0;
}


static int
pll_callback (zloop_t *loop, zsock_t *pll, void *args)
{
    int cmd;
    zsock_recv (pll, "i", &cmd);
    msgd_state *state = args;

    if (cmd == SEND_MESSAGE) {
        rabbit_publish (state->rabbit, EXCHANGE, BINDING, "hi!");
    }
    else if (cmd == SEND_METHOD) {
        char *exch;
        asprintf (&exch, "exch-%d", state->exch_c);
        rabbit_declare_exchange (state->rabbit, exch, "topic");
        state->exch_c += 1;
        return amqp_callback (loop, state->amqp, args);
    }
    else {
        assert (false);
    }

    return 0;
}


static void
msgd (zsock_t *pipe, void *args)
{
    printf ("[msgd] creating rabbit instance\n");
    rabbit_login_t *login_data = args;
    rabbit_t *rabbit = rabbit_new (login_data);
    rabbit_enable_pubconf (rabbit);

    zmq_pollitem_t amqp = {
        .socket = NULL,
        .fd = rabbit_get_sockfd (rabbit),
        .events = ZMQ_POLLIN,
        .revents = 0
    };

    msgd_state state = {
        .rabbit = rabbit,
        .amqp = &amqp,
        .ack_c = 0,
        .exch_c = 0
    };

    printf ("[msgd] create PULL on %s\n", MSGD_PLL_ENDPOINT);
    zsock_t *pll = zsock_new_pull (MSGD_PLL_ENDPOINT);
    zloop_t *loop = zloop_new ();

    zloop_reader (loop, pipe, pipe_callback, NULL);
    zloop_reader (loop, pll, pll_callback, &state);
    zloop_poller (loop, &amqp, amqp_callback, &state);

    printf ("[msgd] sending signal\n");
    zsock_signal (pipe, 0);

    zloop_start (loop);
    printf ("[msgd] loop finished, handled %d ack's\n", state.ack_c);

    printf ("[msgd] destroying rabbit\n");
    rabbit_destroy (&rabbit);

    zloop_destroy (&loop);
    zsock_destroy (&pll);
    printf ("[msgd] cleaned up, exiting\n");
}


msgd_t *
msgd_new (rabbit_login_t *login_data)
{
    msgd_t *self = malloc (sizeof (msgd_t));
    assert (self);

    zactor_t *actor = zactor_new (msgd, login_data);
    self->actor = actor;
    return self;
}


void
msgd_destroy (msgd_t **self)
{
    assert (self);
    zactor_destroy (&(*self)->actor);
    free (*self);
    *self = NULL;
}
