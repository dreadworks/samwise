/*  =========================================================================

    sam_cfg - easy access to the configuration

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief public API
   @file sam.c

   TODO description

*/


#include "../include/sam_prelude.h"


sam_cfg_t *
sam_cfg_new (const char *cfg_file)
{
    assert (cfg_file);

    sam_cfg_t *self = malloc (sizeof (sam_cfg_t));
    assert (self);

    self->zcfg = zconfig_load (cfg_file);
    if (!self->zcfg) {
        sam_log_error ("could not load configuration");
        return NULL;
    }

    return self;
}


void
sam_cfg_destroy (sam_cfg_t **self)
{
    assert (*self);
    zconfig_destroy (&(*self)->zcfg);

    free (*self);
    *self = NULL;
}

