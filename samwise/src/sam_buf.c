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
/// Delete a database record and all its tombstones.
static int
del (
    state_t *state)
{
    assert (state);

    int rc, curr_key, prev_key;
    rc = curr_key = prev_key = 0;

    sam_db_t *db = state->db;

    do {
        sam_db_del (db);

        // determine previous tombstone - if any
        record_t *header;
        sam_db_get_val (db, NULL, (void **) &header);

        if (header->type == RECORD) {
            prev_key = header->c.record.prev;
        }
        else if (header->type == RECORD_TOMBSTONE) {
            prev_key = header->c.tombstone.prev;
        }

        // prepare next round
        if (prev_key) {
            curr_key = prev_key;
            rc = sam_db_get (db, &prev_key);

            if (rc != SAM_DB_OK) {
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
    int prev,
    int *position)
{
    sam_db_t *db = state->db;

    size_t size = sizeof (record_t);
    int next = sam_db_get_key (db);

    record_t tombstone;
    memset (&tombstone, 0, size);

    sam_log_tracef ("creating tombstone: %d | %d", prev, *position, next);

    tombstone.type = RECORD_TOMBSTONE;
    tombstone.c.tombstone.prev = prev;
    tombstone.c.tombstone.next = next;

    // write new key
    sam_db_set_key (db, position);

    // write new data
    return sam_db_put (db, size, (void *) &tombstone);
}


//  --------------------------------------------------------------------------
/// Decrements the try-counter and returns a non-zero value for
/// to-be-discarded messages.
static int
update_record_tries (
    state_t *state,
    record_t *header)
{
    header->c.record.tries -= 1;

    if (!header->c.record.tries) {
        sam_log_infof (
            "discarding message '%d'", sam_db_get_key (state->db));

        del (state);
        return -1;
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Checks if the record is inside the bounds of messages to be resent.
static int
resend_condition (
    state_t *state,
    record_t *header)
{
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
    state_t *state)
{
    record_t *header;
    size_t record_size;
    sam_db_t *db = state->db;

    sam_db_get_val (db, &record_size, (void **) &header);
    size_t header_size = sizeof (record_t);

    // decode message
    size_t msg_size = record_size - header_size;
    byte *encoded_msg = (byte *) header + header_size;
    sam_msg_t *msg = sam_msg_decode (encoded_msg, msg_size);
    if (msg == NULL) {
        sam_log_error ("could not decode stored message");
        return -1;
    }

    // wrap backend acknowledgments
    uint64_t be_acks = header->c.record.be_acks;
    zframe_t *id_frame = zframe_new (&be_acks, sizeof (be_acks));
    int msg_id = sam_db_get_key (db);

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
    sam_msg_t *msg)
{
    size_t size, header_size;
    record_size (&size, &header_size, msg);

    byte *record;
    record = malloc (size);
    if (!record) {
        return -1;
    }

    sam_db_t *db = state->db;
    sam_log_tracef (
        "creating record for msg '%d'", sam_db_get_key (db));

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
    sam_db_ret_t ret = sam_db_put (db, size, record);

    free (record);
    return ret;
}


//  --------------------------------------------------------------------------
/// If an acknowledgement already created a database record, this
/// function is used to either delete the record if only one
/// acknowledgment suffices or update the meta information and append
/// the encoded messages.
static int
update_record_store (
    state_t *state,
    sam_msg_t *msg)
{
    int rc = 0;
    size_t total_size, header_size;
    record_size (&total_size, &header_size, msg);

    sam_db_t *db = state->db;

    record_t *header;
    sam_db_get_val (db, NULL, (void **) &header);
    assert (header->type == RECORD_ACK);

    sam_log_tracef (
        "ack already there, %d arrived already",
        header->c.record.acks_remaining * -1);
    header->c.record.acks_remaining += acks_remaining (msg);

    // remove if there are no outstanding acks
    if (!header->c.record.acks_remaining) {
        rc = del (state);
    }

    // add encoded message to the record
    else {
        byte *record = malloc (total_size);
        if (!record) {
            return -1;
        }

        // copy header
        byte
            *header_cpy = memmove (record, header, header_size),
            *content = record + header_size;

        assert (header_cpy);
        sam_msg_encode (msg, &content);
        rc = sam_db_put (db, total_size, record);
    }

    return rc;
}


//  --------------------------------------------------------------------------
/// Create a new database record just containing meta information and
/// no payload.
static int
create_record_ack (
    state_t *state,
    uint64_t backend_id)
{
    record_t record;
    record.type = RECORD_ACK;
    record.c.record.acks_remaining = -1;
    record.c.record.be_acks = backend_id;

    sam_log_tracef (
        "created record (ack) '%d'", sam_db_get_key (state->db));

    return sam_db_put (state->db, sizeof (record_t), (void *) &record);
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
    uint64_t backend_id)
{
    sam_db_t *db = state->db;
    int rc = 0;

    // skip all tombstones up to the record
    record_t *header;

    sam_db_get_val (db, NULL, (void **) &header);
    while (header->type == RECORD_TOMBSTONE) {

        int new_id = header->c.tombstone.next;
        sam_log_tracef ("following tombstone chain to '%d'", new_id);
        if (sam_db_get (db, &new_id)) {
            return -1;
        }

        sam_db_get_val (db, NULL, (void **) &header);
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
        rc = sam_db_del (db);
    }


    // not enough acks, update record
    else {
        sam_log_tracef (
            "updating '%d', acks remaining: %d",
            sam_db_get_key (db),
            header->c.record.acks_remaining);

        header->type = RECORD;
        rc = sam_db_update (db, SAM_DB_CURRENT);
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
    sam_db_t *db = state->db;

    if (sam_db_begin (db)) {
        return -1;
    }

    int rc = sam_db_get (db, &ack_id);

    // record already there, update data
    if (rc == SAM_DB_OK) {
        rc = update_record_ack (state, backend_id);
    }

    // record not yet there, create db entry
    else if (rc == SAM_DB_NOTFOUND) {
        if (ack_id < state->last_stored) {
            sam_log_tracef ("ignoring late ack '%d'", ack_id);
            rc = 0;
        } else {
            rc = create_record_ack (state, backend_id);
        }
    }

    else {
        rc = -1;
    }

    sam_db_end (db, (rc)? true: false);
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

    sam_db_ret_t ret = sam_db_get (db, &msg_id);
    int rc = -1;

    // record already there (ack arrived early), update data
    if (ret == SAM_DB_OK) {
        rc = update_record_store (state, msg);
    }

    // record not yet there, create db entry
    else if (ret == SAM_DB_NOTFOUND) {
        // key was already set by get ()
        rc = create_record_store (state, msg);
    }

    sam_db_end (db, (rc)? true: false);
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
    sam_db_t *db = state->db;

    sam_db_begin (db);

    int first_requeued_key = 0; // can never be zero
    int rc = sam_db_sibling (db, SAM_DB_NEXT);

    record_t *header;
    sam_db_get_val (db, NULL, (void **) &header);

    while (
        !rc &&                                        // there's another item
        !resend_condition (state, header) &&          // check threshold
        first_requeued_key != sam_db_get_key (db)) {  // don't send requeued

        int cur_id = sam_db_get_key (db);
        sam_db_get_val (db, NULL, (void **) &header);

        // if early acks are reached, no record can follow
        if (header->type == RECORD_ACK) {
            break;
        }


        // skip tombstones
        if (header->type == RECORD_TOMBSTONE) {
            rc = sam_db_sibling (db, SAM_DB_NEXT);
            continue;
        }


        //  decrement tries
        assert (header->type == RECORD);
        if (update_record_tries (state, header)) {
            rc = sam_db_sibling (db, SAM_DB_NEXT);
            continue;
        }


        //  update record
        //  cursor gets positioned to the new records location
        int new_id = create_msg_id (state);
        if (!first_requeued_key) {
            first_requeued_key = new_id;
        }

        sam_db_set_key (db, &new_id);

        int prev_id = header->c.record.prev;
        header->c.record.ts = zclock_mono ();
        header->c.record.prev = cur_id;
        if (sam_db_update (db, SAM_DB_KEY)) {
            rc = -1;
            break;
        }
        state->last_stored += 1;
        sam_log_tracef (
            "requeued message '%d' (formerly '%d')", new_id, cur_id);


        //  resend message
        //  use current dbop state to read the message
        if (resend_message (state)) {
            rc = -1;
            break;
        }


        //  create tombstone
        //  resets cursor position
        if (insert_tombstone (state, prev_id, &cur_id)) {
            rc = -1;
            break;
        }

        rc = sam_db_sibling (db, SAM_DB_NEXT);
    }

    if (rc == SAM_DB_NOTFOUND) {
        rc = 0;
    }

    sam_db_end (db, (rc)? true: false);
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
    if (sam_db_begin (db)) {
        return -1;
    }

    int rc = sam_db_sibling (db, SAM_DB_PREV);
    if (rc && rc == SAM_DB_NOTFOUND) {
        rc = 0;
    }

    // there are records in the db, update seq and last_stored
    else if (!rc) {
        state->seq = sam_db_get_key (db);
        record_t *header;
        sam_db_get_val (db, NULL, (void **) &header);

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
                    sam_db_get_val (db, NULL, (void **) &header);
                    if (header->type == RECORD) {
                        state->last_stored = sam_db_get_key (db);
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

    if (sam_cfg_buf_file (cfg, &db_file_name) ||
        sam_cfg_buf_home (cfg, &db_home_name) ||
        sam_cfg_buf_retry_count (cfg, &state->tries) ||
        sam_cfg_buf_retry_interval (cfg, &state->interval) ||
        sam_cfg_buf_retry_threshold (cfg, &state->threshold)) {

        sam_log_error ("could not initialize the buffer");
        goto abort;
    }

    // create db
    state->db = sam_db_new (db_home_name, db_file_name);

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
    if (sam_db_restore (state)) {
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
