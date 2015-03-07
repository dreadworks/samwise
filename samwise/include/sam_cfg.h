/*  =========================================================================

    sam_cfg - easy access to the configuration

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief Loads samwise configuration files

*/

#ifndef __SAM_CFG_H__
#define __SAM_CFG_H__


typedef struct sam_cfg_t {
    zconfig_t *zcfg;
} sam_cfg_t;



sam_cfg_t *
sam_cfg_new (const char *cfg_file);


void
sam_cfg_destroy (sam_cfg_t **self);


void *
sam_cfg_test ();


#endif
