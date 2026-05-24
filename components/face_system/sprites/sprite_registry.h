#ifndef SPRITE_REGISTRY_H
#define SPRITE_REGISTRY_H

#include "face_model.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns number of available sprites
int sprite_registry_count(void);

// Get sprite by index (wraps around)
const sprite_set_t *sprite_registry_get(int index);

// Get the default sprite (first in list)
const sprite_set_t *sprite_registry_default(void);

#ifdef __cplusplus
}
#endif

#endif
