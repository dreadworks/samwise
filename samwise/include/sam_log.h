/*  =========================================================================

    sam_log - logger facility for samwise

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   \brief sam_log - logger facility for samwise

    This is the interface for a logging facility that
    handles different log levels and different
    output channels.

*/


#ifndef _SAM_LOG_H_
#define _SAM_LOG_H_


/// The different severity levels for logging
typedef enum {
    SAM_LOG_LVL_ERROR,   ///< just output severe errors
    SAM_LOG_LVL_INFO,    ///< to get informed upon important events
    SAM_LOG_LVL_TRACE    ///< to track the program flow
} sam_log_lvl_t;


#define SAM_LOG_LVL_TRACE_REPR  "trace"
#define SAM_LOG_LVL_INFO_REPR   "info"
#define SAM_LOG_LVL_ERROR_REPR  "error"


// Everything above gets cut. Prevents
// buffer overflows, since no heap is used.
#define SAM_LOG_LINE_MAXSIZE 256
#define SAM_LOG_DATE_MAXSIZE 16


void
sam_log_ (
    sam_log_lvl_t lvl,
    const char *fac,
    const char *msg);


void
sam_logf_ (
    sam_log_lvl_t lvl,
    const char *fac,
    const char *msg,
    ...);


//
// TRACE LOGGING
//
#if defined(LOG_THRESHOLD_TRACE) || defined(LOG_THRESHOLD_INFO) || defined(LOG_THRESHOLD_ERROR)
    #define sam_log_trace(msg)
    #define sam_log_tracef(msg, __FILE__, ...)

#else
    #define sam_log_trace(msg)                            \
        sam_log_ (SAM_LOG_LVL_TRACE, msg, __FILE__);
    #define sam_log_tracef(msg, ...)                              \
        sam_logf_ (SAM_LOG_LVL_TRACE, msg, __FILE__, __VA_ARGS__);

#endif


//
// INFO LOGGING
//
#if defined(LOG_THRESHOLD_INFO) || defined(LOG_THRESHOLD_TRACE)
    #define sam_log_info(msg)
    #define sam_log_infof(msg, __FILE__, ...)

#else
    #define sam_log_info(msg)                     \
        sam_log_ (SAM_LOG_LVL_INFO, msg, __FILE__);
    #define sam_log_infof(msg, ...)                             \
        sam_logf_ (SAM_LOG_LVL_INFO,  msg, __FILE__, __VA_ARGS__);

#endif


//
// ERROR LOGGING
//
#if defined(LOG_THRESHOLD_ERROR)
    #define sam_log_error(msg)
    #define sam_log_errorf(msg, __FILE__, ...)

#else
    #define sam_log_error(msg)                    \
        sam_log_ (SAM_LOG_LVL_ERROR, msg, __FILE__);
    #define sam_log_errorf(msg, ...)                            \
        sam_logf_ (SAM_LOG_LVL_ERROR, msg, __FILE__, __VA_ARGS__);

#endif


#endif
