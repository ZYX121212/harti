#ifndef FACE_RENDERER_H
#define FACE_RENDERER_H

#include "face_model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void renderer_init(void);
void renderer_set_sprite(const sprite_set_t *sprite);
const sprite_set_t *renderer_get_sprite(void);
void renderer_render_frame(const face_state_t *st);

#ifdef __cplusplus
}
#endif

#endif
