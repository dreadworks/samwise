/*  =========================================================================

    sam_cfg - easy access to the configuration

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief public API
   @file sam.c

   This class wraps a zconfig instance and offers easy, failure proof
   access to samwise specific configuration options.

*/


#include "../include/sam_prelude.h"


//  --------------------------------------------------------------------------
/// Resolve a set of options. The picture, provided as the second
/// argument, defines the number of expected arguments and their type.
///
/// For example, consider a zconfig_t *ptr points to loaded
/// configuration like this:
///
/// <code>
///
/// foo = bar
/// number = 1234
/// subsection
///     foo = bar
///
/// </code>
///
/// It is easy to load all those information in one call by invoking
/// this function like this:
///
/// <code>
///
/// char *foo, *subfoo;
/// int nbr;
///
/// resolve (
///     ptr, "iss",
///     "foo", &foo,
///     "number", &nbr,
///     "subsection/foo", &subfoo);
///
/// </code>
static int
resolve (
    zconfig_t *cfg_ptr,
    const char *pic,
    ...)
{
    assert (cfg_ptr);
    assert (pic);

    va_list arg_p;
    va_start (arg_p, pic);

    while (*pic) {
        char *key = va_arg (arg_p, char *);
        assert (key);

        if (*pic == 'i') {
            int *va_p = va_arg (arg_p, int *);
            assert (va_p);

            char *val = zconfig_resolve (cfg_ptr, key, NULL);
            if (!val) {
                return -1;
            }

            *va_p = atoi (val);
        }

        else if (*pic == 's') {
            char **va_p = va_arg (arg_p, char **);
            assert (va_p);

            char *val = zconfig_resolve (cfg_ptr, key, NULL);
            if (!val) {
                return -1;
            }

            *va_p = val;
        }

        else {
            assert (false);
        }

        pic += 1;
    }

    return 0;
}



static char
get_prefix (char *size_str)
{
    char *str_ptr = size_str;
    while (*str_ptr && '0' <= *str_ptr && *str_ptr <= '9') {
        str_ptr += 1;
    }

    if (str_ptr == size_str) {
        return 0;
    }

    char prefix = *str_ptr;
    *str_ptr = '\0';

    return prefix;
}


static uint64_t
conv_time_prefix (char *time_str)
{
    char prefix = get_prefix (time_str);
    uint64_t ms = atoi (time_str);

    if (prefix == 'M' || prefix == '\0') {
        // do nothing
    }
    else if (prefix == 's') {
        ms *= 1000;
    }
    else if (prefix == 'm') {
        ms *= 1000 * 60;
    }
    else if (prefix == 'h') {
        ms *= 1000 * 60 * 60;
    }
    else if (prefix == 'd') {
        ms *= 1000 * 60 * 60 * 24;
    }
    else {
        sam_log_errorf ("unknown time prefix: '%c'", prefix);
        return 0;
    }

    return ms;
}



static uint64_t
conv_binary_prefix (char *size_str)
{
    char prefix = get_prefix (size_str);

    int power;
    if (prefix == 'B' || prefix == '\0') {
        power = 0;
    }
    else if (prefix == 'K') {
        power = 1;
    }
    else if (prefix == 'M') {
        power = 2;
    }
    else if (prefix == 'G') {
        power = 3;
    }
    else {
        sam_log_errorf ("unknown binary prefix: '%c'", prefix);
        return 0;
    }

    uint64_t size = atoi (size_str);
    while (power) {
        size *= 1024;
        power -= 1;
    }

    return size;
}


static int
retrieve_time_value (sam_cfg_t *self, const char *path, uint64_t *val)
{
    char *str = zconfig_resolve (self->zcfg, path, NULL);

    if (str != NULL) {
        uint64_t rc = conv_time_prefix (str);
        if (rc != 0) {
            *val = rc;
            return 0;
        }
    }

    sam_log_error ("could not load retry threshold");
    return -1;
}


//  --------------------------------------------------------------------------
/// Create a new cfg instance.
sam_cfg_t *
sam_cfg_new (const char *cfg_file)
{
    assert (cfg_file);

    sam_cfg_t *self = malloc (sizeof (sam_cfg_t));
    assert (self);

    self->zcfg = zconfig_load (cfg_file);
    if (!self->zcfg) {
        sam_log_error ("could not load configuration");
        free (self);
        return NULL;
    }

    return self;
}


//  --------------------------------------------------------------------------
/// Destroy a cfg instance. Also clears all allocated memory needed to
/// store the configuration file.
void
sam_cfg_destroy (sam_cfg_t **self)
{
    assert (*self);
    zconfig_destroy (&(*self)->zcfg);

    free (*self);
    *self = NULL;
}


//  --------------------------------------------------------------------------
/// Retrieve the file name of the buffer.
int
sam_cfg_buf_file (sam_cfg_t *self, char **fname)
{
    assert (self);

    *fname = zconfig_resolve (self->zcfg, "buffer/file", NULL);
    if (*fname == NULL) {
        sam_log_error ("could not load buffer file name");
        return -1;
    }
    return 0;
}


//  --------------------------------------------------------------------------
/// Retrieve the maximum size of the buffer.
int
sam_cfg_buf_size (sam_cfg_t *self, uint64_t *size)
{
    assert (self);
    assert (size);

    char *size_str = zconfig_resolve (self->zcfg, "buffer/size", NULL);
    if (size_str != NULL) {
        uint64_t rc = conv_binary_prefix (size_str);
        if (rc != 0) {
            *size = rc;
            return 0;
        }
    }

    sam_log_error ("could not load buffer size");
    return -1;


}


//  --------------------------------------------------------------------------
/// Retrieve the buffers retry interval.
int
sam_cfg_buf_retry_interval (
    sam_cfg_t *self,
    uint64_t *interval)
{
    assert (self);
    assert (interval);

    return retrieve_time_value (
        self, "buffer/retries/interval", interval);
}


//  --------------------------------------------------------------------------
/// Retrieve the buffers retry threshold.
int
sam_cfg_buf_retry_threshold (
    sam_cfg_t *self,
    uint64_t *threshold)
{
    assert (self);
    assert (threshold);

    return retrieve_time_value (
        self, "buffer/retries/threshold", threshold);
}



//  --------------------------------------------------------------------------
/// Retrieve the public endpoint string. Used to bind a socket clients
/// can connect to.
int
sam_cfg_endpoint (sam_cfg_t *self, char **endpoint)
{
    assert (self);
    char *val = zconfig_resolve (self->zcfg, "/endpoint", NULL);

    if (val == NULL) {
        sam_log_error ("could not load endpoint");
        return -1;
    }

    *endpoint = val;
    return 0;
}


//  --------------------------------------------------------------------------
/// Retrieve the backend type. Used to determine what kind of
/// messaging backend to spawn and what configuration to expect.
int
sam_cfg_be_type (sam_cfg_t *self, sam_be_t *be_type)
{
    assert (self);
    assert (be_type);

    char *val = zconfig_resolve (self->zcfg, "backend/type", NULL);

    if (val == NULL) {
        sam_log_error ("could not load backend type");
        return -1;
    }

    if (!strcmp (val, "rmq")) {
        *be_type = SAM_BE_RMQ;
    }
    else {
        sam_log_errorf ("unknown backend type: '%s'", val);
        return -1;
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Load RabbitMQ specific configuration options used to spawn be_rmq
/// instances.
static int
read_backends_rmq (
    zconfig_t *cfg_ptr,
    int *be_c,
    char ***names_ptr,
    void **opts_ptr)
{
    size_t opt_s = sizeof (sam_be_rmq_opts_t);
    size_t str_s = sizeof (char *);

    sam_be_rmq_opts_t *opts = malloc (opt_s);
    assert (opts);

    char **names = malloc (str_s);
    assert (names);

    if (!opts) {
        assert (false);
    }

    *be_c = 0;
    while (cfg_ptr) {
        *be_c += 1;

        // expand opts buffer
        opts = realloc (opts, opt_s * *be_c);
        if (!opts) {
            assert (false);
        }

        // expand names buffer
        names = realloc (names, str_s * *be_c);
        if (!names) {
            assert (false);
        }
        names[*be_c - 1] = zconfig_name (cfg_ptr);

        sam_be_rmq_opts_t *be_opts = opts + (*be_c - 1);
        int rc = resolve (
            cfg_ptr, "sissi",
            "host", &(be_opts->host),
            "port", &(be_opts->port),
            "user", &(be_opts->user),
            "pass", &(be_opts->pass),
            "heartbeat", &(be_opts->heartbeat));

        if (rc) {
            free (opts);
            return rc;
        }

        cfg_ptr = zconfig_next (cfg_ptr);
    }

    *names_ptr = names;
    *opts_ptr = opts;
    return 0;
}


//  --------------------------------------------------------------------------
/// Creates a buffer containing an array of backend specific
/// configuration options.
int
sam_cfg_be_backends (
    sam_cfg_t *self,
    sam_be_t be_type,
    int *be_c,
    char ***names,
    void **opts)
{
    assert (self);

    zconfig_t *cfg_ptr = zconfig_locate (self->zcfg, "backend/backends");
    if (cfg_ptr == NULL) {
        return -1;
    }

    cfg_ptr = zconfig_child (cfg_ptr);
    if (cfg_ptr == NULL) {
        sam_log_error ("no backends provided");
        return -1;
    }

    int rc = 0;
    if (be_type == SAM_BE_RMQ) {
        rc = read_backends_rmq (cfg_ptr, be_c, names, opts);
    }
    else {
        sam_log_error ("unknown backend type");
        rc = -1;
    }

    return rc;
}
