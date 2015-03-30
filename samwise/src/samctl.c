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

typedef enum out_t {
    NORMAL,
    VERBOSE
} out_t;



/*
 *    ARGUMENT PARSING
 *
 */
typedef enum cmd_t {
    CMD_NONE,
    CMD_PING
} cmd_t;

typedef struct args_t {
    bool verbose;
    bool quiet;
    cmd_t cmd;
} args_t;


static args_t *
args_new () {
    args_t *args = malloc (sizeof (args_t));
    args->quiet = false;
    args->verbose = false;
    args->cmd = CMD_NONE;
    return args;
}


static void
args_destroy (args_t **args)
{
    assert (*args);
    free (*args);
    *args = NULL;
}


// --- do not edit, set by tools/release.fish
const char
    *argp_program_version = "TODO release.fish",
    *argp_program_bug_address = "TODO release.fish";
// ---

static char doc [] =
    "Samwise Control Interface -- samctl";

static char args_doc [] =
    "CONFIG";


static struct argp_option options [] = {

    { .name = "verbose", .key = 'v', .arg = 0,
      .doc = "Verbose output" },

    { .name = "quiet", .key = 'q', .arg = 0,
      .doc = "Suppress output" },

    { .name = "ping", .key = 'p', .arg = 0,
      .doc = "Ping samwise" },

/*
    { .name = "cmd", .key = 'c', .arg = "CMD", .flags = 0,
      .doc = "Command to send to samwise" },
*/
    { 0 }
};


static error_t
parse_opt (
    int key,
    char *arg UU,
    struct argp_state *state)
{
    args_t *args = state->input;

    switch (key) {
    case 'v':
        if (args->quiet) {
            printf ("error: -q and -v are mutually exclusive\n");
            argp_usage (state);
            return -1;
        }

        args->verbose = true;
        break;

    case 'q':
        if (args->verbose) {
            printf ("error: -q and -v are mutually exclusive\n");
            argp_usage (state);
            return -1;
        }

        args->quiet = true;
        break;


    case 'p':
        args->cmd = CMD_PING;
        break;
    }

    return 0;
}


static struct argp argp = {
    .options = options,
    .parser = parse_opt,
    .args_doc = args_doc,
    .doc = doc,
    .children = 0
};




/*
 *    CTL
 *
 */
static void
out (
    out_t lvl,
    args_t *args,
    const char *line)
{
    if (
        (lvl == VERBOSE && args->verbose) ||
        (lvl == NORMAL && !args->quiet)) {

        printf ("%s", line);
    }
}


static int
eval (args_t *args)
{
    switch (args->cmd)
    {
    case CMD_PING:
        out (VERBOSE, args, "ping\n");
        break;

    default:
        assert (false);
    }

    return 0;
}


int
main (
    int argc,
    char **argv)
{
    args_t *args = args_new ();
    error_t rc = argp_parse (&argp, argc, argv, 0, 0, args);

    if (rc || args->cmd == CMD_NONE) {
        fprintf (stderr, "Argument error\n");
        rc = EXIT_FAILURE;
    }
    else {
        rc = eval (args);
    }

    out (VERBOSE, args, "exiting.\n");
    args_destroy (&args);
    return rc;
}
