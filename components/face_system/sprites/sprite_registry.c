#include "sprite_registry.h"
#include "sprite_vector.h"
#include "sprite_lineart.h"
#include "sprite_classic.h"
#include "sprite_cat.h"
#include "sprite_pixel.h"
#include "sprite_robot.h"
#include "sprite_nova.h"
#include "sprite_pig.h"
#include "sprite_chibi.h"

// Single source of truth for all available sprites.
// To add a new sprite: import its header and add it to this array.
static const sprite_set_t *const SPRITES[] = {
    &SPRITE_VECTOR,
    &SPRITE_LINEART,
    &SPRITE_CLASSIC,
    &SPRITE_CAT,
    &SPRITE_PIXEL,
    &SPRITE_ROBOT,
    &SPRITE_NOVA,
    &SPRITE_PIG,
    &SPRITE_CHIBI,
};
static const int SPRITE_COUNT = sizeof(SPRITES) / sizeof(SPRITES[0]);

int sprite_registry_count(void) {
    return SPRITE_COUNT;
}

const sprite_set_t *sprite_registry_get(int index) {
    if (SPRITE_COUNT == 0) return NULL;
    return SPRITES[index % SPRITE_COUNT];
}

const sprite_set_t *sprite_registry_default(void) {
    return SPRITES[0];
}
