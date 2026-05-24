#include "face_micro.h"
#include "lvgl.h"
#include <math.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════
   Timing constants (all in ms, driven by lv_tick_get())
   ═══════════════════════════════════════════════════════════ */

/* ── Blink ──────────────────────────────────────────────── */
#define BLINK_CLOSE_MS       80     // eyelid closing
#define BLINK_HOLD_MS        60     // fully closed
#define BLINK_OPEN_MS        160    // eyelid opening (~300ms total, natural)
#define BLINK_INTERVAL_MIN   2200   // min time between blinks
#define BLINK_INTERVAL_MAX   5500   // max time between blinks
#define BLINK_WIDE_EYE_EXTRA 2500   // extra interval when surprised
#define BLINK_DROOPY_FAST    -1200  // faster when sleepy
#define BLINK_CLOSED_LID     0.55f  // top_lid_mid.dy for fully closed
#define DOUBLE_BLINK_CHANCE  0.12f  // 12% chance of double blink
#define AUTO_WINK_CHANCE     0.05f  // 5% chance auto-blink is a wink

/* ── Gaze wandering ─────────────────────────────────────── */
#define GAZE_RANGE           0.28f  // max gaze offset from center
#define GAZE_DWELL_MIN       1200   // min time looking at one spot
#define GAZE_DWELL_MAX       4000   // max time looking at one spot
#define GAZE_TRANSIT_MS      280    // smooth transition between gaze points
#define GLANCE_CHANCE        0.15f  // 15% chance of glance instead of dwell
#define GLANCE_HOLD_MS       180    // glance hold time
#define GLANCE_RETURN_MS     220    // glance return time
#define GLANCE_RANGE         0.35f  // glance can go further

/* ── Breathing ──────────────────────────────────────────── */
#define BREATH_PERIOD_MS     3500
#define BREATH_AMPLITUDE     0.015f  // visible but subtle

/* ── Eye position drift ─────────────────────────────────── */
#define EYE_DRIFT_PERIOD_MS  4200
#define EYE_DRIFT_AMPLITUDE  0.12f  // ±1.8 px whole-eye shift

/* ── Tilt tracking ──────────────────────────────────────── */
#define TILT_SMOOTH          0.07f
#define TILT_MAX_IRIS        0.10f
#define TILT_GAIN            0.35f

/* ═══════════════════════════════════════════════════════════
   State
   ═══════════════════════════════════════════════════════════ */

/* ── Blink ──────────────────────────────────────────────── */
typedef enum {
    BLINK_WAITING,
    BLINK_CLOSING,
    BLINK_CLOSED,
    BLINK_OPENING,
} blink_phase_t;

typedef struct {
    blink_phase_t phase;
    uint32_t phase_start;
    float t;             // 0=open, 1=closed
    bool is_double;
    bool double_done;
    bool winking;        // true during manual wink, suppresses auto-blink
} eye_blink_t;

static eye_blink_t eye_blink[2];
static uint32_t next_blink_at;   // shared auto-blink timer

/* ── Gaze ───────────────────────────────────────────────── */
typedef enum {
    GAZE_DWELL,    // looking steadily at current point
    GAZE_TRANSIT,  // moving to a new point
    GAZE_GLANCE,   // quick look to side
    GAZE_RETURN,   // glancing back to center
} gaze_phase_t;

static gaze_phase_t gaze_phase = GAZE_DWELL;
static uint32_t gaze_start;
static float gaze_x, gaze_y;           // current gaze offset
static float gaze_from_x, gaze_from_y; // transition start
static float gaze_to_x, gaze_to_y;     // transition target

/* ── Breathing ──────────────────────────────────────────── */
static uint32_t breath_t0;

/* ── Tilt ───────────────────────────────────────────────── */
static float raw_pitch, raw_roll;
static float smooth_pitch, smooth_roll;

/* ── Master ─────────────────────────────────────────────── */
static bool enabled = true;

/* ═══════════════════════════════════════════════════════════
   Helpers
   ═══════════════════════════════════════════════════════════ */

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float randf(void) { return (float)rand() / (float)RAND_MAX; }
static float randf_range(float lo, float hi) { return lo + randf() * (hi - lo); }
static uint32_t rand_ms(uint32_t lo, uint32_t hi) { return lo + (rand() % (hi - lo + 1)); }

static float ease_out(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }

/* ═══════════════════════════════════════════════════════════
   Public API
   ═══════════════════════════════════════════════════════════ */

void micro_animator_init(void) {
    uint32_t now = lv_tick_get();

    // Blink
    for (int i = 0; i < 2; i++) {
        eye_blink[i].phase = BLINK_WAITING;
        eye_blink[i].phase_start = now;
        eye_blink[i].t = 0.0f;
        eye_blink[i].is_double = false;
        eye_blink[i].double_done = false;
        eye_blink[i].winking = false;
    }
    next_blink_at = now + rand_ms(BLINK_INTERVAL_MIN, BLINK_INTERVAL_MAX);

    // Gaze
    gaze_phase = GAZE_DWELL;
    gaze_x = gaze_y = 0;
    gaze_from_x = gaze_from_y = 0;
    gaze_to_x = gaze_to_y = 0;
    gaze_start = now;

    // Breath
    breath_t0 = now;

    // Tilt
    raw_pitch = raw_roll = 0;
    smooth_pitch = smooth_roll = 0;
}

void micro_animator_set_tilt(float pitch, float roll) {
    raw_pitch = pitch;
    raw_roll  = roll;
}

void micro_animator_set_enabled(bool on) { enabled = on; }

void micro_animator_apply(face_state_t *s) {
    if (!enabled) return;
    uint32_t now = lv_tick_get();

    /* ══════════════════════════════════════════════════════
       1. Blink
       ══════════════════════════════════════════════════════ */

    float avg_lid = (s->eye[0].top_lid_mid.dy + s->eye[1].top_lid_mid.dy) * 0.5f;
    int interval_adj = 0;
    if (avg_lid < -0.08f)      interval_adj = BLINK_WIDE_EYE_EXTRA;
    else if (avg_lid > 0.15f)  interval_adj = BLINK_DROOPY_FAST;

    switch (blink_phase) {

    case BLINK_WAITING:
        if (now >= next_blink_at) {
            blink_phase = BLINK_CLOSING;
            blink_start = now;
            blink_is_double = (!blink_double_done && randf() < DOUBLE_BLINK_CHANCE);
        }
        break;

    case BLINK_CLOSING: {
        uint32_t elapsed = now - blink_start;
        blink_t = clampf((float)elapsed / BLINK_CLOSE_MS, 0.0f, 1.0f);
        if (elapsed >= BLINK_CLOSE_MS) {
            blink_t = 1.0f;
            blink_phase = BLINK_CLOSED;
            blink_start = now;
        }
        break;
    }

    case BLINK_CLOSED:
        blink_t = 1.0f;
        if (now - blink_start >= BLINK_HOLD_MS) {
            blink_phase = BLINK_OPENING;
            blink_start = now;
        }
        break;

    case BLINK_OPENING: {
        uint32_t elapsed = now - blink_start;
        blink_t = 1.0f - clampf((float)elapsed / BLINK_OPEN_MS, 0.0f, 1.0f);
        if (elapsed >= BLINK_OPEN_MS) {
            blink_t = 0.0f;
            if (blink_is_double) {
                // Start second blink after a tiny gap
                blink_is_double = false;
                blink_double_done = true;
                blink_phase = BLINK_CLOSING;
                blink_start = now;
            } else {
                blink_phase = BLINK_WAITING;
                blink_start = now;
                blink_double_done = false;
                next_blink_at = now + rand_ms(
                    BLINK_INTERVAL_MIN + interval_adj,
                    BLINK_INTERVAL_MAX + interval_adj);
            }
        }
        break;
    }
    }

    // Apply blink to both eyes
    if (blink_t > 0.0f) {
        for (int i = 0; i < 2; i++) {
            float expr_lid = s->eye[i].top_lid_mid.dy;
            float closed_lid = expr_lid + BLINK_CLOSED_LID;
            s->eye[i].top_lid_mid.dy = lerpf(expr_lid, closed_lid, blink_t);
        }
    }

    /* ══════════════════════════════════════════════════════
       2. Gaze wandering
       ══════════════════════════════════════════════════════ */

    switch (gaze_phase) {

    case GAZE_DWELL:
        if (now - gaze_start >= rand_ms(GAZE_DWELL_MIN, GAZE_DWELL_MAX)) {
            if (randf() < GLANCE_CHANCE) {
                // Quick glance to the side
                gaze_from_x = gaze_x;
                gaze_from_y = gaze_y;
                gaze_to_x = randf_range(-GLANCE_RANGE, GLANCE_RANGE);
                gaze_to_y = randf_range(-GLANCE_RANGE, GLANCE_RANGE);
                gaze_phase = GAZE_GLANCE;
                gaze_start = now;
            } else {
                // Move to a new focus point
                gaze_from_x = gaze_x;
                gaze_from_y = gaze_y;
                gaze_to_x = randf_range(-GAZE_RANGE, GAZE_RANGE);
                gaze_to_y = randf_range(-GAZE_RANGE, GAZE_RANGE);
                gaze_phase = GAZE_TRANSIT;
                gaze_start = now;
            }
        }
        break;

    case GAZE_TRANSIT: {
        float t = clampf((float)(now - gaze_start) / GAZE_TRANSIT_MS, 0.0f, 1.0f);
        float et = ease_out(t);
        gaze_x = lerpf(gaze_from_x, gaze_to_x, et);
        gaze_y = lerpf(gaze_from_y, gaze_to_y, et);
        if (t >= 1.0f) {
            gaze_x = gaze_to_x;
            gaze_y = gaze_to_y;
            gaze_phase = GAZE_DWELL;
            gaze_start = now;
        }
        break;
    }

    case GAZE_GLANCE: {
        float t = clampf((float)(now - gaze_start) / GAZE_TRANSIT_MS, 0.0f, 1.0f);
        float et = ease_out(t);
        gaze_x = lerpf(gaze_from_x, gaze_to_x, et);
        gaze_y = lerpf(gaze_from_y, gaze_to_y, et);
        if (t >= 1.0f) {
            gaze_x = gaze_to_x;
            gaze_y = gaze_to_y;
            gaze_phase = GAZE_RETURN;
            gaze_start = now;
            gaze_from_x = gaze_x;
            gaze_from_y = gaze_y;
            gaze_to_x = 0.0f;
            gaze_to_y = 0.0f;
        }
        break;
    }

    case GAZE_RETURN: {
        float t = clampf((float)(now - gaze_start) / GLANCE_RETURN_MS, 0.0f, 1.0f);
        float et = ease_out(t);
        gaze_x = lerpf(gaze_from_x, gaze_to_x, et);
        gaze_y = lerpf(gaze_from_y, gaze_to_y, et);
        if (t >= 1.0f) {
            gaze_x = 0.0f;
            gaze_y = 0.0f;
            gaze_phase = GAZE_DWELL;
            gaze_start = now;
        }
        break;
    }
    }

    /* ══════════════════════════════════════════════════════
       3. Breathing
       ══════════════════════════════════════════════════════ */

    float breath_t = (float)((now - breath_t0) % BREATH_PERIOD_MS) / BREATH_PERIOD_MS;
    float wave = sinf(breath_t * 2.0f * 3.14159265f);
    s->face.squash_x  += wave * BREATH_AMPLITUDE;
    s->face.stretch_y -= wave * BREATH_AMPLITUDE;

    /* ══════════════════════════════════════════════════════
       4. Tilt tracking
       ══════════════════════════════════════════════════════ */

    smooth_pitch = smooth_pitch * (1.0f - TILT_SMOOTH) + raw_pitch * TILT_SMOOTH;
    smooth_roll  = smooth_roll  * (1.0f - TILT_SMOOTH) + raw_roll  * TILT_SMOOTH;

    float tilt_dx = clampf(smooth_roll  * TILT_GAIN, -TILT_MAX_IRIS, TILT_MAX_IRIS);
    float tilt_dy = clampf(smooth_pitch * TILT_GAIN, -TILT_MAX_IRIS, TILT_MAX_IRIS);

    /* ══════════════════════════════════════════════════════
       5. Slow eye-position drift (whole eye, independent of gaze)
       ══════════════════════════════════════════════════════ */

    float drift_t = (float)(now % EYE_DRIFT_PERIOD_MS) / EYE_DRIFT_PERIOD_MS;
    float drift_x = sinf(drift_t * 2.0f * 3.14159f) * EYE_DRIFT_AMPLITUDE;
    float drift_y = cosf(drift_t * 2.0f * 3.14159f + 1.2f) * EYE_DRIFT_AMPLITUDE * 0.7f;

    /* ══════════════════════════════════════════════════════
       6. Apply to eyes
       ══════════════════════════════════════════════════════ */

    for (int i = 0; i < 2; i++) {
        s->eye[i].iris_center.dx += gaze_x + tilt_dx;
        s->eye[i].iris_center.dy += gaze_y + tilt_dy;
        s->eye[i].position.dx += drift_x;
        s->eye[i].position.dy += drift_y;
    }
}
