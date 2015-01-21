/*  =========================================================================

    sam_prelude - internal header definitions

    TODO: LICENCE

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


#endif
