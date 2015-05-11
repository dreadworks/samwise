/*  =========================================================================

    sam_selftest - Generic functions for selftests

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief Generic functions for selftests.

*/

#ifndef __SAM_SELFTEST_H__
#define __SAM_SELFTEST_H__

#ifdef __cplusplus
extern "C" {
#endif


//  --------------------------------------------------------------------------
/// @brief Used to add a visual seperator between tests.
/// @param name Name of the test
void
sam_selftest_introduce (
    const char *name);


#ifdef __cplusplus
}
#endif

#endif
