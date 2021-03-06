/*  =========================================================================

    samd - send all message daemon (for samwise)

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief daemon process to accept publishing requests
   @file samd.c

   This is the frontend clients communicate with. Used as a daemon
   process utilizing libsam.

*/


#include "../include/sam.h"
#include "../include/sam_stat.h"
#include "../include/sam_msg.h"
#include "../include/sam_log.h"
#include "../include/sam_cfg.h"


typedef struct samd_t {
    sam_t *sam;              ///< encapsulates a sam thread
    zsock_t *client_rep;     ///< REPLY socket for client requests
    sam_stat_handle_t *stat; ///< gathers metrics
} samd_t;


//  --------------------------------------------------------------------------
/// Helper function to wrap an error message in a sam_ret.
static sam_ret_t *
create_error (
    char *msg)
{
    sam_ret_t *ret = malloc (sizeof (sam_ret_t));
    ret->rc = -1;
    ret->msg = msg;
    ret->allocated = false;
    return ret;
}



//  --------------------------------------------------------------------------
/// Handle external publishing/rpc requests. Checks the protocol
/// number to decide if libsam can handle it and then either delegates
/// or rejects the message.
static int
handle_req (
    zloop_t *loop UU,
    zsock_t *client_rep,
    void *args)
{
    samd_t *self = args;
    sam_ret_t *ret;

    zmsg_t *zmsg = zmsg_new ();
    int version = -1;

    zsock_recv (client_rep, "im", &version, &zmsg);
    sam_stat (self->stat, "samd.accepted requests", 1);

    if (version == -1) {
        ret = create_error ("malformed request");
        zmsg_destroy (&zmsg);
    }

    else if (version != SAM_PROTOCOL_VERSION) {
        ret = create_error ("wrong protocol version");
        zmsg_destroy (&zmsg);
    }

    else if (zmsg_size (zmsg) < 1) {
        ret = create_error ("no payload");
        zmsg_destroy (&zmsg);
    }

    else {
        sam_stat (self->stat, "samd.valid requests", 1);

        sam_msg_t *msg = sam_msg_new (&zmsg);
        ret = sam_eval (self->sam, msg);
    }

    sam_log_tracef ("sending reply to client (%d)", ret->rc);

    int rc = (ret->rc == SAM_RET_RESTART)? 0: ret->rc;
    zsock_send (client_rep, "is", rc, ret->msg);

    if (ret->allocated) {
        free (ret->msg);
    }

    rc = (ret->rc == SAM_RET_RESTART)? -1: 0;
    free (ret);
    return rc;
}


//  --------------------------------------------------------------------------
/// Destroys the samd instance and free's all allocated memory.
void
samd_destroy (
    samd_t **self)
{
    sam_log_info ("destroying samd");

    zsock_destroy (&(*self)->client_rep);
    sam_destroy (&(*self)->sam);
    sam_stat_handle_destroy (&(*self)->stat);

    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
/// Creates a new samd instance and binds the public endpoints socket.
samd_t *
samd_new (
    const char *cfg_file)
{
    samd_t *self = malloc (sizeof (samd_t));
    assert (self);

    self->stat = sam_stat_handle_new ();

    sam_cfg_t *cfg = sam_cfg_new (cfg_file);
    if (!cfg) {
        goto abort;
    }

    char *endpoint;
    int rc = sam_cfg_endpoint (cfg, &endpoint);
    if (rc) {
        goto abort;
    }

    sam_be_t be_type;
    rc = sam_cfg_be_type (cfg, &be_type);
    self->sam = sam_new (be_type);
    assert (self->sam);

    self->client_rep = zsock_new_rep (endpoint);
    if (!self->client_rep) {
        sam_log_errorf ("could not bind endpoint '%s'", endpoint);
        goto abort;
    }


    sam_log_tracef ("bound public endpoint '%s'", endpoint);

    rc = sam_init (self->sam, &cfg);
    if (rc) {
        goto abort;
    }

    sam_log_info ("created samd");
    return self;


abort:
    samd_destroy (&self);

    if (cfg) {
        sam_cfg_destroy (&cfg);
    }

    return NULL;
}


//  --------------------------------------------------------------------------
/// Start a blocking loop for client requests.
int
samd_start (
    samd_t *self)
{
    zloop_t *loop = zloop_new ();
    zloop_reader (loop, self->client_rep, handle_req, self);
    int rc = zloop_start (loop);
    zloop_destroy (&loop);
    sam_log_info ("leaving main loop");
    return rc;
}


#ifndef __SAM_TEST
//  --------------------------------------------------------------------------
/// Main entry point, initializes and starts samd.
int
main (
    int argc,
    char **argv)
{
    argc -= 1;
    argv += 1;

    if (!argc || argc > 1) {
        printf ("samd - samwise messaging daemon\n");
        printf ("usage: samd path/to/config.cfg\n\n");
        return 2;
    }

    samd_t *samd = NULL;

    do {
        if (samd != NULL) {
            sam_log_info ("destroying former samd instance, restarting");
            samd_destroy (&samd);
        }

        samd = samd_new (*argv);
        if (!samd) {
            return 2;
        }

    } while (samd_start (samd) == -1);
    samd_destroy (&samd);

    sam_log_info ("exiting");
}
#endif
