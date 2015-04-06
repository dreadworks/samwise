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


/// return codes for db operations
typedef enum {
    SAM_DB_OK = 0,     // unnecessary, but explicit
    SAM_DB_NOTFOUND,
    SAM_DB_ERROR
} sam_db_ret_t;


/// flags for iteration or insertion
typedef enum {
    SAM_DB_PREV,
    SAM_DB_NEXT,
    SAM_DB_CURRENT,
    SAM_DB_KEY
} sam_db_flag_t;


//  --------------------------------------------------------------------------
/// @brief Create a new db instance, (re)-opens the database
/// @return A db instance
sam_db_t *
sam_db_new (
    zconfig_t *conf);


//  --------------------------------------------------------------------------
/// @brief Destroy a db instance, closes the database
/// @param self A db instance
void
sam_db_destroy (
    sam_db_t **self);

sam_db_ret_t
sam_db_begin (
    sam_db_t *self);

void
sam_db_end (
    sam_db_t *self,
    bool abort);

int
sam_db_get_key (
    sam_db_t *self);

void
sam_db_set_key (
    sam_db_t *self,
    int *key);

void
sam_db_get_val (
    sam_db_t *self,
    size_t *size,
    void **record);


sam_db_ret_t
sam_db_get (
    sam_db_t *self,
    int *key);


sam_db_ret_t
sam_db_sibling (
    sam_db_t *self,
    sam_db_flag_t trav);


sam_db_ret_t
sam_db_put (
    sam_db_t *self,
    size_t size,
    byte *record);

sam_db_ret_t
sam_db_update (
    sam_db_t *self,
    sam_db_flag_t flag);


sam_db_ret_t
sam_db_del (
    sam_db_t *self);


void *
sam_db_test ();


#endif
