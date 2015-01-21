/*  =========================================================================

    samwise - reliable message publishing

    Copyright (c) 2015 XING AG, Felix Hamann

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT


    =========================================================================
*/
/**

   @brief API interface for sam

   TODO: description

*/

#ifndef __SAM_H__
#define __SAM_H__


#define SAM_VERSION_MAJOR 0
#define SAM_VERSION_MINOR 1
#define SAM_VERSION_PATCH 0

#define SAM_MAKE_VERSION(major, minor, patch) \
    ((major) *10000 + (minor) * 100 + (patch))

#define SAM_VERSION \
    SAM_MAKE_VERSION(SAM_VERSION_MAJOR, \
                     SAM_VERSION_MINOR, \
                     SAM_VERSION_PATCH)


//  --------------------------------------------------------------------------
/// @brief   This is just a test function.
/// @param   x  It just takes one parameter
/// @return  The return value is always x
int
sam_test (int x);


#endif
