/*  =========================================================================

    sam_db - Storage system agnostic db interface

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief Handle persistent storage

   This interface exists to allow database engine indepented storage
   for sam_buf. Currently BerkeleyDB is supported. LMDB is going to
   follow

*/

#ifndef __SAM_DB_H__
#define __SAM_DB_H__


typedef struct sam_db_t sam_db_t;


typedef enum {
    SAM_DB_OK,
    SAM_DB_NOTFOUND,
    SAM_DB_ERROR
} sam_db_ret_t;


typedef enum {
    SAM_DB_PREV,
    SAM_DB_NEXT
} sam_db_trav_t;


sam_db_t *
sam_db_new (zconf_t *conf);


void
sam_db_destroy (
    sam_db_t **self);

int
sam_db_begin (
    sam_db_t *self);

void
sam_db_end (
    sam_db_t *self
    bool abort);

int
sam_db_key (
    sam_db_t *self);

void *
sam_db_val (
    sam_db_t *self);


sam_db_ret_t
sam_db_sibling (
    sam_db_t *self,
    sam_db_trav_t trav);




#endif
