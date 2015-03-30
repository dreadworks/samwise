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




/*
 *    ARGUMENT PARSING
 *
 */

// set by tools/release.fish
const char
    *argp_program_version = "TODO release.fish",
    *argp_program_bug_address = "TODO release.fish";

static char doc [] =
    "Samwise Control Interface -- samctl";

static struct argp argp = {
    .options = 0,
    .parser = 0,
    .args_doc = 0,
    .doc = doc,
    .children = 0
};


int
main (int argc, char **argv)
{
    int rc = argp_parse (&argp, argc, argv, 0, 0, 0);
    if (rc == EINVAL) {
        printf ("unknown option encountered\n");
        rc = EXIT_FAILURE;
    }


    printf ("exiting.\n");
    return rc;
}
