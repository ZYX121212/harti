#include "face_vivid.h"
#include "lvgl.h"
#include <math.h>
#include <string.h>

/* ── Phase accumulator ───────────────────────────────────── */

static uint32_t vivid_t0;

/* ── Prop snapshot: store original values so we can apply offsets cleanly ── */

typedef struct {
    prop_type_t type;
    float base_angle;
    float base_distance;
    float base_scale;
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
            continue;
        }

        if (p->type != snapshots[i].type) {
            snapshots[i].type          = p->type;
            snapshots[i].base_angle    = p->angle;
            snapshots[i].base_distance = p->distance;
            snapshots[i].base_scale    = p->scale;
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
}
