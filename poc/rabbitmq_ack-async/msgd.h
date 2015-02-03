
#ifndef _MSGD_H_
#define _MSGD_H_

typedef struct msgd_t {
    zactor_t *actor;
} msgd_t;


typedef struct msgd_state {
    rabbit_t *rabbit;
    int ack_c;
} msgd_state;


msgd_t *
msgd_new (rabbit_login_t *login_data);


void
msgd_destroy (msgd_t **self);


#endif
