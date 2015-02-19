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


static void
out (
    FILE *f,
    sam_log_lvl_t lvl,
    const char *fac,
    const char *msg)
{
    char date_buf[SAM_LOG_DATE_MAXSIZE];

    time_t time_curr = time (NULL);
    struct tm *time_loc = localtime (&time_curr);
    strftime (date_buf, SAM_LOG_DATE_MAXSIZE, "%T", time_loc);

    // format output string
    fprintf (
        f, "%s [%.*s] (%s): %.*s\n",
        date_buf,
        16, fac,
        get_lvl_repr (lvl),
        SAM_LOG_LINE_MAXSIZE, msg);
}


void
sam_log_ (
    sam_log_lvl_t lvl,
    const char *fac,
    const char *msg)
{

    if (lvl == SAM_LOG_LVL_TRACE || lvl == SAM_LOG_LVL_INFO) {
        out (stdout, lvl, fac, msg);
        return;
    }

    if (lvl == SAM_LOG_LVL_ERROR) {
        out (stderr, lvl, fac, msg);
        return;
    }

    assert (false);
}


void
sam_logf_ (
    sam_log_lvl_t lvl,
    const char *fac,
    const char *fmt,
    ...)
{
    va_list argp;
    char buf [SAM_LOG_LINE_MAXSIZE];

    va_start (argp, fmt);
    vsnprintf (buf, SAM_LOG_LINE_MAXSIZE - 1, fmt, argp);
    buf[SAM_LOG_LINE_MAXSIZE - 1] = 0;
    va_end (argp);

    sam_log_ (lvl, fac, buf);
}
