/*  =========================================================================

    sam_buf - Persists messages

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief BerkeleyDB storage
   @file sam_buf.c

   TODO description


*/


#include "../include/sam_prelude.h"


/// all database related stuff
struct sam_db_t {
    DB_ENV *env;       ///< database environment
    DB *dbp;           ///< database pointer

    struct op {
        DB_TXN *txn;   ///< transaction handle
        DBC *cursor;   ///< to traverse the tree

        DBT key;       ///< buffer for the records key
        DBT val;       ///< buffer for the records data
    } op;

};



#define DBOP_SIZE sizeof (sam_db_t.op)
#define DBT_SIZE sizeof (DBT)


static void
stat_db_size (
    sam_db_t *self)
{
    DB_BTREE_STAT *statp;
    self->dbp->stat (self->dbp, NULL, &statp, 0);
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
/// Generic error handler invoked by DB.
static void
db_error_handler (
    const DB_ENV *db_env UU,
    const char *err_prefix UU,
    const char *msg)
{
    sam_log_errorf ("db error: %s", msg);
}


static void
clear_op (
    sam_db_t *self)
{
    self->op.txn = NULL;
    self->op.cursor = NULL;

    DBT
        *key = &self->op.key,
        *val = &self->op.val;

    size_t size = sizeof (DBT);

    memset (key, 0, size);
    memset (val, 0, size);
}


sam_db_t *
sam_db_new (
    const char *db_home_name,
    const char *db_file_name)
{
    sam_db_t *self = malloc (sizeof (sam_db_t));
    assert (self);
    clear_op (self);


    // initialize the environment
    uint32_t env_flags =
        DB_CREATE      |    // create environment if it's not there
        DB_INIT_TXN    |    // initialize transactions
        DB_INIT_LOCK   |    // locking (is this needed for st?)
        DB_INIT_LOG    |    // for recovery
        DB_INIT_MPOOL;      // in-memory cache

    int rc = db_env_create (&self->env, 0);
    if (rc) {
        sam_log_errorf (
            "could not create db environment: %s",
            db_strerror (rc));
        return NULL;
    }

    rc = self->env->open (self->env, db_home_name, env_flags, 0);
    if (rc) {
        sam_log_errorf (
            "could not open db environment: %s",
            db_strerror (rc));
        sam_db_destroy (&self);
        return NULL;
    }


    // open the database
    uint32_t db_flags = DB_CREATE | DB_AUTO_COMMIT;

    rc = db_create (&self->dbp, self->env, 0);
    if (rc) {
        self->env->err (self->env, rc, "database creation failed");
        sam_db_destroy (&self);
        return NULL;
    }

   rc = self->dbp->open (
        self->dbp,
        NULL,             // transaction pointer
        db_file_name,     // on disk file
        NULL,             // logical db name
        DB_BTREE,         // access method
        db_flags,         // open flags
        0);               // file mode

    if (rc) {
        self->env->err (self->env, rc, "database open failed");
        sam_db_destroy (&self);
        return NULL;
    }


    stat_db_size (self);
    self->dbp->set_errcall (self->dbp, db_error_handler);
    return self;
}


//  --------------------------------------------------------------------------
/// Close (partially) initialized database and environment.
void
sam_db_destroy (
    sam_db_t **self)
{
    int rc = 0;
    sam_db_t *db = *self;


    if (db->dbp) {
        stat_db_size (db);
        rc = db->dbp->close (db->dbp, 0);
        if (rc) {
            sam_log_errorf (
                "could not safely close db: %s",
                db_strerror (rc));
        }
    }

    if (db->env) {
        rc = db->env->close (db->env, 0);
        if (rc) {
            sam_log_errorf (
                "could not safely close db environment: %s",
                db_strerror (rc));
        }
    }


    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
/// Close the database cursor and end the transaction based on the
/// abort parameter: Either commit or abort.
void
sam_db_end (
    sam_db_t *self,
    bool abort)
{
    // handle cursor
    if (self->op.cursor != NULL) {
        self->op.cursor->close (self->op.cursor);
    }

    // handle transaction
    if (self->op.txn) {
        if (abort) {
            sam_log_error ("aborting transaction");
            self->op.txn->abort (self->op.txn);
        }

        else {
            int rc = self->op.txn->commit (self->op.txn, 0);
            if (rc) {
                self->env->err (self->env, rc, "transaction failed");
            }
        }
    }

    clear_op (self);
}


//  --------------------------------------------------------------------------
/// Create a database cursor and transaction handle.
int
sam_db_begin (
    sam_db_t *self)
{
    assert (self->op.txn == NULL);
    assert (self->op.cursor == NULL);


    // create transaction handle
    int rc = self->env->txn_begin (self->env, NULL, &self->op.txn, 0);

    if (rc) {
        self->env->err (self->env, rc, "transaction begin failed");
        return -1;
    }

    // create database cursor
    self->dbp->cursor (self->dbp, self->op.txn, &self->op.cursor, 0);
    if (self->op.cursor == NULL) {
        sam_log_error ("could not initialize cursor");
        sam_db_end (self, true);
        return -1;
    }

    return 0;
}


int
sam_db_get_key (
    sam_db_t *self)
{
    return *(int *) self->op.key.data;
}

void
sam_db_set_key (
    sam_db_t *self,
    int *id)
{
    DBT *key = &self->op.key;
    key->data = &id;
    key->size = sizeof (*key);
}


void
sam_db_get_val (
    sam_db_t *self,
    size_t *size,
    void **record)
{
    if (size != NULL) {
        *size = self->op.val.size;
    }

    if (record != NULL) {
        *record = self->op.val.data;
    }
}


static void
reset (
    DBT *key,
    DBT *val)
{
    memset (key, 0, DBT_SIZE);
    memset (val, 0, DBT_SIZE);
}



//  --------------------------------------------------------------------------
/// This function searches the db for the provided id and either fills
/// the dbop structure (return code 0), or returns DB_NOTFOUND or
/// another DB error code.
sam_db_ret_t
sam_db_get (
    sam_db_t *self,
    int *id)
{
    assert (self);

    DBT
        *key = &self->op.key,
        *val = &self->op.val;

    reset (key, val);
    sam_db_set_key (self, id);

    DBC *cursor = self->op.cursor;
    int rc = cursor->get (cursor, key, val, DB_SET);

    if (rc == DB_NOTFOUND) {
        sam_log_tracef (
            "'%d' was not found!", sam_db_get_key (self));
        return SAM_DB_NOTFOUND;
    }

    if (rc && rc != DB_NOTFOUND) {
        self->env->err (self->env, rc, "could not get record");
        return SAM_DB_ERROR;
    }

    return SAM_DB_OK;
}


sam_db_ret_t
sam_db_sibling (
    sam_db_t *self,
    sam_db_flag_t trav)
{
    assert (self);
    assert (trav == SAM_DB_PREV || trav == SAM_DB_CURRENT);

    uint32_t flag = 0;
    if (trav == SAM_DB_PREV) {
        flag = DB_PREV;
    } else if (trav == SAM_DB_NEXT) {
        flag = DB_NEXT;
    }

    DBT
        *key = &self->op.key,
        *val = &self->op.val;

    reset (key, val);

    DBC *cursor = self->op.cursor;
    int rc = cursor->get(cursor, key, val, flag);

    if (rc && rc != DB_NOTFOUND) {
        self->env->err (
            self->env, rc, "could not get next/previous item");
        return SAM_DB_ERROR;
    }

    return (rc == DB_NOTFOUND)? SAM_DB_NOTFOUND: SAM_DB_OK;
}


//  --------------------------------------------------------------------------
/// Insert a database record.
sam_db_ret_t
sam_db_put (
    sam_db_t *self,
    size_t size,
    byte *record)
{
    assert (self);
    assert (record);

    sam_log_tracef (
        "putting '%d' (size %d) into the database",
        sam_db_get_key (self), size);

    DBT
        *key = &self->op.key,
        *val = &self->op.val;

    memset (val, 0, DBT_SIZE);
    val->size = size;
    val->data = record;

    DBC *cursor = self->op.cursor;

    int rc = cursor->put (cursor, key, val, DB_KEYFIRST);
    if (rc) {
        self->env->err (self->env, rc, "could not put record");
        return SAM_DB_ERROR;
    }

    return SAM_DB_OK;
}


//  --------------------------------------------------------------------------
/// Update a database record. If the flag is SAM_DB_CURRENT, the key is
/// ignored and the cursors position is updated; if the flag is
/// SAM_DB_KEY, the key is used to determine where to put the record.
sam_db_ret_t
sam_db_update (
    sam_db_t *self,
    sam_db_flag_t kind)
{
    assert (self);
    assert (kind == SAM_DB_CURRENT || kind == SAM_DB_KEY);

    uint32_t flag;
    if (kind == SAM_DB_CURRENT) {
        flag = DB_CURRENT;
    } else if (kind == SAM_DB_KEY) {
        flag = DB_KEYFIRST;
    }

    DBT
        *key = &self->op.key,
        *val = &self->op.val;

    DBC *cursor= self->op.cursor;

    int rc = cursor->put (cursor, key, val, flag);
    if (rc) {
        self->env->err (self->env, rc, "could not update record");
        return SAM_DB_ERROR;
    }

    return SAM_DB_OK;
}


//  --------------------------------------------------------------------------
/// Delete the record the cursor currently points to.
sam_db_ret_t
sam_db_del (
    sam_db_t *self)
{
    assert (self);

    DBC *cursor = self->op.cursor;
    int rc = cursor->del (cursor, 0);
    if (rc) {
        self->env->err (self->env, rc, "could not delete record");
        return SAM_DB_ERROR;
    }

    return SAM_DB_OK;
}
