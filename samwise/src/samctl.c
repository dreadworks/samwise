/*  =========================================================================

    samctl - samwise control interface

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief samwise control interface
   @file samctl.c

   Command line interface to control samwise.

*/


#include <argp.h>
#include "../include/sam_prelude.h"


#define SAM_PROTOCOL_VERSION 100


typedef struct args_t args_t;
typedef struct ctl_t ctl_t;
typedef void (cmd_fn) (ctl_t *ctl, args_t *args);


/// argparse object
struct args_t {
    bool verbose;      ///< output additional information
    bool quiet;        ///< suppress output
    cmd_fn *fn;        ///< command function
    sam_cfg_t *cfg;    ///< samwise configuration
    char *endpoint;
};


/// control object
struct ctl_t {
    zsock_t *sam_sock;   ///< socket to communicate with samd
};



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



/*
 *    ---- CMD ----
 *
 */

//  --------------------------------------------------------------------------
/// Generic function to send a message to samd.
static sam_msg_t *
send_cmd (
    ctl_t *ctl,
    args_t *args,
    const char *cmd_name)
{
    out (VERBOSE, args, "sending command to samd");

    int rc = zsock_send (
        ctl->sam_sock, "is", SAM_PROTOCOL_VERSION, cmd_name);

    if (rc) {
        out (ERROR, args, "could not send command");
        return NULL;
    }

    zsock_set_rcvtimeo (ctl->sam_sock, 1000);
    zmsg_t *zmsg = zmsg_recv (ctl->sam_sock);

    if (zmsg == NULL) {
        out (
            ERROR, args, "could not receive answer (interrupt or timeout)");
        return NULL;
    }

    sam_msg_t *msg = sam_msg_new (&zmsg);
    int reply_code;
    rc = sam_msg_pop (msg, "i", &reply_code);
    assert (!rc);

    if (reply_code) {
        char *err_msg;
        rc = sam_msg_pop (msg, "s", &err_msg);
        assert (!rc);

        out (ERROR, args, err_msg);
        sam_msg_destroy (&msg);
        return NULL;
    }

    return msg;
}


//  --------------------------------------------------------------------------
/// Ping samd.
static void
cmd_ping (
    ctl_t *ctl,
    args_t *args)
{
    sam_msg_t *msg = send_cmd (ctl, args, "ping");
    if (msg) {
        out (NORMAL, args, "pong");
        sam_msg_destroy (&msg);
    }
}


//  --------------------------------------------------------------------------
/// Order samd to kill itself.
static void
cmd_stop (
    ctl_t *ctl,
    args_t *args)
{
    sam_msg_t *msg = send_cmd (ctl, args, "stop");
    if (msg) {
        out (NORMAL, args, "samd kills itself");
        sam_msg_destroy (&msg);
    }
}


//  --------------------------------------------------------------------------
/// Order samd to restart itself.
static void
cmd_restart (
    ctl_t *ctl,
    args_t *args)
{
    sam_msg_t *msg = send_cmd (ctl, args, "restart");
    if (msg) {
        out (NORMAL, args, "samd restarts");
        sam_msg_destroy (&msg);
    }
}


//  --------------------------------------------------------------------------
/// Retrieve status data from samd.
static void
cmd_status (
    ctl_t *ctl,
    args_t *args)
{
    sam_msg_t *msg = send_cmd (ctl, args, "status");
    if (msg) {
        char *status;
        int rc = sam_msg_pop (msg, "s", &status);
        assert (rc == 0);

        out (NORMAL, args, status);
        sam_msg_destroy (&msg);
    }
}



/*
 *    ---- ARGP ----
 *
 */

//  --------------------------------------------------------------------------
/// Create a new args object. Gets its values set by parse_opt.
static args_t *
args_new ()
{
    args_t *args = malloc (sizeof (args_t));
    assert (args);

    args->quiet = false;
    args->verbose = false;
    args->fn = NULL;
    args->cfg = NULL;
    args->endpoint = NULL;

    return args;
}


//  --------------------------------------------------------------------------
/// Free all memory allocated by the args object.
static void
args_destroy (
    args_t **args)
{
    assert (*args);

    if ((*args)->cfg) {
        sam_cfg_destroy (&(*args)->cfg);
    }

    free (*args);
    *args = NULL;
}


// --- do not edit, set by tools/release.fish
const char
    *argp_program_version = "0.0.1",
    *argp_program_bug_address = "http://github.com/dreadworks/samwise/issues";
// ---


static char doc [] =
    " ___ __ _ _ ____ __ _(_)___ ___   __ ___ _ _| |_ _ _ ___| |\n"
    "(_-</ _` | '  \\ V  V / (_-</ -_) / _/ _ \\ ' \\  _| '_/ _ \\ |\n"
    "/__/\\__,_|_|_|_\\_/\\_/|_/__/\\___| \\__\\___/_||_\\__|_| \\___/_|\n\n"

    "Currently the following commands are supported:\n"
    "  ping      Ping samwise\n"
    "  status    Get extensive status information about samd's state\n"
    "  stop      Order samd to kill itself\n"
    "  restart   Restart samd\n"

    "\nAdditionally the following options can be provided:\n";

static char args_doc [] = "COMMAND";


/// possible options to pass to samctl
static struct argp_option options [] = {

    { .name = "verbose", .key = 'v', .arg = 0,
      .doc = "Verbose output" },

    { .name = "quiet", .key = 'q', .arg = 0,
      .doc = "Suppress output" },

    { .name = "config", .key = 'c', .arg = "CFG",
      .doc = "Specify a configuration file (or use -e)"
    },

    { .name = "endpoint", .key = 'e', .arg = "ENDPOINT",
      .doc = "Specify the endpoint (or use -c)"
    },

    { 0 }
};


//  --------------------------------------------------------------------------
/// Determine the desired command function.
static void
set_cmd (
    args_t *args,
    const char *fn_name)
{
    if (!strcmp (fn_name, "ping")) {
        args->fn = cmd_ping;
    }

    if (!strcmp (fn_name, "status")) {
        args->fn = cmd_status;
    }

    if (!strcmp (fn_name, "stop")) {
        args->fn = cmd_stop;
    }

    if (!strcmp (fn_name, "restart")) {
        args->fn = cmd_restart;
    }
}


//  --------------------------------------------------------------------------
/// Parse the argument vector.
static error_t
parse_opt (
    int key,
    char *arg UU,
    struct argp_state *state)
{
    args_t *args = state->input;

    switch (key) {

    // verbose
    case 'v':
        if (args->quiet) {
            out (ERROR, args, "-q and -v are mutually exclusive");
            argp_usage (state);
            return -1;
        }

        args->verbose = true;
        out (VERBOSE, args, "setting output verbose");
        break;

    // quiet
    case 'q':
        if (args->verbose) {
            out (ERROR, args, "-q and -v are mutually exclusive");
            argp_usage (state);
            return -1;
        }

        args->quiet = true;
        break;

    // cfg
    case 'c':
        out (VERBOSE, args, "loading configuration");
        args->cfg = sam_cfg_new (arg);
        if (args->cfg == NULL) {
            out (ERROR, args, "could not load config file");
            return -1;
        }
        break;


    // endpoint
    case 'e':
        out (VERBOSE, args, "using custom endpoint");
        args->endpoint = arg;
        break;

    // key gargs (command)
    case ARGP_KEY_ARG:
        if (state->arg_num >= 1) {
            out (ERROR, args, "too many arguments");
            argp_usage (state);
            return -1;
        }

        set_cmd (args, arg);
        if (args->fn == NULL) {
            out (ERROR, args, "unknown command");
            argp_usage (state);
            return -1;
        }

        break;

    case ARGP_KEY_END:
        if (state->arg_num < 1) {
            out (ERROR, args, "not enough arguments");
            argp_usage (state);
            return -1;
        }
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



/*
 *    ---- CTL ----
 *
 */

//  --------------------------------------------------------------------------
/// Create a new control object.
static ctl_t *
ctl_new (
    args_t *args)
{
    ctl_t *ctl = malloc (sizeof (ctl_t));
    assert (ctl);

    char *endpoint;

    if (args->endpoint) {
        endpoint = args->endpoint;
    }
    else if (args->cfg) {
        if (sam_cfg_endpoint (args->cfg, &endpoint)) {
            out (ERROR, args, "could not load endpoint");
            goto abort;
        }
    }
    else {
        out (ERROR, args, "no endpoint provided, try -e or -c");
        goto abort;
    }

    ctl->sam_sock = zsock_new_req (endpoint);
    if (ctl->sam_sock == NULL) {
        out (ERROR, args, "could not establish connection");
        goto abort;
    }

    return ctl;


abort:
    free (ctl);
    return NULL;
}


//  --------------------------------------------------------------------------
/// Close all connections and free all control object memory.
static void
ctl_destroy (
    ctl_t **ctl)
{
    assert (*ctl);

    zsock_destroy (&(*ctl)->sam_sock);

    free (*ctl);
    *ctl = NULL;
}



//  --------------------------------------------------------------------------
/// Evaluates its argument vector and interacts with a running samd
/// instance.
int
main (
    int argc,
    char **argv)
{
    args_t *args = args_new ();
    error_t rc = argp_parse (&argp, argc, argv, 0, 0, args);

    if (rc) {
        out (ERROR, args, "Argument error");
        rc = EXIT_FAILURE;
    }

    else {
        ctl_t *ctl = ctl_new (args);
        if (ctl) {
            args->fn (ctl, args);
            ctl_destroy (&ctl);
        }
    }

    out (VERBOSE, args, "exiting");
    args_destroy (&args);
    return rc;
}
