#include "face_temperament.h"
#include "face_animator.h"
#include "face_micro.h"
#include "lvgl.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════
   Pulse constants
   ═══════════════════════════════════════════════════════════ */

#define PULSE_DECAY_MS 300

/* ═══════════════════════════════════════════════════════════
   Global state
   ═══════════════════════════════════════════════════════════ */

static struct {
    temperament_profile_t profile;
    temperament_profile_t current;

    bool pulse_active;
    uint32_t pulse_start;
    expression_id_t pulse_target;

    struct {
        float mouth_corner_dx;
        float mouth_corner_dy;
        float mouth_openness;
        float brow_arch_dy;
        float eye_lid_dy;
        float eye_pupil_scale;
        float eye_shine;
        float brow_thickness;
        float blush;
    } pulse;
} ctx;

/* ═══════════════════════════════════════════════════════════
   Helpers
   ═══════════════════════════════════════════════════════════ */

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float randf_range(float lo, float hi) {
    return lo + ((float)rand() / (float)RAND_MAX) * (hi - lo);
}

static float decay(uint32_t elapsed, uint32_t decay_ms) {
    if (elapsed >= decay_ms) return 0.0f;
    return 1.0f - (float)elapsed / (float)decay_ms;
}

/* ═══════════════════════════════════════════════════════════
   Preset profiles
   ═══════════════════════════════════════════════════════════ */

const temperament_profile_t TEMPERAMENT_DEFAULT  = {0.5f, 0.6f, 0.1f, 0.5f};
const temperament_profile_t TEMPERAMENT_CHILL    = {0.3f, 0.3f, 0.2f, 0.3f};
const temperament_profile_t TEMPERAMENT_DRAMATIC = {0.9f, 0.9f, 0.3f, 0.8f};

/* ═══════════════════════════════════════════════════════════
   Public API
   ═══════════════════════════════════════════════════════════ */

void face_temperament_init(void) {
    ctx.profile = TEMPERAMENT_DEFAULT;
    ctx.current = ctx.profile;
    ctx.pulse_active = false;
    memset(&ctx.pulse, 0, sizeof(ctx.pulse));
}

void face_temperament_set_profile(const temperament_profile_t *p) {
    ctx.profile = *p;
    ctx.current = *p;
}

void face_temperament_notify_expression_change(expression_id_t prev, expression_id_t cur) {
    /* Apply jitter to profile */
    ctx.current.warmth    = clampf(ctx.profile.warmth    + randf_range(-0.08f, 0.08f), 0.0f, 1.0f);
    ctx.current.energy    = clampf(ctx.profile.energy    + randf_range(-0.08f, 0.08f), 0.0f, 1.0f);
    ctx.current.mischief  = clampf(ctx.profile.mischief  + randf_range(-0.08f, 0.08f), 0.0f, 1.0f);
    ctx.current.curiosity = clampf(ctx.profile.curiosity + randf_range(-0.08f, 0.08f), 0.0f, 1.0f);

    /* ── Set up transition pulse ───────────────────── */

    memset(&ctx.pulse, 0, sizeof(ctx.pulse));
    ctx.pulse_active = true;
    ctx.pulse_start  = lv_tick_get();
    ctx.pulse_target = cur;

    switch (cur) {
    case 1: /* HAPPY */
        ctx.pulse.mouth_corner_dx = 0.05f;
        ctx.pulse.brow_arch_dy    = -0.03f;
        break;
    case 3: /* SURPRISED */
        ctx.pulse.eye_lid_dy      = -0.04f;
        ctx.pulse.mouth_openness  = 0.05f;
        break;
    case 2: /* SAD */
        ctx.pulse.eye_lid_dy      = 0.03f;
        ctx.pulse.mouth_corner_dy = 0.02f;
        break;
    case 10: /* COLD */
        ctx.pulse.eye_pupil_scale = -0.05f;
        break;
    case 7: /* EXCITED */
        ctx.pulse.mouth_openness  = 0.08f;
        ctx.pulse.eye_shine       = 0.15f;
        break;
    case 5: /* ANGRY */
        ctx.pulse.brow_thickness  = 0.2f;
        ctx.pulse.mouth_corner_dy = 0.03f;
        break;
    case 11: /* WARM */
        ctx.pulse.eye_shine       = 0.1f;
        ctx.pulse.blush           = 0.15f;
        break;
    default:
        ctx.pulse_active = false;
        break;
    }

    /* ── Mischief: random micro-expression ────────── */

    if (randf_range(0.0f, 1.0f) < ctx.current.mischief) {
        int roll = rand() % 3;
        if (roll == 0) {
            micro_animator_wink(rand() & 1);
        }
    }

    /* ── Curiosity: override animator timing ───────── */

    float time_mult = 1.0f / (0.3f + ctx.current.curiosity * 0.7f);

    expression_id_t safe_id = (cur < EXPRESSION_COUNT) ? cur : 0;
    const expression_def_t *def = &EXPRESSION_DEFS[safe_id];

    animator_set_component(COMPONENT_FACE, &def->target.face,
        (uint32_t)(def->timing[COMPONENT_FACE].duration_ms * time_mult),
        (uint32_t)(def->timing[COMPONENT_FACE].delay_ms * time_mult),
        def->timing[COMPONENT_FACE].path_type);

    animator_set_component(COMPONENT_BROW_LEFT, &def->target.brow[0],
        (uint32_t)(def->timing[COMPONENT_BROW_LEFT].duration_ms * time_mult),
        (uint32_t)(def->timing[COMPONENT_BROW_LEFT].delay_ms * time_mult),
        def->timing[COMPONENT_BROW_LEFT].path_type);

    animator_set_component(COMPONENT_BROW_RIGHT, &def->target.brow[1],
        (uint32_t)(def->timing[COMPONENT_BROW_RIGHT].duration_ms * time_mult),
        (uint32_t)(def->timing[COMPONENT_BROW_RIGHT].delay_ms * time_mult),
        def->timing[COMPONENT_BROW_RIGHT].path_type);

    animator_set_component(COMPONENT_EYE_LEFT, &def->target.eye[0],
        (uint32_t)(def->timing[COMPONENT_EYE_LEFT].duration_ms * time_mult),
        (uint32_t)(def->timing[COMPONENT_EYE_LEFT].delay_ms * time_mult),
        def->timing[COMPONENT_EYE_LEFT].path_type);

    animator_set_component(COMPONENT_EYE_RIGHT, &def->target.eye[1],
        (uint32_t)(def->timing[COMPONENT_EYE_RIGHT].duration_ms * time_mult),
        (uint32_t)(def->timing[COMPONENT_EYE_RIGHT].delay_ms * time_mult),
        def->timing[COMPONENT_EYE_RIGHT].path_type);

    animator_set_component(COMPONENT_MOUTH, &def->target.mouth,
        (uint32_t)(def->timing[COMPONENT_MOUTH].duration_ms * time_mult),
        (uint32_t)(def->timing[COMPONENT_MOUTH].delay_ms * time_mult),
        def->timing[COMPONENT_MOUTH].path_type);

    animator_set_component(COMPONENT_DECOR, &def->target.decor,
        (uint32_t)(def->timing[COMPONENT_DECOR].duration_ms * time_mult),
        (uint32_t)(def->timing[COMPONENT_DECOR].delay_ms * time_mult),
        def->timing[COMPONENT_DECOR].path_type);
}

void face_temperament_apply(face_state_t *s) {
    /* ── Apply transition pulse decay ───────────────── */

    if (ctx.pulse_active) {
        uint32_t elapsed = lv_tick_get() - ctx.pulse_start;
        float d = decay(elapsed, PULSE_DECAY_MS);

        if (d <= 0.0f) {
            ctx.pulse_active = false;
        } else {
            s->mouth.left_corner.dx  += ctx.pulse.mouth_corner_dx * d;
            s->mouth.right_corner.dx -= ctx.pulse.mouth_corner_dx * d;
            s->mouth.right_corner.dy += ctx.pulse.mouth_corner_dy * d;
            s->mouth.left_corner.dy  += ctx.pulse.mouth_corner_dy * d;
            s->mouth.openness        += ctx.pulse.mouth_openness * d;

            s->brow[0].arch.dy += ctx.pulse.brow_arch_dy * d;
            s->brow[1].arch.dy += ctx.pulse.brow_arch_dy * d;
            s->brow[0].thickness += ctx.pulse.brow_thickness * d;
            s->brow[1].thickness += ctx.pulse.brow_thickness * d;

            s->eye[0].top_lid_mid.dy += ctx.pulse.eye_lid_dy * d;
            s->eye[1].top_lid_mid.dy += ctx.pulse.eye_lid_dy * d;
            s->eye[0].pupil_scale    += ctx.pulse.eye_pupil_scale * d;
            s->eye[1].pupil_scale    += ctx.pulse.eye_pupil_scale * d;
            s->eye[0].shine_intensity += ctx.pulse.eye_shine * d;
            s->eye[1].shine_intensity += ctx.pulse.eye_shine * d;

            s->decor.blush += ctx.pulse.blush * d;
        }
    }

    /* ── Temperament coloring ────────────────────────── */

    float energy_bias = (ctx.current.energy - 0.5f) * 0.2f;
    s->mouth.openness += energy_bias * 0.15f;
    s->eye[0].pupil_scale += energy_bias * 0.1f;
    s->eye[1].pupil_scale += energy_bias * 0.1f;

    float warmth_bias = (ctx.current.warmth - 0.5f) * 0.06f;
    s->brow[0].arch.dy += ctx.current.warmth > 0.5f ? -warmth_bias * 0.8f : 0;
    s->brow[1].arch.dy += ctx.current.warmth > 0.5f ? -warmth_bias * 0.8f : 0;
    s->mouth.left_corner.dx  += warmth_bias;
    s->mouth.right_corner.dx -= warmth_bias;
}
