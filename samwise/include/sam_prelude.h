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


#define UU __attribute__((unused))


#include <czmq.h>
#if CZMQ_VERSION < 20200
#  error "sam needs at least CZMQ 2.2.0"
#endif


#include "sam_log.h"
#include "sam_logger.h"

#endif
