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


// system dependencies
#include <sys/mman.h>


// external dependencies
#ifdef __SAM_TEST
#include <check.h>
#endif

#include <db.h>
#include <amqp.h>
#include <amqp_tcp_socket.h>

#include <czmq.h>
#if CZMQ_VERSION < 30000
#  error "sam needs at least CZMQ 3.0.0"
#endif


// compiler macros
#define UU __attribute__((unused))

// global configuration
#define SAM_PROTOCOL_VERSION 100

// don't define these to show all log levels
// #define LOG_THRESHOLD_TRACE   // show info + error
// #define LOG_THRESHOLD_INFO    // only show error
// #define LOG_THRESHOLD_ERROR   // disable logging


/// backend types
typedef enum {
    SAM_BE_RMQ     ///< RabbitMQ message backend
} sam_be_t;


/// return type for "start" functions
/// of different message backends
typedef struct sam_backend_t {
    // public interface
    char *name;            ///< name of the backend
    uint64_t id;           ///< id (power of 2) > 0
    zsock_t *publish_psh;  ///< push messages to be published
    zsock_t *rpc_req;      ///< request an rpc call

    // privates
    zactor_t *_actor;   ///< thread handling the broker connection
    void *_self;        ///< reference used by stop() give back control
} sam_backend_t;


#include "sam_log.h"
#include "sam_gen.h"
#include "sam_msg.h"
#include "sam_be_rmq.h"
#include "sam_cfg.h"
#include "sam_buf.h"
#include "sam.h"

#endif
