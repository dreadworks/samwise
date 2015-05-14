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

#ifdef __cplusplus
extern "C" {
#endif


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


//  --------------------------------------------------------------------------
/// @brief Begin a series of database operations
/// @param self A db instance
/// @return A db status code
sam_db_ret_t
sam_db_begin (
    sam_db_t *self);


//  --------------------------------------------------------------------------
/// @brief End database operations, either commit or abort.
/// @param self A db instance
/// @param abort If true, discards all changes since db_begin ()
/// @return A db status code
void
sam_db_end (
    sam_db_t *self,
    bool abort);


//  --------------------------------------------------------------------------
/// @brief Retrieve the key of the current db record
/// @param self A db instance
/// @return The records key
int
sam_db_get_key (
    sam_db_t *self);


//  --------------------------------------------------------------------------
/// @brief Set the key of the current record
/// @param self A db instance
/// @param key Pointer to the key (mind the lifespan)
/// @return The records key
void
sam_db_set_key (
    sam_db_t *self,
    int *key);


//  --------------------------------------------------------------------------
/// @brief Get the current records data
/// @param self A db instance
/// @param size Size to be set
/// @param record Pointer to the record to be set
void
sam_db_get_val (
    sam_db_t *self,
    size_t *size,
    void **record);


//  --------------------------------------------------------------------------
/// @brief Set the current record based on the key
/// @param self A db instance
/// @param key The key to look the record up with
sam_db_ret_t
sam_db_get (
    sam_db_t *self,
    int *key);


//  --------------------------------------------------------------------------
/// @brief Set the current record to a sibling of the former record
/// @param self A db instance
/// @param trav Either SAM_DB_NEXT or SAM_DB_PREV
/// @return A db status code
sam_db_ret_t
sam_db_sibling (
    sam_db_t *self,
    sam_db_flag_t trav);


//  --------------------------------------------------------------------------
/// @brief Insert some data into the database
/// @param self A db instance
/// @param size Record's size
/// @param record Pointer to the records memory location
/// @return A db status code
sam_db_ret_t
sam_db_put (
    sam_db_t *self,
    size_t size,
    byte *record);


//  --------------------------------------------------------------------------
/// @brief Update some existing data
/// @param self A db instance
/// @param flag Ignores the key for _CURRENT and re-inserts for _KEY
/// @return A db status code
sam_db_ret_t
sam_db_update (
    sam_db_t *self,
    sam_db_flag_t flag);


//  --------------------------------------------------------------------------
/// @brief Delete the current record
/// @param self A db instance
/// @return A db status code
sam_db_ret_t
sam_db_del (
    sam_db_t *self);


//  --------------------------------------------------------------------------
/// @brief Self test this class
void *
sam_db_test ();


#ifdef __cplusplus
}
#endif

#endif
