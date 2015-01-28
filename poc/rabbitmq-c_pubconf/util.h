
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <amqp.h>


amqp_rpc_reply_t
a_login (amqp_connection_state_t conn, const char *user, const char *pass);

int
a_publish (
    amqp_connection_state_t conn,
    const int chan,
    const char *queue,
    char *msg);

void *
a_try (const char *ctx, amqp_rpc_reply_t x);
