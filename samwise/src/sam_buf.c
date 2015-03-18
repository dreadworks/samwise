/*  =========================================================================

    sam_buf - Persists messages

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief Persists messages
   @file sam_buf.c

   TODO description

*/


#include "../include/sam_prelude.h"


//  --------------------------------------------------------------------------
/// Demultiplexes acknowledgements arriving on the push/pull
/// connection wiring the backends to sam.
///
/// TO BE REMOVED - belongs to sam_buf in the future
static int
handle_backend_req (zloop_t *loop UU, zsock_t *pll, void *args UU)
{
    char *be_name;
    sam_res_t res_t;
    int msg_id;

    zsock_recv (pll, "sii", &be_name, &res_t, &msg_id);

    if (res_t == SAM_RES_ACK) {
        sam_log_tracef ("ack from '%s' for msg: %d", be_name, msg_id);
    }
    else {
        sam_log_infof ("got non-ack answer: %d", res_t);
    }

    free (be_name);
    return 0;
}



static void
actor (zsock_t *pipe, void *args)
{
    sam_log_info ("starting actor");
    sam_buf_t *self = args;
    zloop_t *loop = zloop_new ();

    zloop_reader (loop, self->in, handle_backend_req, self);
    zloop_reader (loop, pipe, sam_gen_handle_pipe, NULL);

    sam_log_info ("starting poll loop");
    zsock_signal (pipe, 0);
    zloop_start (loop);

    sam_log_trace ("destroying loop");
    zloop_destroy (&loop);
}


//  --------------------------------------------------------------------------
/// Generic error handler invoked by DB.
static void
db_error_handler (
    const DB_ENV *db_env UU,
    const char *err_prefix UU,
    const char *msg)
{
    sam_log_errorf ("db error: %s", msg);
}



//  --------------------------------------------------------------------------
/// Create a sam buf instance.
sam_buf_t *
sam_buf_new (const char *fname, zsock_t **in, zsock_t **out)
{
    assert (fname);
    assert (in);
    assert (out);

    sam_buf_t *self = malloc (sizeof (sam_buf_t));
    assert (self);


    // create and load database
    int rc = db_create (&self->dbp, NULL, 0);
    if (rc) {
        sam_log_error ("could not create database");
        free (self);
        return NULL;
    }

    rc = self->dbp->open (
        self->dbp,
        NULL,             // transaction pointer
        fname,            // on disk file
        NULL,             // logical db name
        DB_BTREE,         // access method
        DB_CREATE,        // open flags
        0);               // file mode

    if (rc) {
        sam_log_error ("could not open database");
        self->dbp->close (self->dbp, 0);
        free (self);
        return NULL;
    }

    self->dbp->set_errcall (self->dbp, db_error_handler);


    // initialize sequence number
    self->seq = 0;


    // set sockets, change ownership
    self->in = *in;
    *in = NULL;

    self->out = *out;
    *out = NULL;


    // hand everything over to the actor
    self->actor = zactor_new (actor, self);

    sam_log_info ("created buffer instance");
    return self;
}


//  --------------------------------------------------------------------------
/// Destroy a sam buf instance.
void
sam_buf_destroy (sam_buf_t **self)
{
    assert (*self);
    sam_log_info ("destroying buffer instance");
    zactor_destroy (&(*self)->actor);

    // database
    (*self)->dbp->close ((*self)->dbp, 0);


    // sockets
    zsock_destroy (&(*self)->in);
    zsock_destroy (&(*self)->out);

    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
/// Save a message, get a key as the receipt.
int
sam_buf_save (sam_buf_t *self, sam_msg_t *msg UU)
{
    self->seq += 1;
    return self->seq;
}
