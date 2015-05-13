/*  =========================================================================

    samwise - best effort store and forward message publishing

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief samwise c client
   @file samwise.c


*/


#include <time.h>
#include <stdbool.h>
#include <argp.h>
#include <inttypes.h>
#include "../include/samwise.h"


/// arguments provided by the user
typedef struct args_t {
    bool verbose;               ///< print additional information
    bool quiet;                 ///< suppress all output

    const char *endpoint;       ///< public endpoint to connect to
    const char *action;         ///< positional action string

    // publishing options
    int n;                         ///< number of messages to be published
    samwise_disttype_t disttype;   ///< distribution type (roundrobin|redundant)
    int d;                         ///< count for distribution=redundant

} args_t;


/// state maintained by the client
typedef struct state_t {
    samwise_t *sam;          ///< samwise client instance
    args_t *args;            ///< arguments provided by the user
} state_t;



/*
 *   ---- HELPER ----
 *
 */

/// output levels
typedef enum out_t {
    NORMAL,    ///< print to stdout unless -q
    ERROR,     ///< print to stderr unless -q
    VERBOSE    ///< print to stdout if -v
} out_t;


//  --------------------------------------------------------------------------
/// Print a message to std* based on the level and either -q or -v.
static void
out (
    out_t lvl,
    args_t *args,
    const char *line)
{
    if (
        (lvl == VERBOSE && args->verbose) ||
        (lvl == NORMAL && !args->quiet)) {

        fprintf (stdout, "%s\n", line);
    }

    if (lvl == ERROR && !args->quiet) {
        fprintf (stderr, "error: %s\n", line);
    }

}


//  --------------------------------------------------------------------------
/// Returns current monotonic timestamp in ms.
static int64_t
clock_mono ()
{
    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);

    return (int64_t)
        ((int64_t) ts.tv_sec * 1000 + (int64_t) ts.tv_nsec / 1000000);
}



//  --------------------------------------------------------------------------
/// Publish n messages to samd.
static void
publish (
    state_t *state)
{
    assert (state);
    args_t *args = state->args;

    int count = 1;
    int64_t ts = clock_mono ();

    char buf [256];

    while (count <= args->n) {
        char buf [64];
        snprintf (buf, 64, "message no %d", count);

        samwise_pub_t pub = {
            .disttype = args->disttype,
            .distcount = args->d,

            .exchange = "amq.direct",
            .routing_key = "",

            .size = strlen (buf),
            .msg = buf
        };

        sprintf (buf, "publishing message %d", count);
        out (VERBOSE, state->args, buf);

        int rc = samwise_publish (state->sam, &pub);

        if (rc) {
            fprintf (stderr, "publishing failed\n");
        }

        count += 1;
    }


    char *fmt = "publishing took %" PRId64 "ms";
    sprintf (buf, fmt, clock_mono () - ts);
    out (NORMAL, state->args, buf);
}



/*
 *    ---- ARGP ----
 *
 */
const char
    *argp_program_version = "0.1",
    *argp_program_bug_adress = "http://github.com/dreadworks/samwise/issues";


static char doc [] =
    " ___ __ _ _ ____ __ _(_)___ ___   __| (_)___ _ _| |_\n"
    "(_-</ _` | '  \\ V  V / (_-</ -_) / _| | / -_) ' \\  _|\n"
    "/__/\\__,_|_|_|_\\_/\\_/|_/__/\\___| \\__|_|_\\___|_||_\\__|\n\n"

    "Currently the following actions are supported:\n"
    "  ping     Ping a samd instance\n"
    "  publish  Publish some messages to samd\n"

    "\nAdditionally, the following options can be provided:\n";


static char args_doc [] = "ACTION";


// possible options
static struct argp_option options [] = {

    { .name = "verbose", .key = 'v', .arg = 0,
      .flags = OPTION_ARG_OPTIONAL,
      .doc = "Verbose output" },

    { .name = "quiet", .key = 'q', .arg = 0,
      .flags = OPTION_ARG_OPTIONAL,
      .doc = "Suppress output" },

    { .name = "endpoint", .key = 'e', .arg = "ENDPOINT",
      .doc = "Public endpoint of samd" },

    // for ACTION = publish
    { .name = "number", .key = 'n', .arg = "N",
      .doc = "Number of messages to be published (default: 1)" },

    { .name = "type", .key = 't', .arg = "TYPE",
      .doc = "Distribution type (roundrobin|redundant) "
             "(default: roundrobin)" },

    { .name = "distcount", .key = 'd', .arg = "D",
      .doc = "Count for distribution=redundant (default: 2)" },


    // excond
    { 0 }

};


//  --------------------------------------------------------------------------
/// Parse the argument vector.
static error_t
parse_opt (
    int key,
    char *arg,
    struct argp_state *a_state)
{
    args_t *args = a_state->input;


    switch (key) {

    // verbose
    case 'v':
        if (args->quiet) {
            out (ERROR, args, "-q and -v are mutually exclusive");
            argp_usage (a_state);
            return -1;
        }

        args->verbose = true;
        out (VERBOSE, args, "setting output verbose");
        break;

    // quiet
    case 'q':
        if (args->verbose) {
            out (ERROR, args, "-q and -v are mutually exclusive");
            argp_usage (a_state);
            return -1;
        }

        args->quiet = true;
        break;

    // endpoint
    case 'e':
        args->endpoint = arg;
        break;


    /*
     *    for ACTION = publish
     *
     */

    // number
    case 'n':
        args->n = atoi (arg);
        break;

    // distribution type
    case 't':
        if (!strcmp (arg, "roundrobin")) {
            args->disttype = SAMWISE_ROUNDROBIN;
            out (VERBOSE, args, "publishing in a round robin fashion");
        }
        else if (!strcmp (arg, "redundant")) {
            args->disttype = SAMWISE_REDUNDANT;
            out (VERBOSE, args, "publishing redundantly");
        }
        else {
            out (ERROR, args, "unknown distribution type");
        }
        break;


    // distribution count
    case 'd':
        args->d = atoi (arg);
        break;

    /*
     *    ACTION
     *
     */

    // key args (action)
    case ARGP_KEY_ARG:
        if (a_state->arg_num >= 1) {
            out (ERROR, args, "too many arguments");
            argp_usage (a_state);
            return -1;
        }

        args->action = arg;
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}


/// configures argp
static struct argp argp = {
    .options = options,
    .parser = parse_opt,
    .args_doc = args_doc,
    .doc = doc,
    .children = 0
};



static int
eval (
    state_t *state)
{
    const char *action = state->args->action;

    if (!strcmp (action, "ping")) {
        samwise_ping (state->sam);
    }

    else if (!strcmp (action, "publish")) {
        publish (state);
    }

    else {
        return -1;
    }

    return 0;
}



//  --------------------------------------------------------------------------
/// Entry point.
int
main (
    int argc,
    char **argv)
{

    args_t args = {
        .quiet = false,
        .verbose = false,

        .action = NULL,

        .endpoint = "",
        .n = 1,
        .disttype = SAMWISE_ROUNDROBIN,
        .d = 2
    };

    error_t rc = argp_parse (&argp, argc, argv, 0, 0, &args);

    if (rc) {
        out (ERROR, &args, "argument error");
        return EXIT_FAILURE;
    }


    state_t state = {
        .sam = samwise_new (args.endpoint),
        .args = &args
    };


    if (!state.sam || eval (&state)) {
        out (ERROR, &args, "an error occured, exiting\n");
        samwise_destroy (&state.sam);
        return EXIT_FAILURE;
    }

    samwise_destroy (&state.sam);
    return rc;
}
