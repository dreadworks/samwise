
#ifndef _RABBIT_H_
#define _RABBIT_H_

typedef struct rabbit_t {
    amqp_connection_state_t conn;
    amqp_socket_t *sock;
    int chan;
} rabbit_t;

typedef struct rabbit_login_t {
    const char *host;
    int port;
    int chan;
    const char *user;
    const char *pass;
} rabbit_login_t;


rabbit_t *
rabbit_new (rabbit_login_t *login_data);


void
rabbit_destroy (rabbit_t **self);


void
rabbit_declare_exchange (
    rabbit_t *self,
    const char *exchange,
    const char *type);


void
rabbit_declare_and_bind (
    rabbit_t *self,
    const char *queue,
    const char *exchange,
    const char *binding);


void
rabbit_enable_pubconf (rabbit_t *self);


void
rabbit_publish (
    rabbit_t *self,
    const char *exch,
    const char *rkey,
    char *msg);


int
rabbit_get_sockfd (rabbit_t *self);


int
rabbit_handle_ack (rabbit_t *self);


#endif
