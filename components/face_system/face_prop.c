#include "face_prop.h"
#include "face_animator.h"
#include "lvgl.h"
#include <string.h>

#define PROP_DEFAULT_DURATION 200
#define PROP_CLEANUP_FRAMES   2

typedef struct {
    float target_opacity;
    float fade_speed;
    uint8_t dead_frames;
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

        if (a->target_opacity <= 0.01f && op <= 0.01f) a->dead_frames++;
        else a->dead_frames = 0;
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

    float dur = (duration_ms > 0) ? (float)duration_ms : PROP_DEFAULT_DURATION;
    anim[i].target_opacity = 1.0f;
    anim[i].fade_speed     = 1.0f / dur;
    anim[i].dead_frames    = 0;
}

void face_prop_show(prop_type_t type, float angle, float distance, uint32_t duration_ms) {
    face_state_t *s = (face_state_t *)animator_get_state();
    float dur = (duration_ms > 0) ? (float)duration_ms : PROP_DEFAULT_DURATION;

    for (int i = 0; i < (int)s->decor.prop_count; i++) {
        if (s->decor.props[i].type == type) {
            anim[i].target_opacity = 1.0f;
            anim[i].fade_speed     = 1.0f / dur;
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
    }
}
