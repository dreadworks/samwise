#ifndef _COMMON_H_
#define _COMMON_H_


#define _GNU_SOURCE

#include <czmq.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>

#include "rabbit.h"
#include "msgd.h"


// some shared config
#define EXCHANGE    "amq.direct"
#define QUEUE       "test queue"
#define ROUTING_KEY "test"
#define BINDING     "test"
#define CHAN        1

#define MSGD_PLL_ENDPOINT "inproc://msgd-pull"

#endif
