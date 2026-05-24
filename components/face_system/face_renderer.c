#include "face_renderer.h"
#include "gc9a01.h"

#define SCREEN_W 240
#define SCREEN_H 240

static uint16_t line_buf[SCREEN_W];
static const sprite_set_t *current_sprite = NULL;

void renderer_init(void) {
    current_sprite = NULL;
}

void renderer_set_sprite(const sprite_set_t *sprite) {
    current_sprite = sprite;
}

const sprite_set_t *renderer_get_sprite(void) {
    return current_sprite;
}

void renderer_render_frame(const face_state_t *st) {
    if (!current_sprite) return;
    const sprite_set_t *sp = current_sprite;

    gc9a01_set_window(0, 0, SCREEN_W - 1, SCREEN_H - 1);

    for (int y = 0; y < SCREEN_H; y++) {
        // z-order compositing (8 passes)
        sp->draw_face(y, st, sp, line_buf);
        sp->draw_blush(y, st, sp, line_buf);
        sp->draw_mouth(y, st, sp, line_buf);
        sp->draw_eye_left(y, st, sp, line_buf);
        sp->draw_eye_right(y, st, sp, line_buf);
        sp->draw_brow_left(y, st, sp, line_buf);
        sp->draw_brow_right(y, st, sp, line_buf);
        sp->draw_decor_overlay(y, st, sp, line_buf);
        if (sp->draw_props)
            sp->draw_props(y, st, sp, line_buf);

        gc9a01_send_pixels(line_buf, SCREEN_W);
    }
}
