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


typedef struct sam_backend_t sam_backend_t;


#include "sam.h"
#include "sam_log.h"
#include "sam_stat.h"
#include "sam_gen.h"
#include "sam_msg.h"
#include "sam_cfg.h"
#include "sam_be_rmq.h"
#include "sam_db.h"
#include "sam_buf.h"


#ifdef __SAM_TEST
#include "sam_selftest.h"
#endif


/// return type for "start" functions
/// of different message backends
struct sam_backend_t {
    // public interface
    char *name;          ///< name of the backend
    uint64_t id;         ///< id (power of 2) > 0

    zsock_t *sock_sig;   ///< socket for signaling state changes
    zsock_t *sock_pub;   ///< push messages to be published
    zsock_t *sock_rpc;   ///< request an rpc call

    // methods
    char *(*str) (sam_backend_t *be);  ///< return string representation

    // privates, do not touch!
    zactor_t *_actor;   ///< thread handling the broker connection
    void *_self;        ///< internally used reference to the original state
};


#endif
