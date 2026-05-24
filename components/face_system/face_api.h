#ifndef FACE_API_H
#define FACE_API_H

#include "face_model.h"
#include "face_animator.h"
#include "face_renderer.h"
#include "face_micro.h"
#include "face_prop.h"
#include "sprites/sprite_registry.h"

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void face_init(void) {
    animator_init();
    renderer_init();
    micro_animator_init();
    prop_animator_init();
    renderer_set_sprite(sprite_registry_default());
}

static inline void face_set_expression(expression_id_t id) {
    animator_set_expression(id);
}

static inline void face_set_component(face_component_t comp,
                                      const void *target_params,
                                      uint32_t duration_ms,
                                      uint32_t delay_ms,
                                      anim_path_type_t path_type) {
    animator_set_component(comp, target_params, duration_ms, delay_ms, path_type);
}

static inline void face_set_component_instant(face_component_t comp,
                                              const void *target_params) {
    animator_set_component_instant(comp, target_params);
}

static inline void face_set_sprite(const sprite_set_t *sprite) {
    renderer_set_sprite(sprite);
}

static inline void face_set_tilt(float pitch, float roll) {
    micro_animator_set_tilt(pitch, roll);
}

static inline void face_render_frame(void) {
    face_state_t display_state = *animator_get_state();
    micro_animator_apply(&display_state);
    prop_animator_apply(&display_state);
    renderer_render_frame(&display_state);
}

static inline void face_animator_tick(void) {
    animator_tick();
}

#ifdef __cplusplus
}
#endif

#endif
