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


/// State object maintained by the actor
typedef struct state_t {
    DB *dbp;                ///< database pointer

    zsock_t *in;            ///< for arriving acknowledgements
    zsock_t *out;           ///< for re-publishing
    zsock_t *store_sock;    ///< for (internal) storage requests
} state_t;


/// Meta information stored beside every encoded message
typedef struct record_header_t {
    uint64_t be_acks;     ///< mask containing backend ids
    int acks_remaining;   ///< number of remaining acks, can be
                          ///  negative for early arriving
                          ///  acknowledgements
} record_header_t;


//  --------------------------------------------------------------------------
/// Returns a human readable representation for DB error codes.
static char *
db_failure_reason (int code)
{
    char *reason;

    if (code == DB_LOCK_DEADLOCK) {
        reason = "The operation was selected to resolve a deadlock";
    }
    else if (code == DB_SECONDARY_BAD) {
        reason = "A secondary index references a nonexistent primary key";
    }
    else if (code ==  DB_NOTFOUND) {
        reason = "Key was not found";
    }
    else if (code == EACCES) {
        reason = "An attempt was made to modify a read-only database";
    }
    else if (code == EINVAL) {
        reason = "EINVAL (could be various, consult db manual)";
    }
    else if (code == ENOSPC) {
        reason = "BTREE max depth exceeded";
    }
    else {
        printf ("error code: %d\n", code);
        reason = "Unknown";
    }

    return reason;
}


//  --------------------------------------------------------------------------
/// Create or update a database record.
static int
put (
    state_t *state,
    int size,
    byte **record,
    DBT *key,
    DBT *val)
{
    sam_log_tracef (
        "putting '%d' into the buffer (acks remaining: %d)",
        *(int *) key->data,
        ((record_header_t *) *record)->acks_remaining);

    val->size = size;
    val->data = *record;

    int rc = state->dbp->put (
        state->dbp, NULL, key, val, 0);

    free (*record);
    *record = NULL;

    if (rc) {
        sam_log_errorf (
            "could not put '%d' in db: %s",
            *(int *) key->data,
            db_failure_reason (rc));
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// Delete a database record.
static int
del (
    state_t *state,
    DBT *key)
{
    sam_log_tracef (
        "deleting '%d' from the buffer",
        *(int *) key->data);

    int rc = state->dbp->del (state->dbp, NULL, key, 0);

    if (rc) {
        sam_log_errorf (
            "could not delete '%d' : %s",
            *(int *) key->data,
            db_failure_reason (rc));
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// Determines how much space is needed to store a record in a
/// continuous block of memory. If the sam_msg is null, just the space
/// for the header is considered.
static void
record_size (
    size_t *total_size,
    size_t *header_size,
    sam_msg_t *msg)
{
    *header_size = sizeof (record_header_t);

    if (msg) {
        *total_size = *header_size + sam_msg_encoded_size (msg);
    }
    else {
        *total_size = *header_size;
    }

    sam_log_tracef (
        "determined record size: %d (header: %d)",
        *total_size, *header_size);
}


//  --------------------------------------------------------------------------
/// Analyzes the publishing request and returns the number of
/// acknowledgements needed to consider the distribution guarantee
/// fulfilled.
static int
acks_remaining (sam_msg_t *msg)
{
    char *distribution;
    int rc = sam_msg_get (msg, "s", &distribution);
    assert (!rc);

    if (!strcmp (distribution, "round robin")) {
        rc = 1;
    }
    else if (!strcmp (distribution, "redundant")) {
        int ack_c = -1;
        sam_msg_get (msg, "?i", &ack_c);
        rc = ack_c;
    }
    else {
        assert (false);
    }

    free (distribution);
    return rc;
}


//  --------------------------------------------------------------------------
/// Create a new DBT key based on a message id.
static void
create_key (int *msg_id, DBT *thang)
{
    memset (thang, 0, sizeof (*thang));
    thang->size = sizeof (*msg_id);
    thang->data = msg_id;
}


//  --------------------------------------------------------------------------
/// Create a fresh database record based on a sam_msg enclosed
/// publishing request.
static int
create_record_store (state_t *state, DBT *key, sam_msg_t *msg)
{
    DBT val;
    size_t total_size, header_size;
    byte *record;

    memset (&val, 0, sizeof (val));
    record_size (&total_size, &header_size, msg);

    record = malloc (total_size);
    if (!record) {
        return -1;
    }

    // set header data
    record_header_t *header = (record_header_t *) record;
    header->acks_remaining = acks_remaining (msg);
    header->be_acks = 0;

    // set content data
    byte *content = record + header_size;
    sam_msg_encode (msg, &content);

    return put (state, total_size, &record, key, &val);
}


//  --------------------------------------------------------------------------
/// If an acknowledgement already created a database record, this
/// function is used to either delete the record if only one
/// acknowledgment suffices or update the meta information and append
/// the encoded messages.
static int
update_record_store (
    state_t *state,
    DBT *key,
    DBT *val,
    sam_msg_t *msg)
{
    int rc = 0;
    size_t total_size, header_size;
    record_size (&total_size, &header_size, msg);

    record_header_t *header = (record_header_t *) val->data;
    sam_log_tracef (
        "ack already there, %d arrived already",
        header->acks_remaining * -1);
    header->acks_remaining += acks_remaining (msg);


    // remove if there are no outstanding acks
    if (!header->acks_remaining) {
        rc = del (state, key);
    }

    // add encoded message to the record
    else {
        byte *record = malloc (total_size);
        if (!record) {
            return -1;
        }

        // copy header
        byte *header_cpy = memmove (record, val->data, header_size);
        if (header_cpy - record) {
            free (record);
            sam_log_error ("error while copying header data");
            return -1;
        }

        // encode message
        byte *content = record + header_size;
        sam_msg_encode (msg, &content);

        rc = put (state, total_size, &record, key, val);
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// Create a new database record just containing meta information and
/// no payload.
static int
create_record_ack (
    state_t *state,
    DBT *key,
    DBT *val,
    uint64_t backend_id)
{
    size_t record_size = sizeof (record_header_t);

    // only the header is needed
    byte *record = malloc (record_size);
    if (!record) {
        return -1;
    }

    record_header_t *header = (record_header_t *) record;
    header->acks_remaining = -1;
    header->be_acks = backend_id;

    return put (state, record_size, &record, key, val);
}


//  --------------------------------------------------------------------------
/// This function updates a record with the newly arrived
/// acknowledgment. It gets ignored when the origin was a backend that
/// already sent an acknowledgment (a highly unlikely case). If not,
/// the backend gets added to the list of backends that already
/// acknowledged the message. If enough backends confirmed, then the
/// record gets deleted. Otherwise it's updated.
static int
update_record_ack (
    state_t *state,
    DBT *key,
    DBT *val,
    uint64_t backend_id)
{
    int rc = 0;

    record_header_t *header = (record_header_t *) val->data;

    // if ack arrives multiple times, do nothing
    // -> redundancy guarantee applies only to distinct acks
    if (header->be_acks & backend_id) {
        sam_log_trace ("backend already confirmed, ignoring ack");
        return rc;
    }

    header->be_acks ^= backend_id;
    header->acks_remaining -= 1;

    // enough acks arrived, delete record
    if (!header->acks_remaining) {
        rc = del (state, key);
    }

    // not enough acks, update record
    else {
        sam_log_tracef (
            "updating '%d', acks remaining: %d",
            *(int *) key->data,
            header->acks_remaining);

        rc = state->dbp->put (
            state->dbp, NULL, key, val, 0);

        if (rc) {
            sam_log_errorf (
                "could not put '%d' in db: %s",
                *(int *) key->data,
                db_failure_reason (rc));
        }
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// Handles an acknowledgement. If there's already a record in the
/// database, the record is updated or deleted based on the
/// "remaining_acks" header field.
///
/// @see update_record_ack
///
/// If there's no record in the database, then the ack arrived before
/// the save () finished. In this case a new record is created just
/// containing meta information.
///
/// @see create_record_ack
///
static int
ack (state_t *state, uint64_t backend_id, int msg_id)
{
    DBT key, val;

    create_key (&msg_id, &key);
    memset (&val, 0, sizeof (val));

    int rc = state->dbp->get (
        state->dbp, NULL, &key, &val, 0);

    // record already there, update data
    if (rc == 0) {
        rc = update_record_ack (state, &key, &val, backend_id);
    }

    // record not yet there, create db entry
    else if (rc == DB_NOTFOUND) {
        rc = create_record_ack (state, &key, &val, backend_id);
    }

    else {
        sam_log_errorf (
            "could not retrieve '%d': %s",
            msg_id, db_failure_reason (rc));
    }

    return rc;
}



//  --------------------------------------------------------------------------
/// Handles a request sent internally to save the message to the store.
static int
handle_storage_req (zloop_t *loop UU, zsock_t *store_sock, void *args)
{
    state_t *state = args;

    int msg_id = -1;
    sam_msg_t *msg;
    sam_log_trace ("recv () storage request");
    zsock_recv (store_sock, "ip", &msg_id, &msg);
    assert (msg_id >= 0);

    sam_log_tracef ("handling storage request for '%d'", msg_id);

    DBT key, val;
    memset (&val, 0, sizeof (DBT));

    create_key (&msg_id, &key);
    int rc = state->dbp->get (
        state->dbp, NULL, &key, &val, 0);


    // record already there, update data
    if (rc == 0) {
        assert (val.size == sizeof (record_header_t));
        update_record_store (state, &key, &val, msg);
    }


    // record not yet there, create db entry
    else if (rc == DB_NOTFOUND) {
        rc = create_record_store (state, &key, msg);
    }

    else {
        sam_log_errorf (
            "could not retrieve '%d': %s",
            msg_id, db_failure_reason (rc));
    }


    sam_msg_destroy (&msg);
    return rc;
}


//  --------------------------------------------------------------------------
/// Demultiplexes acknowledgements arriving on the push/pull
/// connection wiring the messaging backends to the buffer.
///
/// @see ack
static int
handle_backend_req (zloop_t *loop UU, zsock_t *pll, void *args)
{
    state_t *state = args;
    int rc = 0;

    zframe_t *id_frame;
    uint64_t be_id = 0;
    sam_res_t res_t;
    int msg_id = -1;

    zsock_recv (pll, "fii", &id_frame, &res_t, &msg_id);
    be_id = *(uint64_t *) zframe_data (id_frame);

    assert (id_frame);
    assert (be_id > 0);
    assert (msg_id >= 0);

    zframe_destroy (&id_frame);

    if (res_t == SAM_RES_ACK) {
        sam_log_tracef (
            "ack from '%" PRIu64 "' for msg: '%d'",
            be_id, msg_id);

        rc = ack (state, be_id, msg_id);
    }
    else {
        sam_log_errorf ("got non-ack answer: %d", res_t);
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// The internally started actor. Listens to storage requests and
/// acknowledgments arriving from the backends.
static void
actor (zsock_t *pipe, void *args)
{
    sam_log_info ("starting actor");
    state_t *state = args;
    zloop_t *loop = zloop_new ();

    zloop_reader (loop, state->store_sock, handle_storage_req, state);
    zloop_reader (loop, state->in, handle_backend_req, state);
    zloop_reader (loop, pipe, sam_gen_handle_pipe, NULL);

    sam_log_info ("starting poll loop");
    zsock_signal (pipe, 0);
    zloop_start (loop);

    sam_log_trace ("destroying loop");
    zloop_destroy (&loop);


    // clean up
    zsock_destroy (&state->in);
    zsock_destroy (&state->out);
    zsock_destroy (&state->store_sock);

    // database
    state->dbp->close (state->dbp, 0);
    free (state);
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
    assert (*in);
    assert (*out);

    char *actor_endpoint = "inproc://sam_buf";
    sam_buf_t *self = malloc (sizeof (sam_buf_t));
    state_t *state = malloc (sizeof (state_t));

    assert (self);
    assert (state);


    // create and load database
    int rc = db_create (&state->dbp, NULL, 0);
    if (rc) {
        sam_log_error ("could not create database");
        free (self);
        return NULL;
    }

    rc = state->dbp->open (
        state->dbp,
        NULL,             // transaction pointer
        fname,            // on disk file
        NULL,             // logical db name
        DB_BTREE,         // access method
        DB_CREATE,        // open flags
        0);               // file mode

    if (rc) {
        sam_log_error ("could not open database");
        state->dbp->close (state->dbp, 0);
        free (self);
        free (state);
        return NULL;
    }

    state->dbp->set_errcall (state->dbp, db_error_handler);


    // set sockets, change ownership
    state->in = *in;
    *in = NULL;
    state->out = *out;
    *out = NULL;

    state->store_sock = zsock_new_pull (actor_endpoint);
    assert (state->store_sock);

    // intialize self
    self->seq = 0;
    self->actor = zactor_new (actor, state);
    self->store_sock = zsock_new_push (actor_endpoint);
    assert (self->store_sock);

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

    zsock_destroy (&(*self)->store_sock);
    zactor_destroy (&(*self)->actor);

    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
/// Save a message, get a message id as the receipt.
int
sam_buf_save (sam_buf_t *self, sam_msg_t *msg)
{
    assert (self);

    self->seq += 1;
    zsock_send (self->store_sock, "ip", self->seq, msg);
    return self->seq;
}
