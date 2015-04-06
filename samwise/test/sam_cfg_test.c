/*  =========================================================================

    sam_cfg_test - Test config loader

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/


#include "../include/sam_prelude.h"


sam_cfg_t *cfg; // TO BE REMOVED


static sam_cfg_t *
load (const char *fname)
{
    char *fmt;
    if (!strcmp (fname, "empty")) {
        fmt = "cfg/test/%s.cfg";
    }
    else{
        fmt = "cfg/test/cfg_%s.cfg";
    }

    char buf [256];
    snprintf (buf, 256, fmt, fname);
    sam_cfg_t *cfg = sam_cfg_new (buf);

    if (!cfg) {
        ck_abort_msg ("could not load config file");
    }

    return cfg;
}


//  --------------------------------------------------------------------------
/// Test cfg_buf_size ().
START_TEST(test_cfg_buf_size)
{
    sam_selftest_introduce ("test_cfg_buf_size");

    sam_cfg_t *cfg = load ("buf_size");

    uint64_t size;
    int rc = sam_cfg_buf_size (cfg, &size);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (size, 666);

    sam_cfg_destroy (&cfg);

}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_size () for byte values.
START_TEST(test_cfg_buf_size_b)
{
    sam_selftest_introduce ("test_cfg_buf_size_b");

    sam_cfg_t *cfg = load ("buf_size_b");

    uint64_t size;
    int rc = sam_cfg_buf_size (cfg, &size);
    ck_assert_int_eq (rc, 0);
    ck_assert (size == 666);

    sam_cfg_destroy (&cfg);

}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_size () for kilobyte values.
START_TEST(test_cfg_buf_size_k)
{
    sam_selftest_introduce ("test_cfg_buf_size_k");

    sam_cfg_t *cfg = load ("buf_size_k");

    uint64_t size;
    int rc = sam_cfg_buf_size (cfg, &size);
    ck_assert_int_eq (rc, 0);
    ck_assert (size == 666 * 1024);

    sam_cfg_destroy (&cfg);

}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_size () for megabyte values.
START_TEST(test_cfg_buf_size_m)
{
    sam_selftest_introduce ("test_cfg_buf_size_m");

    sam_cfg_t *cfg = load ("buf_size_m");

    uint64_t size;
    int rc = sam_cfg_buf_size (cfg, &size);
    ck_assert_int_eq (rc, 0);
    ck_assert (size == 666 * 1024 * 1024);

    sam_cfg_destroy (&cfg);

}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_size () for gigabyte values.
START_TEST(test_cfg_buf_size_g)
{
    sam_selftest_introduce ("test_cfg_buf_size_g");

    sam_cfg_t *cfg = load ("buf_size_g");

    uint64_t size;
    int rc = sam_cfg_buf_size (cfg, &size);
    ck_assert_int_eq (rc, 0);

    uint64_t ref = 666;
    ref *= 1024;
    ref *= 1024;
    ref *= 1024;
    ck_assert (size == ref);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_size () with empty config.
START_TEST(test_cfg_buf_size_empty)
{
    sam_selftest_introduce ("test_cfg_buf_size_empty");

    sam_cfg_t *cfg = load ("empty");

    uint64_t size;
    int rc = sam_cfg_buf_size (cfg, &size);
    ck_assert_int_eq (rc, -1);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_count ().
START_TEST(test_cfg_buf_retry_count)
{
    sam_selftest_introduce ("test_cfg_buf_retry_count");

    sam_cfg_t *cfg = load ("buf_retry_count");

    int count;
    int rc = sam_cfg_buf_retry_count (cfg, &count);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (count, 11);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_count () with empty config.
START_TEST(test_cfg_buf_retry_count_empty)
{
    sam_selftest_introduce ("test_cfg_buf_retry_count_empty");

    sam_cfg_t *cfg = load ("empty");

    int count;
    int rc = sam_cfg_buf_retry_count (cfg, &count);
    ck_assert_int_eq (rc, -1);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_interval ().
START_TEST(test_cfg_buf_retry_interval)
{
    sam_selftest_introduce ("test_cfg_buf_retry_interval");

    sam_cfg_t *cfg = load ("buf_retry_interval");

    uint64_t size;
    int rc = sam_cfg_buf_retry_interval (cfg, &size);
    ck_assert_int_eq (rc, 0);
    ck_assert (size == 17);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_interval () with milliseconds.
START_TEST(test_cfg_buf_retry_interval_ms)
{
    sam_selftest_introduce ("test_cfg_buf_retry_interval_ms");

    sam_cfg_t *cfg = load ("buf_retry_interval_ms");

    uint64_t interval;
    int rc = sam_cfg_buf_retry_interval (cfg, &interval);
    ck_assert_int_eq (rc, 0);
    ck_assert (interval == 17);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_interval () with seconds.
START_TEST(test_cfg_buf_retry_interval_s)
{
    sam_selftest_introduce ("test_cfg_buf_retry_interval_s");

    sam_cfg_t *cfg = load ("buf_retry_interval_s");

    uint64_t interval;
    int rc = sam_cfg_buf_retry_interval (cfg, &interval);
    ck_assert_int_eq (rc, 0);
    ck_assert (interval == 17 * 1000);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_interval () with minutes.
START_TEST(test_cfg_buf_retry_interval_min)
{
    sam_selftest_introduce ("test_cfg_buf_retry_interval_min");

    sam_cfg_t *cfg = load ("buf_retry_interval_min");

    uint64_t interval;
    int rc = sam_cfg_buf_retry_interval (cfg, &interval);
    ck_assert_int_eq (rc, 0);
    ck_assert (interval == 17 * 1000 * 60);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_interval () with hours.
START_TEST(test_cfg_buf_retry_interval_h)
{
    sam_selftest_introduce ("test_cfg_buf_retry_interval_h");

    sam_cfg_t *cfg = load ("buf_retry_interval_h");

    uint64_t interval;
    int rc = sam_cfg_buf_retry_interval (cfg, &interval);
    ck_assert_int_eq (rc, 0);
    ck_assert (interval == 17 * 1000 * 60 * 60);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_interval () with days.
START_TEST(test_cfg_buf_retry_interval_d)
{
    sam_selftest_introduce ("test_cfg_buf_retry_interval_d");

    sam_cfg_t *cfg = load ("buf_retry_interval_d");

    uint64_t interval;
    int rc = sam_cfg_buf_retry_interval (cfg, &interval);
    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (interval, 17 * 1000 * 60 * 60 * 24);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_interval () without configuration.
START_TEST(test_cfg_buf_retry_interval_empty)
{
    sam_selftest_introduce ("test_cfg_buf_retry_interval_empty");

    sam_cfg_t *cfg = load ("empty");

    uint64_t interval;
    int rc = sam_cfg_buf_retry_interval (cfg, &interval);
    ck_assert_int_eq (rc, -1);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_threshold ().
START_TEST(test_cfg_buf_retry_threshold)
{
    sam_selftest_introduce ("test_cfg_buf_retry_threshold");

    sam_cfg_t *cfg = load ("buf_retry_threshold");

    uint64_t size;
    int rc = sam_cfg_buf_retry_threshold (cfg, &size);
    ck_assert_int_eq (rc, 0);
    ck_assert (size == 42);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_threshold () with milliseconds.
START_TEST(test_cfg_buf_retry_threshold_ms)
{
    sam_selftest_introduce ("test_cfg_buf_retry_threshold_ms");

    sam_cfg_t *cfg = load ("buf_retry_threshold_ms");

    uint64_t threshold;
    int rc = sam_cfg_buf_retry_threshold (cfg, &threshold);
    ck_assert_int_eq (rc, 0);
    ck_assert (threshold == 42);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_threshold () with seconds.
START_TEST(test_cfg_buf_retry_threshold_s)
{
    sam_selftest_introduce ("test_cfg_buf_retry_threshold_s");

    sam_cfg_t *cfg = load ("buf_retry_threshold_s");

    uint64_t threshold;
    int rc = sam_cfg_buf_retry_threshold (cfg, &threshold);
    ck_assert_int_eq (rc, 0);
    ck_assert (threshold == 42 * 1000);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_threshold () with minutes.
START_TEST(test_cfg_buf_retry_threshold_min)
{
    sam_selftest_introduce ("test_cfg_buf_retry_threshold_min");

    sam_cfg_t *cfg = load ("buf_retry_threshold_min");

    uint64_t threshold;
    int rc = sam_cfg_buf_retry_threshold (cfg, &threshold);
    ck_assert_int_eq (rc, 0);
    ck_assert (threshold == 42 * 1000 * 60);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_threshold () with hours.
START_TEST(test_cfg_buf_retry_threshold_h)
{
    sam_selftest_introduce ("test_cfg_buf_retry_threshold_h");

    sam_cfg_t *cfg = load ("buf_retry_threshold_h");

    uint64_t threshold;
    int rc = sam_cfg_buf_retry_threshold (cfg, &threshold);
    ck_assert_int_eq (rc, 0);
    ck_assert (threshold == 42 * 1000 * 60 * 60);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_threshold () with days.
START_TEST(test_cfg_buf_retry_threshold_d)
{
    sam_selftest_introduce ("test_cfg_buf_retry_threshold_d");

    sam_cfg_t *cfg = load ("buf_retry_threshold_d");

    uint64_t threshold;
    int rc = sam_cfg_buf_retry_threshold (cfg, &threshold);
    ck_assert_int_eq (rc, 0);

    uint64_t ref = 42;
    ref *= 1000;
    ref *= 60;
    ref *= 60;
    ref *= 24;

    ck_assert (threshold == ref);
    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_buf_retry_threshold () without configuration.
START_TEST(test_cfg_buf_retry_threshold_empty)
{
    sam_selftest_introduce ("test_cfg_buf_retry_threshold_empty");

    sam_cfg_t *cfg = load ("empty");

    uint64_t threshold;
    int rc = sam_cfg_buf_retry_threshold (cfg, &threshold);
    ck_assert_int_eq (rc, -1);

    sam_cfg_destroy (&cfg);
}
END_TEST





//  --------------------------------------------------------------------------
/// Test cfg_endpoint ().
START_TEST(test_cfg_endpoint)
{
    sam_selftest_introduce ("test_cfg_endpoint");

    sam_cfg_t *cfg = load ("endpoint");

    char *endpoint;
    int rc = sam_cfg_endpoint (cfg, &endpoint);
    ck_assert_int_eq (rc, 0);

    zsock_t *sock = zsock_new_rep (endpoint);
    if (!sock) {
        ck_abort_msg ("endpoint is not valid");
    }

    zsock_destroy (&sock);
    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_endpoint () when there's no configuration.
START_TEST(test_cfg_endpoint_empty)
{
    sam_selftest_introduce ("test_cfg_endpoint_empty");

    sam_cfg_t *cfg = load ("empty");

    char *endpoint;
    int rc = sam_cfg_endpoint (cfg, &endpoint);
    ck_assert_int_eq (rc, -1);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_be_type ().
START_TEST(test_cfg_be_type_rmq)
{
    sam_selftest_introduce ("test_cfg_be_type");
    sam_cfg_t *cfg = load ("be_type_rmq");

    sam_be_t be_type;
    int rc = sam_cfg_be_type (cfg, &be_type);

    ck_assert_int_eq (rc, 0);
    ck_assert (be_type == SAM_BE_RMQ);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_be_type () when there's no configuration.
START_TEST(test_cfg_be_type_empty)
{
    sam_selftest_introduce ("test_cfg_be_type_empty");
    sam_cfg_t *cfg = load ("empty");

    sam_be_t be_type;
    int rc = sam_cfg_be_type (cfg, &be_type);
    ck_assert_int_eq (rc, -1);

    sam_cfg_destroy (&cfg);
}
END_TEST



//  --------------------------------------------------------------------------
/// Test cfg_be_backends ().
START_TEST(test_cfg_be_backends_rmq)
{
    sam_selftest_introduce ("test_cfg_be_backends_rmq");
    sam_cfg_t *cfg = load ("be_backends_rmq");

    int backend_count;
    char **names;
    sam_be_rmq_opts_t *opts;

    int rc = sam_cfg_be_backends (
        cfg, SAM_BE_RMQ, &backend_count, &names, (void **) &opts);

    ck_assert_int_eq (rc, 0);
    ck_assert_int_eq (backend_count, 2);

    // broker-1
    ck_assert_str_eq (*names, "broker-1");
    ck_assert_str_eq (opts->host, "localhost");
    ck_assert_int_eq (opts->port, 5672);
    ck_assert_str_eq (opts->user, "guest");
    ck_assert_str_eq (opts->pass, "guest");
    ck_assert_int_eq (opts->heartbeat, 3);

    names += 1;
    opts += 1;

    // broker-2
    ck_assert_str_eq (*names, "broker-2");
    ck_assert_str_eq (opts->host, "localhost");
    ck_assert_int_eq (opts->port, 5673);
    ck_assert_str_eq (opts->user, "guest");
    ck_assert_str_eq (opts->pass, "guest");
    ck_assert_int_eq (opts->heartbeat, 3);

    // reset pointers for cleanup
    names -= 1;
    free (names);

    opts -= 1;
    free (opts);

    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Test cfg_be_backends () when there's no configuration.
START_TEST(test_cfg_be_backends_rmq_empty)
{
    sam_selftest_introduce ("test_cfg_be_backends_rmq_empty");

    sam_cfg_t *cfg = load ("empty");

    int backend_count;
    char **names;
    sam_be_rmq_opts_t *opts;
    sam_be_t be_type;

    sam_cfg_be_type (cfg, &be_type);
    int rc = sam_cfg_be_backends (
        cfg, be_type, &backend_count, &names, (void **) &opts);

    ck_assert_int_eq (rc, -1);
    sam_cfg_destroy (&cfg);
}
END_TEST


//  --------------------------------------------------------------------------
/// Self test this class.
void *
sam_cfg_test ()
{
    Suite *s = suite_create ("sam_cfg");

    TCase *tc = tcase_create("buffer size");
    tcase_add_test (tc, test_cfg_buf_size);
    tcase_add_test (tc, test_cfg_buf_size_b);
    tcase_add_test (tc, test_cfg_buf_size_k);
    tcase_add_test (tc, test_cfg_buf_size_m);
    tcase_add_test (tc, test_cfg_buf_size_g);
    tcase_add_test (tc, test_cfg_buf_size_empty);
    suite_add_tcase (s, tc);

    tc = tcase_create("buffer retry count");
    tcase_add_test (tc, test_cfg_buf_retry_count);
    tcase_add_test (tc, test_cfg_buf_retry_count_empty);
    suite_add_tcase (s, tc);

    tc = tcase_create("buffer retry interval");
    tcase_add_test (tc, test_cfg_buf_retry_interval);
    tcase_add_test (tc, test_cfg_buf_retry_interval_ms);
    tcase_add_test (tc, test_cfg_buf_retry_interval_s);
    tcase_add_test (tc, test_cfg_buf_retry_interval_min);
    tcase_add_test (tc, test_cfg_buf_retry_interval_h);
    tcase_add_test (tc, test_cfg_buf_retry_interval_d);
    tcase_add_test (tc, test_cfg_buf_retry_interval_empty);
    suite_add_tcase (s, tc);

    tc = tcase_create("buffer retry threshold");
    tcase_add_test (tc, test_cfg_buf_retry_threshold);
    tcase_add_test (tc, test_cfg_buf_retry_threshold_ms);
    tcase_add_test (tc, test_cfg_buf_retry_threshold_s);
    tcase_add_test (tc, test_cfg_buf_retry_threshold_min);
    tcase_add_test (tc, test_cfg_buf_retry_threshold_h);
    tcase_add_test (tc, test_cfg_buf_retry_threshold_d);
    tcase_add_test (tc, test_cfg_buf_retry_threshold_empty);
    suite_add_tcase (s, tc);

    tc = tcase_create("buffer endpoint");
    tcase_add_test (tc, test_cfg_endpoint);
    tcase_add_test (tc, test_cfg_endpoint_empty);
    suite_add_tcase (s, tc);

    tc = tcase_create("backends");
    tcase_add_test (tc, test_cfg_be_type_rmq);
    tcase_add_test (tc, test_cfg_be_type_empty);

    tcase_add_test (tc, test_cfg_be_backends_rmq);
    tcase_add_test (tc, test_cfg_be_backends_rmq_empty);
    suite_add_tcase (s, tc);

    return s;
}
