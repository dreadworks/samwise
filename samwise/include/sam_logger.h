/*  =========================================================================

    sam_logger - logger clients

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   \brief sam_logger - logger clients

   Instances of this class may be used to send
   the central log facility messages.

*/


#ifndef _SAM_LOGGER_H_
#define _SAM_LOGGER_H_


//  --------------------------------------------------------------------------
/// @brief   Create a new logger instance
/// @param   endpoint The log facilities endpoint
/// @return  The new logger instance
CZMQ_EXPORT sam_logger_t *
sam_logger_new (char *endpoint);


//  --------------------------------------------------------------------------
/// @brief   Destroy the logger
/// @param   logger Logger instance
CZMQ_EXPORT void
sam_logger_destroy (sam_logger_t **logger);



//  --------------------------------------------------------------------------
/// @brief   Send a log line
/// @param   logger Logger instance
/// @param   lvl Severity of the log message
/// @param   line A zero-terminated string
CZMQ_EXPORT void
sam_logger_send (
    sam_logger_t *logger,
    sam_log_lvl_t lvl,
    const char *line);


#endif
