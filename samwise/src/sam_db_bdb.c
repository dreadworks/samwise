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
typedef struct sam_db_t {
    DB_ENV *env;       ///< database environment
    DB *dbp;           ///< database pointer

    struct op {
        DB_TXN *txn;   ///< transaction handle
        DBC *cursor;   ///< to traverse the tree

        DBT key;       ///< buffer for the records key
        DBT val;       ///< buffer for the records data
    } op;

} sam_db_t;


#define DBOP_SIZE sizeof (sam_db_t.op);



static void
clear_op (sam_db_t *self)
{
    self->op.txn = NULL;
    self->op.cursor = NULL;
    memset (self->op.key, 0, sizeof (DBT));
    memset (self->op.val, 0, sizeof (DBT));
}



sam_db_t *
sam_db_new ()
{
    sam_db_t *self = malloc (sizeof (sam_db_t));
    assert (self);

    clear_op (self);
}

void
sam_db_destroy (sam_db_t **self)
{
    assert (*self);
    close_db (state);
}




//  --------------------------------------------------------------------------
/// Close the database cursor and end the transaction based on the
/// abort parameter: Either commit or abort.
static void
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
            if (self->op.txn->commit (self->op.txn, 0)) {
                self->env->err (self->env, rc, "transaction failed");
            }
        }
    }

    clear_op (self);
}


//  --------------------------------------------------------------------------
/// Create a database cursor and transaction handle.
static int
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
sam_db_key (
    sam_db_t *self)
{
    assert (self->op.key);
    return *(int *) self->op.key.data;
}

void *
sam_db_val (
    sam_db_t *self)
{
    assert (self->op.val);
    return self->op.val.data;
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
int
sam_db_get (
    sam_db_t *self,
    int *key)
{
    DBT
        *key = &self->op.key,
        *val = &self->op.val;

    reset (key, val);
    key.size = sizeof (*key);
    key.data = key;

    DBC *cursor = self->op.cursor;
    int rc = op->cursor->get (cursor, key, val, DB_SET);

    if (rc == DB_NOTFOUND) {
        sam_log_tracef ("'%d' was not found!", sam_db_key (self));
        return SAM_DB_NOTFOUND;
    }

    if (rc && rc != DB_NOTFOUND) {
        state->db.e->err (state->db.e, rc, "could not get record");
        return SAM_DB_ERROR;
    }

    return SAM_DB_OK;
}


sam_db_ret_t
sam_db_sibling (
    sam_db_t *self,
    sam_db_trav_t trav)
{
    assert (db);
    assert (trav == SAM_DB_PREV || trav == SAM_DB_CURRENT);

    uint32_t flag;
    if (trav == SAM_DB_PREV) {
        flag = DB_PREV;
    } else if (trav == SAM_DB_NEXT) {
        flag = DB_NEXT;
    }

    reset (self);

    DBC *cursor = self->op.cursor;
    int rc = cursor->get(cursor, key, val, flag);

    if (rc && rc != DB_NOTFOUND) {
        state->db.e->err (
            state->db.e, rc, "could not get next/previous item");
        return SAM_DB_ERROR;
    }

    return (rc == DB_NOTFOUND)? SAM_DB_NOTFOUND: SAM_DB_OK;
}
