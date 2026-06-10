#include "face_vivid.h"
#include "lvgl.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define PROP_SPRING_DURATION_MS 400
#define PROP_SPRING_OVERSHOOT   0.30f

static float randf_v(void) { return (float)rand() / (float)RAND_MAX; }
#define randf() randf_v()
static uint32_t rand_ms_v(uint32_t lo, uint32_t hi) {
    return lo + (uint32_t)(rand() % (hi - lo + 1));
}
#define rand_ms(lo, hi) rand_ms_v(lo, hi)

/* ── Phase accumulator ───────────────────────────────────── */

static uint32_t vivid_t0;

/* ── Tear particles ──────────────────────────────────────── */

typedef struct {
    float    y_offset;   /* px below eye bottom, grows downward */
    float    speed;      /* px / second */
    bool     active;
    uint32_t spawn_at;   /* absolute ms timestamp to spawn */
} tear_particle_t;

static tear_particle_t tears[4];  /* [0,1]=left eye, [2,3]=right eye */
static uint32_t        tear_prev_ms = 0;

/* ── Prop snapshot: store original values so we can apply offsets cleanly ── */

typedef struct {
    prop_type_t type;
    float base_angle;
    float base_distance;
    float base_scale;
    bool     spawn_active;
    uint32_t spawn_start_ms;
} prop_snapshot_t;

static prop_snapshot_t snapshots[3];
static bool snapshot_init = false;

/* ── Per-prop-type animation frequencies ─────────────────── */

static float prop_freq(prop_type_t t) {
    switch (t) {
        case PROP_TEACUP_STEAM: return 1.2f;
        case PROP_MUSIC_NOTE:   return 2.5f;
        case PROP_SUNGLASSES:   return 0.6f;
        case PROP_HEART:        return 1.5f;
        case PROP_STAR_SMALL:   return 3.0f;
        case PROP_TEACUP:       return 1.0f;
        default:                return 0.0f;
    }
}

/* ═══════════════════════════════════════════════════════════
   Public API
   ═══════════════════════════════════════════════════════════ */

void face_vivid_init(void) {
    vivid_t0 = lv_tick_get();
    memset(snapshots, 0, sizeof(snapshots));
    snapshot_init = false;
    /* Staggered tear spawn: 0ms, 200ms, 600ms, 800ms offsets */
    static const uint32_t SPAWN_DELAY_MS[4] = {0, 200, 600, 800};
    for (int i = 0; i < 4; i++) {
        tears[i].active   = false;
        tears[i].y_offset = 0.0f;
        tears[i].speed    = 0.0f;
        tears[i].spawn_at = vivid_t0 + SPAWN_DELAY_MS[i];
    }
    tear_prev_ms = 0;
}

void face_vivid_apply(face_state_t *s) {
    uint32_t now = lv_tick_get();
    float total_s = (float)(now - vivid_t0) / 1000.0f;

    /* ── Part B: Prop dynamics ─────────────────────────── */

    for (int i = 0; i < (int)s->decor.prop_count; i++) {
        prop_instance_t *p = &s->decor.props[i];
        float freq = prop_freq(p->type);
        if (freq <= 0.0f) continue;

        if (!snapshot_init || i >= 3) {
            snapshots[i].type          = p->type;
            snapshots[i].base_angle    = p->angle;
            snapshots[i].base_distance = p->distance;
            snapshots[i].base_scale    = p->scale;
            snapshots[i].spawn_active   = false;
            snapshots[i].spawn_start_ms = 0;
            continue;
        }

        if (p->type != snapshots[i].type) {
            snapshots[i].type          = p->type;
            snapshots[i].base_angle    = p->angle;
            snapshots[i].base_distance = p->distance;
            snapshots[i].base_scale    = p->scale;
            snapshots[i].spawn_active   = (p->type != PROP_NONE);
            snapshots[i].spawn_start_ms = now;
        }

        float phase = total_s * freq * 2.0f * 3.14159265f;
        float wave = sinf(phase);

        switch (p->type) {
        case PROP_TEACUP_STEAM:
            p->distance = snapshots[i].base_distance + wave * 0.03f;
            p->scale    = snapshots[i].base_scale    + wave * 0.05f;
            break;
        case PROP_MUSIC_NOTE:
            p->angle    = snapshots[i].base_angle + wave * 0.05f;
            p->scale    = snapshots[i].base_scale + sinf(phase * 2.0f) * 0.08f;
            break;
        case PROP_SUNGLASSES:
            p->distance = snapshots[i].base_distance + wave * 0.02f;
            break;
        case PROP_HEART:
            p->scale    = snapshots[i].base_scale + wave * 0.06f;
            break;
        case PROP_STAR_SMALL:
            p->angle    = snapshots[i].base_angle + total_s * 0.3f;
            p->scale    = snapshots[i].base_scale + sinf(phase) * 0.04f;
            break;
        case PROP_TEACUP:
            p->distance = snapshots[i].base_distance + wave * 0.02f;
            break;
        default:
            break;
        }

        /* ── Part B4: Prop spring entrance (overshoot envelope) ── */
        if (snapshots[i].spawn_active) {
            uint32_t dt = now - snapshots[i].spawn_start_ms;
            if (dt >= PROP_SPRING_DURATION_MS) {
                snapshots[i].spawn_active = false;
            } else {
                float t = (float)dt / (float)PROP_SPRING_DURATION_MS;
                float one_minus_t = 1.0f - t;
                float spring_scale = 1.0f + PROP_SPRING_OVERSHOOT *
                                     sinf(t * 3.14159265f) * one_minus_t * one_minus_t;
                p->scale = snapshots[i].base_scale * spring_scale;
            }
        }
    }

    snapshot_init = true;

    /* ── Part C: Facial micro-motion ───────────────────── */

    float micro_t = total_s * 2.0f * 3.14159265f;

    /* Eye position drift (0.7Hz, anti-phase between eyes) */
    float eye_dx = sinf(micro_t * 0.7f) * 0.02f;
    float eye_dy = cosf(micro_t * 0.7f) * 0.02f;
    s->eye[0].position.dx += eye_dx;
    s->eye[0].position.dy += eye_dy;
    s->eye[1].position.dx -= eye_dx;
    s->eye[1].position.dy -= eye_dy;

    /* Brow arch quiver (1.3Hz, 30% time gate) */
    float gate = sinf(micro_t * 2.7f + 0.3f);
    if (gate > 0.4f) {
        float arch_delta = sinf(micro_t * 1.3f) * 0.015f;
        s->brow[0].arch.dy += arch_delta;
        s->brow[1].arch.dy += arch_delta;
    }

    /* Mouth corner micro-shift (0.4Hz, 90 deg out of phase with breathing) */
    float mouth_d = sinf(micro_t * 0.4f + 1.5708f) * 0.01f;
    s->mouth.left_corner.dx  += mouth_d;
    s->mouth.right_corner.dx -= mouth_d;

    /* Pupil micro-pulse (1.8Hz, heartbeat-like) */
    float pupil_pulse = sinf(micro_t * 1.8f) * 0.03f;
    s->eye[0].pupil_scale += pupil_pulse;
    s->eye[1].pupil_scale += pupil_pulse;

    /* ── Part B3: Tear particle physics ─────────────────── */

    float dt_s = (tear_prev_ms == 0) ? 0.0f : (float)(now - tear_prev_ms) / 1000.0f;
    if (dt_s > 0.1f) dt_s = 0.1f;   /* clamp big frame gaps */
    tear_prev_ms = now;

    for (int i = 0; i < 4; i++) {
        tear_particle_t *t = &tears[i];
        if (!t->active) {
            if (now >= t->spawn_at) {
                t->active   = true;
                t->y_offset = 0.0f;
                t->speed    = 18.0f + randf() * 10.0f;
            }
            continue;
        }
        t->y_offset += t->speed * dt_s;
        if (t->y_offset > 30.0f) {
            t->active   = false;
            t->y_offset = 0.0f;
            t->spawn_at = now + rand_ms(400, 1200);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   Tear readout for renderer
   ═══════════════════════════════════════════════════════════ */

void face_vivid_get_tears(tear_drop_t out[4], const face_state_t *s) {
    (void)s;
    /* Eye-relative x nudge so paired drops don't perfectly overlap. */
    static const float X_OFF[4] = { -3.0f, 3.0f, -3.0f, 3.0f };
    for (int i = 0; i < 4; i++) {
        const tear_particle_t *t = &tears[i];
        if (!t->active) {
            out[i].x       = 0.0f;
            out[i].y       = 0.0f;
            out[i].opacity = 0.0f;
            continue;
        }
        out[i].x = X_OFF[i];
        out[i].y = t->y_offset;
        /* Fade in at birth, fade out near despawn for a soft trail. */
        float op = 1.0f;
        if (t->y_offset < 4.0f)        op = t->y_offset / 4.0f;
        else if (t->y_offset > 24.0f)  op = (30.0f - t->y_offset) / 6.0f;
        if (op < 0.0f) op = 0.0f;
        if (op > 1.0f) op = 1.0f;
        out[i].opacity = op;
    }
}
