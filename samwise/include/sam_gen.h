/*  =========================================================================

    samwise - reliable message publishing

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT


    =========================================================================
*/
/**

   @brief Generic functions for reuse

*/

#ifndef __SAM_GEN_H__
#define __SAM_GEN_H__


//  --------------------------------------------------------------------------
/// @brief Generic pipe handler for zactors reacting to interrupts and $TERM
/// @return -1 in case of termination, 0 otherwise
int
sam_gen_handle_pipe (
    zloop_t *loop,
    zsock_t *pipe,
    void *args);


//  --------------------------------------------------------------------------
/// @brief Self test this file
void
sam_gen_test ();


#endif
