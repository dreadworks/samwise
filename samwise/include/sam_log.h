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

#ifdef __cplusplus
extern "C" {
#endif


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


//  --------------------------------------------------------------------------
/// @brief Log a line
/// @param msg String to be logged
/// @param filename Filename set by the preprocessor
/// @param line Line number set by the preprocessor
void
sam_log_ (
    sam_log_lvl_t lvl,
    const char *msg,
    const char *filename,
    const int line);


//  --------------------------------------------------------------------------
/// @brief Log a formatted line
/// @param fmt String containing a format
/// @param filename Filename set by the preprocessor
/// @param line Line number set by the preprocessor
/// @param ... Format arguments
void
sam_logf_ (
    sam_log_lvl_t lvl,
    const char *fmt,
    const char *filename,
    const int line,
    ...);


//  --------------------------------------------------------------------------
/// @brief Self test this class
void *
sam_log_test ();


//
//   PREPROCESSOR MACROS
//

// trace logging
#if                                             \
    defined(LOG_THRESHOLD_TRACE) ||             \
    defined(LOG_THRESHOLD_INFO)  ||             \
    defined(LOG_THRESHOLD_ERROR)

    #define sam_log_trace(msg)
    #define sam_log_tracef(msg, ...)

#else
    #define sam_log_trace(msg)                            \
        sam_log_ (SAM_LOG_LVL_TRACE, msg, __FILE__, __LINE__);
    #define sam_log_tracef(msg, ...)                              \
        sam_logf_ (SAM_LOG_LVL_TRACE, msg, __FILE__, __LINE__, __VA_ARGS__);

#endif


// info logging
#if defined(LOG_THRESHOLD_INFO) || defined(LOG_THRESHOLD_ERROR)
    #define sam_log_info(msg)
    #define sam_log_infof(msg, ...)

#else
    #define sam_log_info(msg)                     \
        sam_log_ (SAM_LOG_LVL_INFO, msg, __FILE__, __LINE__);
    #define sam_log_infof(msg, ...)                             \
        sam_logf_ (SAM_LOG_LVL_INFO,  msg, __FILE__, __LINE__, __VA_ARGS__);

#endif


// error logging
#if defined(LOG_THRESHOLD_ERROR)
    #define sam_log_error(msg)
    #define sam_log_errorf(msg, ...)

#else
    #define sam_log_error(msg)                    \
        sam_log_ (SAM_LOG_LVL_ERROR, msg, __FILE__, __LINE__);
    #define sam_log_errorf(msg, ...)                            \
        sam_logf_ (SAM_LOG_LVL_ERROR, msg, __FILE__, __LINE__, __VA_ARGS__);

#endif


#ifdef __cplusplus
}
#endif

#endif
