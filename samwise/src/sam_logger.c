/*  =========================================================================

    sam_logger - logger classes to communicate with sam_log instances

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief distributed logger instances
   @file sam_logger.c
   
   TODO: doc

*/

#include "../include/sam_prelude.h"


sam_logger_t *
sam_logger_new (char *endpoint)
{
    sam_logger_t *logger = malloc (sizeof (sam_logger_t));
    logger->psh = zsock_new_push (endpoint);
    return logger;
    
}

void
sam_logger_destroy (sam_logger_t **logger)
{
    assert (logger);
    zsock_destroy (&(*logger)->psh);
    free (*logger);
    logger = NULL;
}


void
sam_logger_send (
    sam_logger_t *logger,
    sam_log_lvl_t lvl,
    const char *line)
{
    time_t time_curr = time (NULL);

    zsock_send (
        logger->psh,
        "sbsb",
        "log",
        &lvl, sizeof (sam_log_lvl_t),
        line,
        &time_curr, sizeof (time_t));
}
