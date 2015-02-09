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

   Instances of this class communicate with a specific log
   facility. They must not be shared between threads, but
   arbitrary many instances of this class (one per thread)
   may communicate with one log facility.


   connections:
       "[<>^v]" : connect
            "o" : bind


               PUSH   PULL
   sam_logger[0] >-----o sam_log
                      /
      sam_logger[i] >/
                   PUSH

*/

#include "../include/sam_prelude.h"



//  --------------------------------------------------------------------------
/// Create a new logger instance.
/// Most of the time you need to provide the return value of
/// sam_log_endpoint (...)
sam_logger_t *
sam_logger_new (char *endpoint)
{
    sam_logger_t *logger = malloc (sizeof (sam_logger_t));
    logger->psh = zsock_new_push (endpoint);
    return logger;

}


//  --------------------------------------------------------------------------
/// Destroy a logger instance.
void
sam_logger_destroy (sam_logger_t **logger)
{
    assert (logger);
    zsock_destroy (&(*logger)->psh);
    free (*logger);
    logger = NULL;
}


//  --------------------------------------------------------------------------
/// Send a log line to the log facility.
/// The timestamp appearing in the log is created in this function.
void
sam_logger_send (
    sam_logger_t *logger,
    sam_log_lvl_t lvl,
    const char *line)
{
    assert (logger);
    time_t time_curr = time (NULL);

    zsock_send (
        logger->psh,
        "sbsb",
        "log",
        &lvl, sizeof (sam_log_lvl_t),
        line,
        &time_curr, sizeof (time_t));
}


//  --------------------------------------------------------------------------
/// Send a log line to the log facility.
/// The timestamp appearing in the log is created in this function.
void
sam_logger_sendf (
    sam_logger_t *logger,
    sam_log_lvl_t lvl,
    const char *fmt,
    ...)
{
    va_list argp;
    char line[256];

    va_start (argp, fmt);
    vsnprintf (line, 255, fmt, argp);
    line[255] = 0;
    va_end (argp);

    sam_logger_send (logger, lvl, line);
}
