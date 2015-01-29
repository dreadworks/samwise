#ifndef _PC_UTIL_H_
#define _PC_UTIL_H_


#define _GNU_SOURCE


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <amqp.h>
#include <amqp_tcp_socket.h>


// some shared config
#define PC_EXCHANGE    "amq.direct"
#define PC_QUEUE       "test queue"
#define PC_ROUTING_KEY "test"
#define PC_BINDING     "test"
#define PC_CHAN        1


void
a_login (
    amqp_connection_state_t conn,
    const char *user,
    const char *pass,
    const int chan);


void
a_logout (
    amqp_connection_state_t conn,
    const int chan);


void
a_declare_and_bind (
    amqp_connection_state_t conn,
    const int chan,
    const char *queue,
    const char *exchange,
    const char *binding);


void
a_enable_pubconf (
    amqp_connection_state_t conn,
    const int chan);


int
a_publish (
    amqp_connection_state_t conn,
    const int chan,
    const char *exch,
    const char *routing_key,
    char *msg);

void *
a_try (const char *ctx, amqp_rpc_reply_t x);

#endif
