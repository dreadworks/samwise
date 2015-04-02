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
p


/*
 *    ---- CMD ----
 *
 */

//  --------------------------------------------------------------------------
/// Generic function to send a message to samd.
static zmsg_t *
send_cmd (const char *cmd_name)
{
    int rc = zsock_send (
        ctl->sam_sock, "is", SAM_PROTOCOL_VERSION, cmd_name);

    if (rc) {
        out (ERROR, args, "could not send command");
        return NULL;
    }

    zsock_set_rcvtimeo (ctl->sam_sock, 1000);

    int reply_code;
    char *reply_msg;
    rc = zsock_recv (
        ctl->sam_sock, "is", &reply_code, &reply_msg);

    if (rc) {
        out (
            ERROR, args, "could not receive answer (interrupt or timeout)");
        return NULL;
    }

    if (reply_code) {
        out (ERROR, args, reply_msg);
        free (reply_msg);
        return NULL;
    }
}



//  --------------------------------------------------------------------------
/// Ping samwise.
static void
cmd_ping (
    ctl_t *ctl,
    args_t *args)
{
    if (!send_cmd ("ping")) {
        out (NORMAL, args, "pong");
    }
}


static void
cmd_status (
    ctl_t *ctl UU,
    args_t *args UU)
{
    printf ("TODO\n");
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

    return args;
}


//  --------------------------------------------------------------------------
/// Free all memory allocated by the args object.
static void
args_destroy (
    args_t **args)
{
    assert (*args);

    if (&(*args)->cfg) {
        sam_cfg_destroy (&(*args)->cfg);
    }

    free (*args);
    *args = NULL;
}


// --- do not edit, set by tools/release.fish
const char
    *argp_program_version = "TODO release.fish",
    *argp_program_bug_address = "TODO release.fish";
// ---


static char doc [] =
    "\n======================================================================\n"
    " * * * * Samwise Control Interface -- samctl * * * *\n"
    "======================================================================\n\n"
    "Send a command with the option -c or --cmd=CMD.\n"
    "Currently the following commands are supported:\n"
    "  ping      Ping samwise\n"
    "  status    Get extensive status information about samd's state"

    "\n\nAdditionally the following options can be provided:\n";

static char args_doc [] =
    "CONFIG";


/// possible options to pass to samctl
static struct argp_option options [] = {

    { .name = "verbose", .key = 'v', .arg = 0,
      .doc = "Verbose output" },

    { .name = "quiet", .key = 'q', .arg = 0,
      .doc = "Suppress output" },

    { .name = "command", .key = 'c', .arg = "CMD",
      .doc = "Specify the command to send to samwise"
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

    // cmd
    case 'c':
        set_cmd (args, arg);
        if (args->fn == NULL) {
            out (ERROR, args, "unknown command");
            argp_usage (state);
            return -1;
        }
        break;

    // key args (config)
    case ARGP_KEY_ARG:
        if (state->arg_num >= 1) {
            out (ERROR, args, "too many arguments");
            argp_usage (state);
            return -1;
        }

        out (VERBOSE, args, "loading configuration");
        args->cfg = sam_cfg_new (arg);
        if (args->cfg == NULL) {
            out (ERROR, args, "could not load config file");
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
    if (sam_cfg_endpoint (args->cfg, &endpoint)) {
        out (ERROR, args, "could not load endpoint");
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
        args->fn (ctl, args);
        ctl_destroy (&ctl);
    }

    out (VERBOSE, args, "exiting");
    args_destroy (&args);
    return rc;
}
