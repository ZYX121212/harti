#include "face_prop.h"
#include "face_animator.h"
#include "lvgl.h"
#include <string.h>

#define PROP_DEFAULT_DURATION 200
#define PROP_CLEANUP_FRAMES   2

typedef enum {
    PROP_PHASE_IN,
    PROP_PHASE_HOLD,
    PROP_PHASE_OUT,
} prop_phase_t;

typedef struct {
    float target_opacity;
    float fade_speed;
    uint8_t dead_frames;
    prop_phase_t phase;
    uint16_t hold_frames;
} prop_anim_t;

static prop_anim_t anim[3];

void prop_animator_init(void) {
    memset(anim, 0, sizeof(anim));
}

void prop_animator_apply(face_state_t *s) {
    uint32_t dt = 50;

    for (int i = 0; i < (int)s->decor.prop_count; i++) {
        prop_anim_t *a = &anim[i];
        float op = s->decor.props[i].opacity;

        if (op < a->target_opacity) {
            op += a->fade_speed * dt;
            if (op > a->target_opacity) op = a->target_opacity;
        } else if (op > a->target_opacity) {
            op -= a->fade_speed * dt;
            if (op < a->target_opacity) op = a->target_opacity;
        }
        s->decor.props[i].opacity = op;

        /* Phase transitions */
        if (a->phase == PROP_PHASE_IN && op >= 0.99f) {
            a->phase = PROP_PHASE_HOLD;
        } else if (a->phase == PROP_PHASE_HOLD) {
            if (a->hold_frames > 0) {
                a->hold_frames--;
            } else {
                a->phase = PROP_PHASE_OUT;
                a->target_opacity = 0.0f;
                a->fade_speed = 1.0f / 150.0f;
            }
        }

        if (a->phase == PROP_PHASE_OUT && op <= 0.01f) a->dead_frames++;
        else if (a->phase != PROP_PHASE_OUT) a->dead_frames = 0;
    }

    /* Garbage collect dead props */
    int w = 0;
    for (int i = 0; i < (int)s->decor.prop_count; i++) {
        if (anim[i].dead_frames >= PROP_CLEANUP_FRAMES) continue;
        if (w != i) {
            s->decor.props[w] = s->decor.props[i];
            anim[w] = anim[i];
        }
        w++;
    }
    s->decor.prop_count = w;
}

static void insert_prop(face_state_t *s, prop_type_t type,
                        float angle, float distance, uint32_t duration_ms) {
    if (s->decor.prop_count >= 3) return;
    int i = s->decor.prop_count;
    s->decor.props[i].type     = type;
    s->decor.props[i].angle    = angle;
    s->decor.props[i].distance = distance;
    s->decor.props[i].scale    = 0.6f;
    s->decor.props[i].opacity  = 0.0f;
    s->decor.prop_count++;

    uint32_t hold_ms = (duration_ms > 0) ? duration_ms : PROP_DEFAULT_DURATION;
    anim[i].target_opacity = 1.0f;
    anim[i].fade_speed     = 1.0f / 100.0f;  // fade-in always 100ms
    anim[i].phase          = PROP_PHASE_IN;
    anim[i].hold_frames    = (uint16_t)(hold_ms / 50);
    anim[i].dead_frames    = 0;
}

void face_prop_show(prop_type_t type, float angle, float distance, uint32_t duration_ms) {
    face_state_t *s = (face_state_t *)animator_get_state();
    float dur = (duration_ms > 0) ? (float)duration_ms : PROP_DEFAULT_DURATION;

    for (int i = 0; i < (int)s->decor.prop_count; i++) {
        if (s->decor.props[i].type == type) {
            anim[i].target_opacity = 1.0f;
            anim[i].fade_speed     = 1.0f / 100.0f;
            anim[i].phase          = PROP_PHASE_IN;
            anim[i].hold_frames    = (uint16_t)(dur / 50);
            anim[i].dead_frames    = 0;
            s->decor.props[i].angle    = angle;
            s->decor.props[i].distance = distance;
            return;
        }
    }
    insert_prop(s, type, angle, distance, duration_ms);
}

void face_prop_hide(prop_type_t type, uint32_t duration_ms) {
    face_state_t *s = (face_state_t *)animator_get_state();
    float dur = (duration_ms > 0) ? (float)duration_ms : PROP_DEFAULT_DURATION;
    for (int i = 0; i < (int)s->decor.prop_count; i++) {
        if (s->decor.props[i].type == type) {
            anim[i].target_opacity = 0.0f;
            anim[i].fade_speed     = 1.0f / dur;
            anim[i].phase          = PROP_PHASE_OUT;
            anim[i].hold_frames    = 0;
            return;
        }
    }
}

void face_prop_clear(uint32_t duration_ms) {
    face_state_t *s = (face_state_t *)animator_get_state();
    float dur = (duration_ms > 0) ? (float)duration_ms : PROP_DEFAULT_DURATION;
    for (int i = 0; i < (int)s->decor.prop_count; i++) {
        anim[i].target_opacity = 0.0f;
        anim[i].fade_speed     = 1.0f / dur;
        anim[i].phase          = PROP_PHASE_OUT;
        anim[i].hold_frames    = 0;
    }
}
