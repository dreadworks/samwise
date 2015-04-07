/*  =========================================================================

    sam_buf_test - Test the buffer

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/


#include "../include/sam_prelude.h"


sam_db_t *db;
sam_cfg_t *cfg;


//  --------------------------------------------------------------------------
/// Create sockets and a sam_buf instance.
static void
setup ()
{
    cfg = sam_cfg_new ("cfg/test/db.cfg");

    zconfig_t *conf;
    int rc = sam_cfg_get (cfg, "db/bdb", &conf);
    ck_assert_int_eq (rc, 0);

    db = sam_db_new (conf);
}


//  --------------------------------------------------------------------------
/// Tear down test fixture.
static void
destroy ()
{

    sam_db_destroy (&db);
    sam_cfg_destroy (&cfg);
    ck_assert (db == NULL);
}


//  --------------------------------------------------------------------------
/// Tests basic db operations
START_TEST(test_db_get_put)
{
    sam_selftest_introduce ("test_db_get_put");

    int key = 100;
    int data = 0xf00;

    sam_db_begin (db);
    size_t size = sizeof (data);
    sam_db_ret_t ret;


    // put ()
    sam_db_set_key (db, &key);
    ret = sam_db_put (db, size, (void *) &data);
    ck_assert (ret == SAM_DB_OK);


    // get ()
    ret = sam_db_get (db, &key);
    ck_assert (ret == SAM_DB_OK);


    // get_val ()
    size_t ret_size;
    int *ret_data;
    sam_db_get_val (db, &ret_size, (void **) &ret_data);
    ck_assert_int_eq (ret_size, size);
    ck_assert_int_eq (data, *ret_data);


    // get () again
    ret = sam_db_get (db, &key);
    ck_assert (ret == SAM_DB_OK);


    // get_key ()
    int ret_key = sam_db_get_key (db);
    ck_assert_int_eq (ret_key, key);


    // start a new transaction
    sam_db_end (db, false);
    sam_db_begin (db);


    // del
    sam_db_get (db, &key);
    ret = sam_db_del (db);
    ck_assert (ret == SAM_DB_OK);

    sam_db_end (db, false);
}
END_TEST


//  --------------------------------------------------------------------------
/// Tests basic db operations
START_TEST(test_db_sibling)
{
    sam_selftest_introduce ("test_db_sibling");

    sam_db_begin (db);

    // insert three records (1, 2), (3, 4), (4, 5)
    int data [] = {
        1, 2,
        3, 4
    };

    size_t size = sizeof (data[0]);
    sam_db_ret_t ret;

    int i, record_count = sizeof (data) / sizeof (data [0]);

    for (i = 0; i < record_count; i += 2) {
        sam_db_set_key (db, &data [i]);
        ret = sam_db_put (db, size, (void *) &data [i+1]);
        ck_assert (ret == SAM_DB_OK);
    }

    int ret_key;

    // iteration with siblings
    ret = sam_db_get (db, &data [0]);
    ck_assert (ret == SAM_DB_OK);

    ret = sam_db_sibling (db, SAM_DB_NEXT);
    ck_assert (ret == SAM_DB_OK);
    ret_key = sam_db_get_key (db);
    ck_assert_int_eq (ret_key, data [2]);

    ret = sam_db_sibling (db, SAM_DB_PREV);
    ck_assert (ret == SAM_DB_OK);
    ret_key = sam_db_get_key (db);
    ck_assert_int_eq (ret_key, data [0]);

    ret = sam_db_sibling (db, SAM_DB_PREV);
    ck_assert (ret == SAM_DB_NOTFOUND);

    // del
    sam_db_get (db, &data [0]);
    do {
        sam_db_del (db);
    } while (!sam_db_sibling (db, SAM_DB_NEXT));

    sam_db_end (db, false);
}
END_TEST


//  --------------------------------------------------------------------------
/// Tests basic db operations
START_TEST(test_db_update)
{
    sam_selftest_introduce ("test_db_update");

    int key = 100;
    int data = 0xf00;

    sam_db_begin (db);
    size_t size = sizeof (data);
    sam_db_ret_t ret;


    // put ()
    sam_db_set_key (db, &key);
    ret = sam_db_put (db, size, (void *) &data);
    ck_assert (ret == SAM_DB_OK);

    ret = sam_db_get (db, &key);
    ck_assert (ret == SAM_DB_OK);

    // retrieve pointer to data to overwrite it
    int *record;
    sam_db_get_val (db, NULL, (void **) &record);
    ck_assert_int_eq (*record, data);

    int other_data = 0xbaa;
    *record = other_data;

    ret = sam_db_update (db, SAM_DB_CURRENT);
    ck_assert (ret == SAM_DB_OK);

    // check if data got updated
    sam_db_get (db, &key);
    int *other_record;
    sam_db_get_val (db, NULL, (void **) &other_record);
    ck_assert_int_eq (*other_record, other_data);


    // del
    ret = sam_db_del (db);
    ck_assert (ret == SAM_DB_OK);

    sam_db_end (db, false);
}
END_TEST


START_TEST(test_db_update_key)
{
    sam_selftest_introduce ("test_db_update_key");

    int key = 100;
    int data = 0xf00;

    sam_db_begin (db);
    size_t size = sizeof (data);
    sam_db_ret_t ret;


    // put ()
    sam_db_set_key (db, &key);
    ret = sam_db_put (db, size, (void *) &data);
    ck_assert (ret == SAM_DB_OK);

    ret = sam_db_get (db, &key);
    ck_assert (ret == SAM_DB_OK);

    // retrieve pointer to data to overwrite it
    int *record;
    sam_db_get_val (db, NULL, (void **) &record);
    ck_assert_int_eq (*record, data);

    int other_data = 0xbaa;
    *record = other_data;

    int other_key = 200;
    sam_db_set_key (db, &other_key);

    ret = sam_db_update (db, SAM_DB_KEY);
    ck_assert (ret == SAM_DB_OK);

    // check if data got updated
    ret = sam_db_get (db, &key);
    ck_assert (ret == SAM_DB_OK);
    sam_db_del (db);

    ret = sam_db_get (db, &other_key);
    ck_assert (ret == SAM_DB_OK);

    // del
    ret = sam_db_del (db);
    ck_assert (ret == SAM_DB_OK);

    sam_db_end (db, false);
}
END_TEST



void *
sam_db_test ()
{
    Suite *s = suite_create ("sam_db");

    TCase *tc = tcase_create ("basic db operations");
    tcase_add_unchecked_fixture (tc, setup, destroy);
    tcase_add_test (tc, test_db_get_put);
    tcase_add_test (tc, test_db_sibling);
    tcase_add_test (tc, test_db_update);
    tcase_add_test (tc, test_db_update_key);
    suite_add_tcase (s, tc);

    return s;
}
