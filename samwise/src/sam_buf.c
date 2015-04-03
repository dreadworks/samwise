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


/*
 *    HANDLE DEFINITIONS
 */


/// State object maintained by the actor
typedef struct state_t {
    // data to be restored after restart
    int seq;                ///< used to assign unique message id's
    int last_stored;        ///< used to decide if it's a premature ack

    sam_db_t *db;           ///< storage engine

    zsock_t *in;            ///< for arriving acknowledgements
    zsock_t *out;           ///< for re-publishing
    zsock_t *store_sock;    ///< for (internal) storage requests

    int tries;              ///< maximum number of retries for a message
    uint64_t interval;      ///< how often messages are being tried again
    uint64_t threshold;     ///< at which point messages are tried again
} state_t;



/*
 *    RECORD DEFINITIONS
 */


/// used to define how to read record_header_t.content
typedef enum {
    RECORD = 0x10,    // arbitrary value, prevents false positives
    RECORD_ACK,
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




static void
stat_db_size (state_t *state)
{
    DB_BTREE_STAT *statp;
    state->db.p->stat (state->db.p, NULL, &statp, 0);
    sam_log_infof ("db contains %d record(s)", statp->bt_nkeys);
    free (statp);
}



/*
static void
PRINT_DB_INFO (state_t *state)
{
    stat_db_size (state);

    DBT key, data;
    memset (&key, 0, DBT_SIZE);
    memset (&data, 0, DBT_SIZE);

    printf ("records: \n");

    DBC *cursor;
    state->db.p->cursor (state->db.p, NULL, &cursor, 0);
    while (!cursor->get (cursor, &key, &data, DB_NEXT)) {
        printf ("%d - ", *(int *) key.data);
    }

    cursor->close (cursor);
    printf ("\n");
}
*/


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
/// Sets the database records key.
static void
set_key (
    dbop_t *op,
    int *key)
{
    memset (&op->key, 0, DBT_SIZE);
    op->key.size = sizeof (*key);
    op->key.data = key;
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
/// Close (partially) initialized database and environment.
static void
close_db (
    state_t *state)
{
    int rc = 0;

    if (state->db.p) {
        stat_db_size (state);
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
    uint32_t db_flags = DB_CREATE | DB_AUTO_COMMIT;

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


    stat_db_size (state);
    state->db.p->set_errcall (state->db.p, db_error_handler);
    return rc;
}


//  --------------------------------------------------------------------------
/// Update current database record.
static int
put (
    state_t *state,
    dbop_t *op,
    size_t size,
    byte **record,
    uint32_t flags)
{
    assert (state);
    assert (op);
    assert (*record);

    sam_log_tracef (
        "putting '%d' (size %d) into the buffer",
        *(int *) op->key.data, size);

    memset (&op->val, 0, DBT_SIZE);
    op->val.size = size;
    op->val.data = *record;

    int rc = op->cursor->put (op->cursor, &op->key, &op->val, flags);
    // int rc = state->db.p->put (state->db.p, op->txn, &op->key, &op->val, 0);

    free (*record);
    *record = NULL;

    if (rc) {
        state->db.e->err (state->db.e, rc, "could not put record");
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// Update a database record. If the flag is DB_CURRENT, op's key is
/// ignored and the cursors position is updated, if the flag is
/// DB_KEYFIRST, the key is used to determine where to put the record.
static int
update (
    state_t *state,
    dbop_t *op,
    uint32_t flags)
{
    assert (state);
    assert (op);
    assert (flags == DB_CURRENT || flags == DB_KEYFIRST);

    int rc = op->cursor->put (op->cursor, &op->key, &op->val, flags);
    if (rc) {
        state->db.e->err (state->db.e, rc, "could not update record");
    }
    return rc;
}


//  --------------------------------------------------------------------------
/// Delete the record the cursor currently points to.
static int
del_current (
    state_t *state,
    dbop_t *op)
{
    assert (state);
    assert (op);

    int rc = op->cursor->del (op->cursor, 0);
    if (rc) {
        state->db.e->err (state->db.e, rc, "could not delete record");
        return rc;
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Delete a database record and all its tombstones.
static int
del (
    state_t *state,
    dbop_t *op)
{
    assert (state);
    assert (op);

    int rc, curr_key, prev_key;
    rc = curr_key = prev_key = 0;

    do {
        del_current (state, op);

        // determine previous tombstone - if any
        record_t *header = (record_t *) op->val.data;
        if (header->type == RECORD) {
            prev_key = header->c.record.prev;
        }
        else if (header->type == RECORD_TOMBSTONE) {
            prev_key = header->c.tombstone.prev;
        }

        // prepare next round
        if (prev_key) {
            curr_key = prev_key;
            rc = get (state, op, &prev_key);

            if (rc) {
                // either DB_NOTFOUND or an error, abort
                // deletion regardless; errors are handled
                // one level up via rc
                prev_key = 0;
            }
        }

    } while (prev_key);

    return rc;
}


//  --------------------------------------------------------------------------
/// Creates a new tombstone record.
static int
insert_tombstone (
    state_t *state,
    dbop_t *op,
    int prev,
    int *position)
{
    size_t size = sizeof (record_t);
    int next = *(int *) op->key.data;

    record_t tombstone;
    memset (&tombstone, 0, size);

    sam_log_tracef ("creating tombstone: %d | %d", prev, *position, next);

    tombstone.type = RECORD_TOMBSTONE;
    tombstone.c.tombstone.prev = prev;
    tombstone.c.tombstone.next = next;

    // write new key
    set_key (op, position);

    // write new data
    memset (&op->val, 0, DBT_SIZE);
    op->val.size = size;
    op->val.data = &tombstone;

    return update (state, op, DB_KEYFIRST);
}


//  --------------------------------------------------------------------------
/// Decrements the try-counter and returns a non-zero value for
/// to-be-discarded messages.
static int
update_record_tries (
    state_t *state,
    dbop_t *op)
{
    record_t *header = op->val.data;

    header->c.record.tries -= 1;
    if (!header->c.record.tries) {
        sam_log_infof ("discarding message '%d'", *(int *) op->key.data);
        del (state, op);
        return -1;
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Checks if the record is inside the bounds of messages to be resent.
static int
resend_condition (
    state_t *state,
    dbop_t *op)
{
    record_t *header = op->val.data;
    if (header->type == RECORD) {
        uint64_t eps = zclock_mono () - header->c.record.ts;
        return (state->threshold < eps)? 0: -1;
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Takes a database record, reconstruct the message and sends a copy
/// it via output channel.
static int
resend_message (
    state_t *state,
    dbop_t *op)
{
    record_t *header = op->val.data;
    size_t header_size = sizeof (record_t);

    // decode message
    size_t msg_size = op->val.size - header_size;
    byte *encoded_msg = ((byte *) op->val.data) + header_size;
    sam_msg_t *msg = sam_msg_decode (encoded_msg, msg_size);
    if (msg == NULL) {
        sam_log_error ("could not decode stored message");
        return -1;
    }

    // wrap backend acknowledgments
    uint64_t be_acks = header->c.record.be_acks;
    zframe_t *id_frame = zframe_new (&be_acks, sizeof (be_acks));
    int msg_id = *(int *) op->key.data;

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
    dbop_t *op,
    sam_msg_t *msg)
{
    size_t size, header_size;
    record_size (&size, &header_size, msg);

    byte *record;
    record = malloc (size);
    if (!record) {
        return -1;
    }

    sam_log_tracef (
        "creating record for msg '%d'", *(int *) op->key.data);

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
    return put (state, op, size, &record, DB_KEYFIRST);
}


//  --------------------------------------------------------------------------
/// If an acknowledgement already created a database record, this
/// function is used to either delete the record if only one
/// acknowledgment suffices or update the meta information and append
/// the encoded messages.
static int
update_record_store (
    state_t *state,
    dbop_t *op,
    sam_msg_t *msg)
{
    int rc = 0;
    size_t total_size, header_size;
    record_size (&total_size, &header_size, msg);

    record_t *header = (record_t *) op->val.data;
    assert (header->type == RECORD);

    sam_log_tracef (
        "ack already there, %d arrived already",
        header->c.record.acks_remaining * -1);
    header->c.record.acks_remaining += acks_remaining (msg);

    // remove if there are no outstanding acks
    if (!header->c.record.acks_remaining) {
        rc = del (state, op);
    }

    // add encoded message to the record
    else {
        byte *record = malloc (total_size);
        if (!record) {
            return -1;
        }

        // copy header
        byte
            *header_cpy = memmove (record, op->val.data, header_size),
            *content = record + header_size;

        assert (header_cpy);
        sam_msg_encode (msg, &content);
        rc = put (state, op, total_size, &record, DB_CURRENT);
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// Create a new database record just containing meta information and
/// no payload.
static int
create_record_ack (
    state_t *state,
    dbop_t *op,
    uint64_t backend_id)
{
    size_t size = sizeof (record_t);

    // only the header is needed
    byte *record = malloc (size);
    if (!record) {
        sam_log_error ("could not create record");
        return -1;
    }

    record_t *header = (record_t *) record;
    header->type = RECORD_ACK;
    header->c.record.acks_remaining = -1;
    header->c.record.be_acks = backend_id;

    sam_log_tracef ("created record (ack) '%d'", *(int *) op->key.data);
    return put (state, op, size, &record, DB_KEYFIRST);
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
    dbop_t *op,
    uint64_t backend_id)
{
    int rc = 0;

    // skip all tombstones up to the record
    record_t *header = (record_t *) op->val.data;
    while (header->type == RECORD_TOMBSTONE) {

        int new_id = header->c.tombstone.next;
        sam_log_tracef ("following tombstone chain to '%d'", new_id);
        if (get (state, op, &new_id)) {
            return -1;
        }

        header = op->val.data;
    }

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
        rc = del (state, op);
    }


    // not enough acks, update record
    else {
        sam_log_tracef (
            "updating '%d', acks remaining: %d",
            *(int *) op->key.data,
            header->c.record.acks_remaining);

        header->type = RECORD;
        rc = update (state, op, DB_CURRENT);
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
    dbop_t op;
    if (start_dbop (state, &op)) {
        return -1;
    }

    int rc = get (state, &op, &ack_id);

    // record already there, update data
    if (rc == 0) {
        rc = update_record_ack (state, &op, backend_id);
    }

    // record not yet there, create db entry
    else if (rc == DB_NOTFOUND) {
        if (ack_id < state->last_stored) {
            sam_log_tracef ("ignoring late ack '%d'", ack_id);
            rc = 0;
        } else {
            rc = create_record_ack (state, &op, backend_id);
        }
    }

    else {
        rc = -1;
    }

    end_dbop (state, &op, (rc)? true: false);
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
    sam_db_t *db = state->db;

    sam_msg_t *msg;
    sam_log_trace ("recv () storage request");
    zsock_recv (store_sock, "p", &msg);

    int msg_id = create_msg_id (state);
    assert (msg_id >= 0);
    sam_log_tracef ("handling storage request for '%d'", msg_id);

    // position of this call handles what guarantee
    // is promised to the publishing client. See #66
    zsock_send (store_sock, "i", msg_id);

    if (sam_db_begin (db)) {
        return -1;
    }

    int rc = get (state, &op, &msg_id);

    // record already there (ack arrived early), update data
    if (rc == 0) {
        rc = update_record_store (state, &op, msg);
    }

    // record not yet there, create db entry
    else if (rc == DB_NOTFOUND) {
        // key was already set by get ()
        rc = create_record_store (state, &op, msg);
    }

    // an error occured
    else {
        rc = -1;
    }

    end_dbop (state, &op, (rc)? true: false);
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
    dbop_t op;

    start_dbop (state, &op);

    int first_requeued_key = 0; // can never be zero
    int rc = get_sibling (state, &op, DB_NEXT);

    while (
        !rc &&                                         // there's another item
        !resend_condition (state, &op) &&              // check threshold
        first_requeued_key != *(int *) op.key.data) {  // don't send requeued

        int cur_id = *(int *) op.key.data;
        record_t *header = op.val.data;

        // if early acks are reached, no record can follow
        if (header->type == RECORD_ACK) {
            break;
        }


        // skip tombstones
        if (header->type == RECORD_TOMBSTONE) {
            rc = get_sibling (state, &op, DB_NEXT);
            continue;
        }


        //  decrement tries
        assert (header->type == RECORD);
        if (update_record_tries (state, &op)) {
            rc = get_sibling (state, &op, DB_NEXT);
            continue;
        }


        //  update record
        //  cursor gets positioned to the new records location
        int new_id = create_msg_id (state);
        op.key.data = &new_id;
        if (!first_requeued_key) {
            first_requeued_key = new_id;
        }

        int prev_id = header->c.record.prev;
        header->c.record.ts = zclock_mono ();
        header->c.record.prev = cur_id;
        if (update (state, &op, DB_KEYFIRST)) {
            rc = -1;
            break;
        }
        state->last_stored += 1;
        sam_log_tracef (
            "requeued message '%d' (formerly '%d')", new_id, cur_id);


        //  resend message
        //  use current dbop state to read the message
        if (resend_message (state, &op)) {
            rc = -1;
            break;
        }


        //  create tombstone
        //  resets cursor position
        if (insert_tombstone (state, &op, prev_id, &cur_id)) {
            rc = -1;
            break;
        }

        rc = get_sibling (state, &op, DB_NEXT);
    }

    if (rc == DB_NOTFOUND) {
        rc = 0;
    }

    end_dbop (state, &op, (rc)? true: false);
    return rc;
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
    sam_db_destroy (&state->db);

    // clean up
    zsock_destroy (&state->in);
    zsock_destroy (&state->out);
    zsock_destroy (&state->store_sock);

    free (state);
}



//  --------------------------------------------------------------------------
/// If records are saved in the db, the global sequence number and
/// last_stored property must be set accordingly.
int
sam_db_restore (
    state_t *state)
{
    state->seq = 0;
    state->last_stored = 0;

    sam_db_t *db = state->db;

    int rc = sam_db_sibling (db, SAM_DB_PREV);
    if (rc && rc == SAM_DB_NOTFOUND) {
        rc = 0;
    }

    // there are records in the db, update seq and last_stored
    else if (!rc) {
        state->seq = sam_db_key (db);
        record_t *header = sam_db_val (db);

        if (header->type == RECORD) {
            state->last_stored = state->seq;
        }

        // find RECORD with highest key
        else {
            do {
                rc = sam_db_sibling (db, SAM_DB_PREV);
                if (rc == SAM_DB_NOTFOUND) {
                    state->last_stored = 0;
                    rc = 0;
                    break;
                }

                else if (!rc) {
                    record_t *header = sam_db_val (db);
                    if (header->type == RECORD) {
                        state->last_stored = sam_db_val (db);
                        break;
                    }
                }

            } while (rc);
        }

    }

    sam_db_end (db, (rc)? true: false);

    sam_log_infof (
        "restored state; seq: %d, last_stored: %d",
        state->seq, state->last_stored);

    return rc;
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


    // read config
    char
        *db_file_name,
        *db_home_name;

    if (sam_cfg_buf_retry_count (cfg, &state->tries) ||
        sam_cfg_buf_retry_interval (cfg, &state->interval) ||
        sam_cfg_buf_retry_threshold (cfg, &state->threshold)) {

        sam_log_error ("could not initialize the buffer");
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

    // restore state
    if (sam_db_restore (
            sam_db_t *db,
            &state->seq,
            &state->last_stored) {
        goto abort;
    }

    // spawn actor
    self->actor = zactor_new (actor, state);
    sam_log_info ("created buffer instance");

    return self;

abort:
    if (state->db) {
        sam_db_destroy (&state->db);
    }

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
    sam_db_destroy (&(*self)->db);

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
