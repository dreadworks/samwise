/*  =========================================================================

    sam_prelude - internal header definitions

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   \brief sam_prelude - internal header definitions

    This file accumulates necessary includes and provides project wide
    definition of preprocessor statements. It may be included by any
    file of this project.

*/

#ifndef __SAM_PRELUDE_H__
#define __SAM_PRELUDE_H__

#include <czmq.h>
#if CZMQ_VERSION < 20200
#  error "sam needs at least CZMQ 2.2.0"
#endif


#include <amqp.h>
#include <amqp_tcp_socket.h>

// compiler macros
#define UU __attribute__((unused))


// don't define these to show all log levels
// #define LOG_THRESHOLD_TRACE   // show info + error
// #define LOG_THRESHOLD_INFO    // only show error
// #define LOG_THRESHOLD_ERROR   // disable logging


// TODO implement shared config (#32)
#define SAM_LOG_ENDPOINT "inproc://log"
#define SAM_PUBLIC_ENDPOINT "ipc://../sam_ipc"


#include "sam_gen.h"
#include "sam_log.h"
#include "sam_msg.h"
#include "sam_msg_rabbitmq.h"
#include "sam.h"

#include "playground.h"  // to be removed (#40)

#endif
