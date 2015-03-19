/*  =========================================================================

    sam_log - a logging facility

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief the central logging facility
   @file sam_log.c

   TODO description

*/

#include "../include/sam_prelude.h"


//  --------------------------------------------------------------------------
/// Returns the string representation of a specific log level.
const char *
get_lvl_repr (sam_log_lvl_t lvl)
{
    switch (lvl) {
    case (SAM_LOG_LVL_TRACE):
        return SAM_LOG_LVL_TRACE_REPR;
    case (SAM_LOG_LVL_INFO):
        return SAM_LOG_LVL_INFO_REPR;
    case (SAM_LOG_LVL_ERROR):
        return SAM_LOG_LVL_ERROR_REPR;
    }

    assert (false);
}



//  --------------------------------------------------------------------------
/// Print log line to the designated FILE.
static void
out (
    FILE *f,
    sam_log_lvl_t lvl,
    const char *prefix,
    const char *msg,
    const char *filename,
    const int line)
{
    char date_buf[SAM_LOG_DATE_MAXSIZE];

    time_t time_curr = time (NULL);
    struct tm *time_loc = localtime (&time_curr);
    strftime (date_buf, SAM_LOG_DATE_MAXSIZE, "%T", time_loc);

    // format output string
    fprintf (
        f, "%s %s [%.*s:%d] (%s): %.*s\n",
        prefix,
        date_buf,
        16, filename,
        line,
        get_lvl_repr (lvl),
        SAM_LOG_LINE_MAXSIZE, msg);
}


//  --------------------------------------------------------------------------
/// This function is used by the preprocessor to replace sam_log_* calls.
void
sam_log_ (
    sam_log_lvl_t lvl,
    const char *msg,
    const char *filename,
    const int line)
{

    if (lvl == SAM_LOG_LVL_TRACE) {
        out (stdout, lvl, "\033[0m", msg, filename, line);
        return;
    }

    if (lvl == SAM_LOG_LVL_INFO) {
        out (stdout, lvl, "\x1B[33m", msg, filename, line);
        return;
    }

    if (lvl == SAM_LOG_LVL_ERROR) {
        out (stderr, lvl, "\x1B[31m", msg, filename, line);
        return;
    }

    assert (false);
}


//  --------------------------------------------------------------------------
/// This function is used by the preprocessor to replace sam_log_*f calls.
void
sam_logf_ (
    sam_log_lvl_t lvl,
    const char *fmt,
    const char *filename,
    const int line,
    ...)
{
    va_list argp;
    char buf [SAM_LOG_LINE_MAXSIZE];

    va_start (argp, line);
    vsnprintf (buf, SAM_LOG_LINE_MAXSIZE - 1, fmt, argp);
    buf[SAM_LOG_LINE_MAXSIZE - 1] = 0;
    va_end (argp);

    sam_log_ (lvl, buf, filename, line);
}
