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

   The samwise buffer encapsulates all persistence layer related
   operations. Underneath, a Berkeley DB storage engine is used to
   write message temporarily to a database file. The buffer accepts
   storage requests to persist a message, demultiplexes
   acknowledgements arriving from one or more messaging backends and
   resends messages based on the configuration file.

   <code>

   sam_buf | sam_buf actor
   -----------------------
     PIPE: sam_buf spawns its actor internally
     REQ/REP: storage requests

   sam_buf_actor | libsam actor
   ----------------------------
     PSH/PLL: resending publishing requests

   be[i] | sam_buf actor
   ---------------------
     PSH/PLL: acknowledgements


   Topology:
   --------

     libsam         sam_buf
      actor o        |    o
        PULL ^       |    ^ REQ
              \    PIPE   |
         PUSH  \     |    v REP
                o    |    o
               sam_buf actor o <-------- o be[i]
                     |      PULL       PUSH
                     |
               -------------
              | Berkeley DB |
              |  *.db file  |
               -------------


   </code>


*/


#include "../include/sam_prelude.h"


#define DBT_SIZE sizeof (DBT)


/// State object maintained by the actor
typedef struct state_t {
    int seq;                ///< used to assign unique message id's
    int last_stored;        ///< used to decide if it's a premature ack

    /// all database related stuff
    struct db {
        DB_ENV *e;           ///< database environment
        DB *p;               ///< database pointer
    } db;

    zsock_t *in;            ///< for arriving acknowledgements
    zsock_t *out;           ///< for re-publishing
    zsock_t *store_sock;    ///< for (internal) storage requests

    int tries;              ///< maximum number of retries for a message
    uint64_t interval;      ///< how often messages are being tried again
    uint64_t threshold;     ///< at which point messages are tried again
} state_t;


/// used to define how to read record_header_t.content
typedef enum {
    RECORD = 0x10,
    RECORD_TOMBSTONE
} record_type_t;


/// Meta information stored for every record
typedef struct record_t {
    record_type_t type;   ///< either record or tombstone
    union {

        /// header stored for messages (encoded message gets appended)
        struct {
            int prev;             ///< previous tombstone

            uint64_t be_acks;     ///< mask containing backend ids
            int acks_remaining;   ///< may be negative for early acks
            int64_t ts;           ///< insertion time
            int tries;            ///< total number of retries
        } record;                 ///< if type == RECORD


        /// data stored for tombstones
        struct {
            int prev;    ///< previous tombstone or NULL
            int next;    ///< next record/tombstone
        } tombstone;

    } c;
} record_t;



//  --------------------------------------------------------------------------
/// Create a unique, sortable message id. Order determines message
/// age. Older keys must have smaller keys.
static int
create_msg_id (
    state_t *state)
{
    return state->seq += 1;
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
/// Returns a human readable representation for DB error codes.
static char *
db_failure_reason (
    int code)
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
        reason = "Unknown";
    }

    return reason;
}


//  --------------------------------------------------------------------------
/// Close (partially) initialized database and environment.
static void
close_db (
    state_t *state)
{
    int rc = 0;

    if (state->db.p) {
        rc = state->db.p->close (state->db.p, 0);
        if (rc) {
            sam_log_errorf (
                "could not safely close db: %s",
                db_strerror (rc));
        }
    }

    if (state->db.e) {
        rc = state->db.e->close (state->db.e, 0);
        if (rc) {
            sam_log_errorf (
                "could not safely close db environment: %s",
                db_strerror (rc));
        }
    }
}


//  --------------------------------------------------------------------------
/// Create a database environment and a database handle.
static int
create_db (
    state_t *state,
    const char *db_home_name,
    const char *db_file_name)
{

    // initialize the environment
    uint32_t env_flags =
        DB_CREATE      |    // create environment if it's not there
        DB_INIT_TXN    |    // initialize transactions
        DB_INIT_LOCK   |    // locking (is this needed for st?)
        DB_INIT_LOG    |    // for recovery
        DB_INIT_MPOOL;      // in-memory cache

    int rc = db_env_create (&state->db.e, 0);
    if (rc) {
        sam_log_errorf (
            "could not create db environment: %s",
            db_strerror (rc));
        return rc;
    }

    rc = state->db.e->open (state->db.e, db_home_name, env_flags, 0);
    if (rc) {
        sam_log_errorf (
            "could not open db environment: %s",
            db_strerror (rc));
        close_db (state);
        return rc;
    }


    // open the database
    uint32_t db_flags =
        DB_CREATE |       // create db if it's not there
        DB_AUTO_COMMIT;   // make all ops. transactional

    rc = db_create (&state->db.p, state->db.e, 0);
    if (rc) {
        state->db.e->err (state->db.e, rc, "database creation failed");
        close_db (state);
        return rc;
    }

   rc = state->db.p->open (
        state->db.p,
        NULL,             // transaction pointer
        db_file_name,     // on disk file
        NULL,             // logical db name
        DB_BTREE,         // access method
        db_flags,         // open flags
        0);               // file mode

    if (rc) {
        state->db.e->err (state->db.e, rc, "database open failed");
        state->db.p->close (state->db.p, 0);
        return rc;
    }

    state->db.p->set_errcall (state->db.p, db_error_handler);
    return rc;
}




//  --------------------------------------------------------------------------
/// Create a transaction handle.
static DB_TXN *
create_txn (
    state_t *state)
{
    DB_TXN *txn = NULL;
    int rc = state->db.e->txn_begin (state->db.e, NULL, &txn, 0);

    if (rc) {
        state->db.e->err (state->db.e, rc, "transaction begin failed");
    }

    return txn;
}


//  --------------------------------------------------------------------------
/// Either abort or commit a transaction based on rc.
static void
end_txn (
    state_t *state,
    int rc,
    DB_TXN *txn)
{
    if (rc) {
        sam_log_error ("aborting transaction");
        txn->abort (txn);
    }
    else if (txn != NULL) {
        if (txn->commit (txn, 0)) {
            state->db.e->err (state->db.e, rc, "transaction failed");
            // TODO do I need to return with some error indication here?
        }
    }
}


//  --------------------------------------------------------------------------
/// Create a new DBT key based on a message id.
static void
create_key (
    int *msg_id,
    DBT *thang)
{
    memset (thang, 0, DBT_SIZE);
    thang->size = sizeof (*msg_id);
    thang->data = msg_id;
}


static int
insert_tombstone (
    state_t *state,
    DBC *cursor,
    int prev,
    int next)
{
    size_t size = sizeof (record_t);

    record_t tombstone;
    memset (&tombstone, 0, size);

    sam_log_tracef ("creating tombstone: %d | %d", prev, next);

    tombstone.type = RECORD_TOMBSTONE;
    tombstone.c.tombstone.prev = prev;
    tombstone.c.tombstone.next = next;

    DBT val;
    memset (&val, 0, DBT_SIZE);
    val.size = size;
    val.data = &tombstone;

    int rc = cursor->put (cursor, NULL, &val, DB_CURRENT);
    if (rc) {
        state->db.e->err (state->db.e, rc, "could not create tombstone");
        return -1;
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Create or update a database record.
static int
put_record (
    state_t *state,
    int size,
    byte **record,
    DBT *key,
    DBT *val,
    DB_TXN *txn)
{
    assert (state);
    assert (*record);
    assert (((record_t *) *record)->type == RECORD);

    sam_log_tracef (
        "putting '%d' into the buffer (acks remaining: %d)",
        *(int *) key->data,
        ((record_t *) *record)->c.record.acks_remaining);

    val->size = size;
    val->data = *record;

    int rc = state->db.p->put (
        state->db.p, txn, key, val, 0);

    free (*record);
    *record = NULL;

    if (rc) {
        state->db.e->err (state->db.e, rc, "could not put");
    }

    return rc;
}


static int
update_record (
    state_t *state,
    DBT *key,
    DBT *val,
    DB_TXN *txn)
{
    int rc = state->db.p->put (state->db.p, txn, key, val, 0);

    if (rc) {
        state->db.e->err (state->db.e, rc, "could not update record");
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// Updates an already existing record.
static int
update_existing_record (
    state_t *state,
    DBT *val,
    DB_TXN *txn,
    int new_id,
    int cur_id)
{
    DBT key;
    create_key (&new_id, &key);

    record_t *header = val->data;

    header->c.record.ts = zclock_mono ();
    header->c.record.prev = cur_id;

    int rc = state->db.p->put (state->db.p, txn, &key, val, 0);

    if (rc) {
        state->db.e->err (state->db.e, rc, "could not update record");
    }

    return rc;
}


static int
update_record_tries (DBT *val)
{
    int rc = 0;
    record_t *header = val->data;

    header->c.record.tries -= 1;
    if (!header->c.record.tries) {
        rc = -1; // to indicate skipping
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// Delete a database record.
static int
del (
    state_t *state,
    DBT *key,
    DBT *val,
    DB_TXN *txn)
{
    int rc = 0,
        curr_key = 0,
        prev_key = 0;

    do {
        // determine previous tombstone - if any
        record_t *header = (record_t *) val->data;
        if (header->type == RECORD) {
            prev_key = header->c.record.prev;
        }
        else if (header->type == RECORD_TOMBSTONE) {
            prev_key = header->c.tombstone.prev;
        }


        // delete current record
        sam_log_tracef (
            "deleting '%d' (prev: '%d') from the buffer",
            *(int *) key->data,
            prev_key);

        rc = state->db.p->del (state->db.p, txn, key, 0);

        if (rc) {
            state->db.e->err (state->db.e, rc, "could not delete record");
            return -1;
        }

        // obtain previous tombstone
        if (prev_key) {
            curr_key = prev_key;
            create_key (&curr_key, key);

            rc = state->db.p->get (state->db.p, txn, key, val, 0);
            if (rc) {
                state->db.e->err (
                    state->db.e, rc, "could not get previous tombstone");
            }
        }

    } while (prev_key);

    return rc;
}


//  --------------------------------------------------------------------------
/// Returns the next item in the database.
static int
get_next (
    state_t *state,
    DBC *cursor,
    DBT *key,
    DBT *val)
{
    memset (key, 0, DBT_SIZE);
    memset (val, 0, DBT_SIZE);

    int rc = cursor->get(cursor, key, val, DB_NEXT);
    if (rc && rc != DB_NOTFOUND) {
        state->db.e->err (state->db.e, rc, "could not get next item");
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// Checks if the record is inside the bounds of messages to be resent.
static int
resend_condition (
    state_t *state,
    DBT *val)
{
    record_t *header = val->data;
    if (header->type == RECORD) {
        uint64_t eps = zclock_mono () - header->c.record.ts;
        return (state->threshold < eps)? 0: -1;
    }

    return 0;
}


static int
resend_message (
    state_t *state,
    DBT *val,
    int msg_id)
{
    record_t *header = val->data;
    size_t header_size = sizeof (record_t);

    // decode message
    size_t msg_size = val->size - header_size;
    byte *encoded_msg = ((byte *) val->data) + header_size;
    sam_msg_t *msg = sam_msg_decode (encoded_msg, msg_size);
    if (msg == NULL) {
        sam_log_error ("could not decode stored message");
        return -1;
    }

    // wrap backend acknowledgments
    uint64_t be_acks = header->c.record.be_acks;
    zframe_t *id_frame = zframe_new (&be_acks, sizeof (be_acks));

    sam_log_tracef ("resending msg '%d'", msg_id);
    zsock_send (state->out, "ifp", msg_id, id_frame, msg);

    zframe_destroy (&id_frame);
    return 0;
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
    *header_size = sizeof (record_t);

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
acks_remaining (
    sam_msg_t *msg)
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
/// Create a fresh database record based on a sam_msg enclosed
/// publishing request.
static int
create_record_store (
    state_t *state,
    DBT *key,
    DB_TXN *txn,
    sam_msg_t *msg)
{
    DBT val;
    size_t total_size, header_size;
    byte *record;

    memset (&val, 0, DBT_SIZE);
    record_size (&total_size, &header_size, msg);

    record = malloc (total_size);
    if (!record) {
        return -1;
    }

    sam_log_tracef (
        "creating record for msg '%d' (%p)",
        *(int *) key->data,
        (void *) record);

    // set header data
    record_t *header = (record_t *) record;
    header->type = RECORD;
    header->c.record.prev = 0;

    header->c.record.acks_remaining = acks_remaining (msg);
    header->c.record.be_acks = 0;
    header->c.record.ts = zclock_mono ();
    header->c.record.tries = state->tries;

    // set content data
    byte *content = record + header_size;
    sam_msg_encode (msg, &content);

    state->last_stored += 1;
    return put_record (state, total_size, &record, key, &val, txn);
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
    DB_TXN *txn,
    sam_msg_t *msg)
{
    int rc = 0;
    size_t total_size, header_size;
    record_size (&total_size, &header_size, msg);

    record_t *header = (record_t *) val->data;
    assert (header->type == RECORD);

    sam_log_tracef (
        "ack already there, %d arrived already",
        header->c.record.acks_remaining * -1);
    header->c.record.acks_remaining += acks_remaining (msg);

    // remove if there are no outstanding acks
    if (!header->c.record.acks_remaining) {
        rc = del (state, key, val, txn);
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

        rc = put_record (state, total_size, &record, key, val, txn);
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
    DB_TXN *txn,
    uint64_t backend_id)
{
    size_t record_size = sizeof (record_t);

    // only the header is needed
    byte *record = malloc (record_size);
    if (!record) {
        sam_log_error ("could not create record");
        return -1;
    }

    sam_log_tracef (
        "creating premature ack record for '%d'", *(int *) key->data);

    record_t *header = (record_t *) record;
    header->type = RECORD;
    header->c.record.acks_remaining = -1;
    header->c.record.be_acks = backend_id;

    return put_record (state, record_size, &record, key, val, txn);
}


//  --------------------------------------------------------------------------
/// This function updates a record with the newly arrived
/// acknowledgment. It gets ignored when the origin was a backend that
/// already sent an acknowledgment (a highly unlikely case). If not,
/// the backend gets added to the list of backends that already
/// acknowledged the message. If enough backends confirmed, then the
/// record and all its tombstones gets deleted. Otherwise it's updated.
static int
update_record_ack (
    state_t *state,
    DBT *key,
    DBT *val,
    DB_TXN *txn,
    uint64_t backend_id)
{
    int rc = 0;

    // skip all tombstones up to the record
    record_t *header = (record_t *) val->data;
    while (header->type == RECORD_TOMBSTONE) {
        int new_key = header->c.tombstone.next;
        sam_log_tracef ("following tombstone chain to '%d'", new_key);

        create_key (&new_key, key);
        rc = state->db.p->get (state->db.p, txn, key, val, 0);
        if (rc) {
            state->db.e->err (state->db.e, rc, "could not follow tombstone");
            return -1;
        }

        header = val->data;
    }

    assert (header->type == RECORD);


    // if ack arrives multiple times, do nothing
    // -> redundancy guarantee applies only to distinct acks
    if (header->c.record.be_acks & backend_id) {
        sam_log_trace ("backend already confirmed, ignoring ack");
        return rc;
    }

    header->c.record.be_acks ^= backend_id;
    header->c.record.acks_remaining -= 1;


    // enough acks arrived, delete record
    if (!header->c.record.acks_remaining) {
        rc = del (state, key, val, txn);
    }


    // not enough acks, update record
    else {
        sam_log_tracef (
            "updating '%d', acks remaining: %d",
            *(int *) key->data,
            header->c.record.acks_remaining);

        rc = update_record (state, key, val, txn);
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// Handles an acknowledgement. If there's already a record in the
/// database, the record is updated or deleted based on the
/// "remaining_acks" header field. If the encountered record is a tombstone,
/// it's reference is followed until the record is found.
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
handle_ack (
    state_t *state,
    uint64_t backend_id,
    int ack_id)
{
    DBT key, val;
    DB_TXN *txn = create_txn (state);

    create_key (&ack_id, &key);
    memset (&val, 0, DBT_SIZE);

    int rc = state->db.p->get (state->db.p, txn, &key, &val, 0);

    // record already there, update data
    if (rc == 0) {
        rc = update_record_ack (state, &key, &val, txn, backend_id);
    }

    // record not yet there, create db entry
    else if (rc == DB_NOTFOUND) {
        if (state->last_stored <= ack_id) {
            sam_log_tracef ("ignoring late ack '%d'", ack_id);
            rc = 0;
        } else {
            rc = create_record_ack (state, &key, &val, txn, backend_id);
        }
    }

    else {
        state->db.e->err (state->db.e, rc, "could not retrieve record");
        rc = -1;
    }

    end_txn (state, rc, txn);
    return rc;
}



//  --------------------------------------------------------------------------
/// Handles a request sent internally to save the message to the store.
static int
handle_storage_req (
    zloop_t *loop UU,
    zsock_t *store_sock,
    void *args)
{
    state_t *state = args;

    sam_msg_t *msg;
    sam_log_trace ("recv () storage request");
    zsock_recv (store_sock, "p", &msg);

    int msg_id = create_msg_id (state);
    assert (msg_id >= 0);
    sam_log_tracef ("handling storage request for '%d'", msg_id);

    // position of this call handles what guarantee
    // is promised to the publishing client. See #66
    zsock_send (store_sock, "i", msg_id);

    DBT key, val;
    memset (&val, 0, DBT_SIZE);


    // try to retrieve the record
    create_key (&msg_id, &key);
    DB_TXN *txn = create_txn (state);
    int rc = state->db.p->get (state->db.p, txn, &key, &val, 0);


    // record already there, update data
    if (rc == 0) {
        assert (val.size == sizeof (record_t));
        rc = update_record_store (state, &key, &val, txn, msg);
    }


    // record not yet there, create db entry
    else if (rc == DB_NOTFOUND) {
        rc = create_record_store (state, &key, txn, msg);
    }

    else {
        sam_log_errorf (
            "could not retrieve '%d': %s",
            msg_id, db_failure_reason (rc));
        rc = -1;
    }

    end_txn (state, rc, txn);
    sam_msg_destroy (&msg);
    return rc;
}


//  --------------------------------------------------------------------------
/// Demultiplexes acknowledgements arriving on the push/pull
/// connection wiring the messaging backends to the buffer.
///
/// @see ack
static int
handle_backend_req (
    zloop_t *loop UU,
    zsock_t *pll,
    void *args)
{
    state_t *state = args;
    int rc = 0;

    zframe_t *id_frame;
    uint64_t be_id = 0;
    int msg_id = -1;

    zsock_recv (pll, "fi", &id_frame, &msg_id);
    be_id = *(uint64_t *) zframe_data (id_frame);

    assert (id_frame);
    assert (be_id > 0);
    assert (msg_id >= 0);

    zframe_destroy (&id_frame);

    sam_log_tracef (
        "ack from '%" PRIu64 "' for msg: '%d'",
        be_id, msg_id);

    rc = handle_ack (state, be_id, msg_id);
    return rc;
}



//  --------------------------------------------------------------------------
/// Checks in a fixed interval if messages need to be resent.
static int
handle_resend (
    zloop_t *loop UU,
    int timer_id UU,
    void *args)
{
    sam_log_trace ("resend cycle triggered");
    state_t *state = args;

    // create transaction handle
    DB_TXN *txn = create_txn (state);

    // create cursor
    DBC *cursor;
    state->db.p->cursor (state->db.p, txn, &cursor, 0);
    if (cursor == NULL) {
        sam_log_error ("could not initialize cursor");
        end_txn (state, -1, txn);
        return -1;
    }

    DBT key, val;
    int first_requeued_key = 0; // can never be zero
    int rc = get_next (state, cursor, &key, &val);

    while (
        !rc &&                                      // there's another item
        !resend_condition (state, &val) &&          // threshold/tombstone?
        first_requeued_key != *(int *) key.data) {  // don't send requeued

        int cur_id = *(int *) key.data;

        //
        // skip tombstones
        //
        record_t *header = val.data;
        if (header->type == RECORD_TOMBSTONE) {
            sam_log_tracef ("skipping tombstone '%d'", cur_id);
            rc = get_next (state, cursor, &key, &val);
            continue;
        }

        //
        //  decrement tries
        //
        //  TODO this can be refactored and completely wrapped in
        //  update_record_tries (). Requirement: Completely work with
        //  cursors, at least in del ().
        assert (header->type == RECORD);
        if (update_record_tries (&val)) {
            sam_log_infof ("discarding message '%d'", cur_id);
            rc = del (state, &key, &val, txn);
            if (rc) {
                state->db.e->err (
                    state->db.e, rc, "could not discard record");
            }

            rc = get_next (state, cursor, &key, &val);
            continue;
        }

        //
        //  update record, resend message, insert tombstone
        //
        int new_id = create_msg_id (state);
        if (!first_requeued_key) {
            first_requeued_key = new_id;
        }

        int prev_id = header->c.record.prev;
        if (
            update_existing_record (state, &val, txn, new_id, cur_id) ||
            resend_message (state, &val, cur_id) ||
            insert_tombstone (state, cursor, prev_id, new_id)) {

            rc = -1;
            break;
        }

        // overwrites key & val
        rc = get_next (state, cursor, &key, &val);
    }

    cursor->close (cursor);

    if (rc && rc != DB_NOTFOUND) {
        end_txn (state, rc, txn);
        return rc;
    }

    end_txn (state, 0, txn);
    return 0;
}


//  --------------------------------------------------------------------------
/// The internally started actor. Listens to storage requests and
/// acknowledgments arriving from the backends.
static void
actor (
    zsock_t *pipe,
    void *args)
{
    sam_log_info ("starting actor");

    state_t *state = args;
    zloop_t *loop = zloop_new ();

    zloop_reader (loop, state->store_sock, handle_storage_req, state);
    zloop_reader (loop, state->in, handle_backend_req, state);
    zloop_reader (loop, pipe, sam_gen_handle_pipe, NULL);

    // is a uint64_t -> size_t conversion okay?
    zloop_timer (loop, state->interval, 0, handle_resend, state);

    sam_log_info ("starting poll loop");
    zsock_signal (pipe, 0);
    zloop_start (loop);

    // tear down
    sam_log_trace ("destroying loop");
    zloop_destroy (&loop);

    // database
    close_db (state);

    // clean up
    zsock_destroy (&state->in);
    zsock_destroy (&state->out);
    zsock_destroy (&state->store_sock);

    free (state);
}


//  --------------------------------------------------------------------------
/// Create a sam buf instance.
sam_buf_t *
sam_buf_new (
    sam_cfg_t *cfg,
    zsock_t **in,
    zsock_t **out)
{
    assert (cfg);
    assert (*in);
    assert (*out);

    char *actor_endpoint = "inproc://sam_buf";
    sam_buf_t *self = malloc (sizeof (sam_buf_t));
    state_t *state = malloc (sizeof (state_t));

    assert (self);
    assert (state);

    // to check for null values
    state->db.p = NULL;
    state->db.e = NULL;

    // read config
    char
        *db_file_name,
        *db_home_name;

    if (
        sam_cfg_buf_home (cfg, &db_home_name) ||
        sam_cfg_buf_file (cfg, &db_file_name) ||
        sam_cfg_buf_retry_count (cfg, &state->tries) ||
        sam_cfg_buf_retry_interval (cfg, &state->interval) ||
        sam_cfg_buf_retry_threshold (cfg, &state->threshold)) {

        sam_log_error ("could not initialize the buffer");
        goto abort;
    }


    // init
    int rc = create_db (state, db_home_name, db_file_name);
    if (rc) {
        goto abort;
    }

    // set sockets, change ownership
    state->in = *in;
    *in = NULL;
    state->out = *out;
    *out = NULL;

    // storage
    state->store_sock = zsock_new_rep (actor_endpoint);
    self->store_sock = zsock_new_req (actor_endpoint);

    assert (state->store_sock);
    assert (self->store_sock);

    state->seq = 0;
    state->last_stored = 0;

    // spawn actor
    self->actor = zactor_new (actor, state);
    sam_log_info ("created buffer instance");

    return self;

abort:
    free (self);
    free (state);
    return NULL;
}


//  --------------------------------------------------------------------------
/// Destroy a sam buf instance.
void
sam_buf_destroy (
    sam_buf_t **self)
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
sam_buf_save (
    sam_buf_t *self,
    sam_msg_t *msg)
{
    assert (self);
    zsock_send (self->store_sock, "p", msg);

    int msg_id;
    zsock_recv (self->store_sock, "i", &msg_id);

    return msg_id;
}
