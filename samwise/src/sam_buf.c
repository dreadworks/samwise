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
/// Create a sam buf instance.
sam_buf_t *
sam_buf_new (const char *fname)
{
    assert (fname);

    sam_buf_t *self = malloc (sizeof (sam_buf_t));
    assert (self);

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

    return self;
}


//  --------------------------------------------------------------------------
/// Destroy a sam buf instance.
void
sam_buf_destroy (sam_buf_t **self)
{
    assert (*self);
    (*self)->dbp->close ((*self)->dbp, 0);

    free (*self);
    *self = NULL;
}
