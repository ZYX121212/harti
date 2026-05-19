#ifndef FACE_RENDERER_H
#define FACE_RENDERER_H

#include "face_model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*face_post_line_cb_t)(int y, uint16_t *line_buf, int width);
extern face_post_line_cb_t face_post_line_cb;

void renderer_init(void);
void renderer_set_sprite(const sprite_set_t *sprite);
const sprite_set_t *renderer_get_sprite(void);
void renderer_render_frame(const face_state_t *st);

#ifdef __cplusplus
}
#endif

#endif
