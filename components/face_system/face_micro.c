#include "face_micro.h"
#include "../../main/harti_config.h"
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

/* ── Expression-linked micro-animations ─────────────────────────────────── */
static expression_id_t current_expr_id = 0;

/* BORED sigh state machine */
typedef enum { SIGH_IDLE, SIGH_CLOSING, SIGH_HOLD, SIGH_OPENING } sigh_phase_t;
static sigh_phase_t sigh_phase    = SIGH_IDLE;
static uint32_t     sigh_start    = 0;
static uint32_t     next_sigh_at  = 0;

/* SLEEPY startle state machine */
typedef enum { STARTLE_IDLE, STARTLE_OPEN, STARTLE_HOLD, STARTLE_CLOSE } startle_phase_t;
static startle_phase_t startle_phase    = STARTLE_IDLE;
static uint32_t        startle_start    = 0;
static uint32_t        next_startle_at  = 0;

/* ── Entry Impact ───────────────────────────────────────── */
typedef struct {
    bool     active;
    uint32_t start_ms;
    uint32_t duration_ms;
    float    sq_peak;       /* squash_x additive peak */
    float    st_peak;       /* stretch_y additive peak */
    float    pupil_peak;    /* pupil_scale additive peak (both eyes) */
    float    brow_peak;     /* brow arch.dy additive peak (both brows) */
    /* EXCITED two-peak extension (sq2_peak==0 → single peak) */
    float    sq2_peak;
    float    st2_peak;
    uint32_t peak2_ms;      /* start ms of second bell (offset from start) */
} impact_state_t;

static impact_state_t g_impact;

typedef struct {
    float    sq_peak;
    float    st_peak;
    uint32_t duration_ms;
    float    pupil_peak;
    float    brow_peak;
    float    sq2_peak;
    float    st2_peak;
    uint32_t peak2_ms;
} impact_cfg_t;

/* Indexed by EMOTION_* from harti_config.h */
static const impact_cfg_t IMPACT_TABLE[] = {
    /* [0]  NEUTRAL     */ {  0.00f,  0.00f,   0,  0.00f,  0.00f,  0.00f,  0.00f,   0 },
    /* [1]  HAPPY       */ { +0.15f, -0.10f, 200, +0.05f, -0.03f,  0.00f,  0.00f,   0 },
    /* [2]  SAD         */ {  0.00f, -0.08f, 350, -0.05f,  0.00f,  0.00f,  0.00f,   0 },
    /* [3]  SURPRISED   */ { +0.35f, -0.20f, 180, +0.12f, -0.06f,  0.00f,  0.00f,   0 },
    /* [4]  SLEEPY      */ {  0.00f, -0.06f, 400, -0.08f,  0.00f,  0.00f,  0.00f,   0 },
    /* [5]  ANGRY       */ { +0.22f, -0.14f, 140, -0.04f, +0.04f,  0.00f,  0.00f,   0 },
    /* [6]  BORED       */ {  0.00f,  0.00f,   0,  0.00f,  0.00f,  0.00f,  0.00f,   0 },
    /* [7]  EXCITED     */ { +0.30f, -0.18f, 320, +0.10f, -0.05f, -0.15f,  0.00f, 240 },
    /* [8]  CONFUSED    */ { +0.12f,  0.00f, 180,  0.00f, +0.03f,  0.00f,  0.00f,   0 },
    /* [9]  CONTENT     */ {  0.00f, +0.04f, 300,  0.00f, -0.02f,  0.00f,  0.00f,   0 },
    /* [10] COLD        */ { +0.18f, -0.10f, 200, -0.06f,  0.00f,  0.00f,  0.00f,   0 },
    /* [11] WARM        */ { +0.10f, +0.08f, 220, +0.04f, -0.04f,  0.00f,  0.00f,   0 },
    /* [12] HEART_EYES  */ { +0.18f, +0.12f, 200,  0.00f, -0.04f,  0.00f,  0.00f,   0 },
    /* [13] THINKING    */ { -0.05f, +0.06f, 200,  0.00f,  0.00f,  0.00f,  0.00f,   0 },
    /* [14] DIZZY       */ { +0.25f, -0.15f, 280,  0.00f,  0.00f,  0.00f,  0.00f,   0 },
    /* [15] UPSIDE_DOWN */ {  0.00f, -0.06f, 350,  0.00f,  0.00f,  0.00f,  0.00f,   0 },
};

/* ── Per-expression micro-animation targets ────────────── */
#define MICRO_TGT_BOT_LID    0
#define MICRO_TGT_TOP_LID    1
#define MICRO_TGT_BROW_ARCH  2
#define MICRO_TGT_PUPIL      3
#define MICRO_TGT_FACE_SQ    4
#define MICRO_TGT_SHINE      5
#define MICRO_TGT_IRIS_DX    6
#define MICRO_TGT_BLUSH      7

typedef struct {
    float   freq_hz;
    float   amplitude;
    uint8_t target;
    uint8_t eye_mask;    /* 0=both, 1=left only, 2=right only */
    bool    gated;
    float   gate_freq_hz;
} expr_micro_cfg_t;

#define MICRO_MAX_EFFECTS 3
typedef struct {
    uint8_t          count;
    expr_micro_cfg_t cfg[MICRO_MAX_EFFECTS];
} expr_micro_entry_t;

static const expr_micro_entry_t EXPR_MICRO_TABLE[] = {
    /* [0]  NEUTRAL     */ { .count = 0 },
    /* [1]  HAPPY       */ { 2, {
        { 0.5f,  +0.015f, MICRO_TGT_BOT_LID,   0, false, 0.0f },
        { 1.1f,  +0.012f, MICRO_TGT_BROW_ARCH,  0, true,  1.8f },
    }},
    /* [2]  SAD         */ { .count = 0 },  /* handled by tear particles */
    /* [3]  SURPRISED   */ { 1, {
        { 0.3f,  +0.018f, MICRO_TGT_IRIS_DX,    0, true,  0.07f },
    }},
    /* [4]  SLEEPY      */ { .count = 0 },  /* handled by startle SM */
    /* [5]  ANGRY       */ { 2, {
        { 10.0f, +0.008f, MICRO_TGT_TOP_LID,    0, true,  2.0f  },
        {  2.0f, +0.012f, MICRO_TGT_BROW_ARCH,  0, false, 0.0f  },
    }},
    /* [6]  BORED       */ { .count = 0 },  /* handled by sigh SM */
    /* [7]  EXCITED     */ { 2, {
        { 4.0f,  +0.020f, MICRO_TGT_FACE_SQ,    0, false, 0.0f },
        { 1.8f,  +0.060f, MICRO_TGT_SHINE,      0, false, 0.0f },
    }},
    /* [8]  CONFUSED    */ { 2, {
        { 1.0f,  +0.022f, MICRO_TGT_BROW_ARCH,  1, false, 0.0f }, /* left only */
        { 1.0f,  -0.022f, MICRO_TGT_BROW_ARCH,  2, false, 0.0f }, /* right, anti-phase */
    }},
    /* [9]  CONTENT     */ { 1, {
        { 0.12f, +0.035f, MICRO_TGT_TOP_LID,    0, false, 0.0f },
    }},
    /* [10] COLD        */ { .count = 0 },  /* handled by shiver block */
    /* [11] WARM        */ { 1, {
        { 0.3f,  +0.040f, MICRO_TGT_BLUSH,      0, false, 0.0f },
    }},
    /* [12] HEART_EYES  */ { 1, {
        { 1.5f,  +0.045f, MICRO_TGT_PUPIL,      0, false, 0.0f },
    }},
    /* [13] THINKING    */ { 1, {
        { 0.07f, +0.080f, MICRO_TGT_IRIS_DX,    0, false, 0.0f },
    }},
    /* [14] DIZZY       */ { .count = 0 },
    /* [15] UPSIDE_DOWN */ { .count = 0 },
};

/* ── Eye contact simulation ─────────────────────────────── */
typedef enum {
    EC_IDLE,
    EC_CONVERGING,
    EC_HOLDING,
    EC_RELEASING,
} eye_contact_phase_t;

static eye_contact_phase_t ec_phase    = EC_IDLE;
static uint32_t            ec_start    = 0;
static uint32_t            ec_hold_ms  = 0;
static uint32_t            next_ec_at  = 0;

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

    // Impact
    g_impact.active = false;

    // Eye contact
    ec_phase   = EC_IDLE;
    next_ec_at = now + rand_ms(8000, 15000);
}

void micro_animator_set_expression(expression_id_t id) {
    if (id == current_expr_id) return;
    current_expr_id = id;
    /* Reset expression-linked state machines */
    sigh_phase = SIGH_IDLE;
    next_sigh_at = lv_tick_get() + rand_ms(3000, 8000);
    startle_phase = STARTLE_IDLE;
    next_startle_at = lv_tick_get() + rand_ms(5000, 12000);

    /* Trigger entry impact */
    if (id < (expression_id_t)(sizeof(IMPACT_TABLE) / sizeof(IMPACT_TABLE[0]))) {
        const impact_cfg_t *cfg = &IMPACT_TABLE[id];
        if (cfg->duration_ms > 0) {
            g_impact.active      = true;
            g_impact.start_ms    = lv_tick_get();
            g_impact.duration_ms = cfg->duration_ms;
            g_impact.sq_peak     = cfg->sq_peak;
            g_impact.st_peak     = cfg->st_peak;
            g_impact.pupil_peak  = cfg->pupil_peak;
            g_impact.brow_peak   = cfg->brow_peak;
            g_impact.sq2_peak    = cfg->sq2_peak;
            g_impact.st2_peak    = cfg->st2_peak;
            g_impact.peak2_ms    = cfg->peak2_ms;
        } else {
            g_impact.active = false;
        }
    }
}

void micro_animator_set_tilt(float pitch, float roll) {
    raw_pitch = pitch;
    raw_roll  = roll;
}

void micro_animator_set_enabled(bool on) { enabled = on; }

void micro_animator_wink(int eye) {
    if (eye < 0 || eye > 1) return;
    eye_blink[eye].phase = BLINK_CLOSING;
    eye_blink[eye].phase_start = lv_tick_get();
    eye_blink[eye].t = 0.0f;
    eye_blink[eye].is_double = false;
    eye_blink[eye].double_done = false;
    eye_blink[eye].winking = true;
}

void micro_animator_apply(face_state_t *s) {
    if (!enabled) return;
    uint32_t now = lv_tick_get();
    float total_s = (float)now / 1000.0f;

    /* ══════════════════════════════════════════════════════
       1. Blink / Wink (per-eye)
       ══════════════════════════════════════════════════════ */

    // Shared interval adjustment based on average lid position
    float avg_lid = (s->eye[0].top_lid_mid.dy + s->eye[1].top_lid_mid.dy) * 0.5f;
    int interval_adj = 0;
    if (avg_lid < -0.08f)      interval_adj = BLINK_WIDE_EYE_EXTRA;
    else if (avg_lid > 0.15f)  interval_adj = BLINK_DROOPY_FAST;

    // Shared auto-blink trigger: both eyes waiting → decide blink vs wink
    if (now >= next_blink_at) {
        bool both_available = (eye_blink[0].phase == BLINK_WAITING && !eye_blink[0].winking)
                           && (eye_blink[1].phase == BLINK_WAITING && !eye_blink[1].winking);

        if (both_available && randf() < AUTO_WINK_CHANCE) {
            // Auto-wink: pick a random eye
            int w = (rand() & 1);
            eye_blink[w].phase = BLINK_CLOSING;
            eye_blink[w].phase_start = now;
            eye_blink[w].is_double = (!eye_blink[w].double_done && randf() < DOUBLE_BLINK_CHANCE);
        } else {
            // Normal blink: start any available eye(s)
            for (int i = 0; i < 2; i++) {
                if (eye_blink[i].phase == BLINK_WAITING && !eye_blink[i].winking) {
                    eye_blink[i].phase = BLINK_CLOSING;
                    eye_blink[i].phase_start = now;
                    eye_blink[i].is_double = (!eye_blink[i].double_done && randf() < DOUBLE_BLINK_CHANCE);
                }
            }
        }
        // Timer consumed — will be reset when all eyes return to WAITING
        next_blink_at = UINT32_MAX;
    }

    // Per-eye phase machine
    for (int i = 0; i < 2; i++) {
        eye_blink_t *eb = &eye_blink[i];

        switch (eb->phase) {

        case BLINK_WAITING:
            break;  // nothing to do, timer trigger handles transition

        case BLINK_CLOSING: {
            uint32_t elapsed = now - eb->phase_start;
            eb->t = clampf((float)elapsed / BLINK_CLOSE_MS, 0.0f, 1.0f);
            if (elapsed >= BLINK_CLOSE_MS) {
                eb->t = 1.0f;
                eb->phase = BLINK_CLOSED;
                eb->phase_start = now;
            }
            break;
        }

        case BLINK_CLOSED:
            eb->t = 1.0f;
            if (now - eb->phase_start >= BLINK_HOLD_MS) {
                eb->phase = BLINK_OPENING;
                eb->phase_start = now;
            }
            break;

        case BLINK_OPENING: {
            uint32_t elapsed = now - eb->phase_start;
            eb->t = 1.0f - clampf((float)elapsed / BLINK_OPEN_MS, 0.0f, 1.0f);
            if (elapsed >= BLINK_OPEN_MS) {
                eb->t = 0.0f;
                if (eb->is_double) {
                    eb->is_double = false;
                    eb->double_done = true;
                    eb->phase = BLINK_CLOSING;
                    eb->phase_start = now;
                } else {
                    eb->phase = BLINK_WAITING;
                    eb->phase_start = now;
                    eb->double_done = false;
                    eb->winking = false;  // clear manual wink flag
                }
            }
            break;
        }
        }
    }

    // Reset shared timer when both eyes are idle
    if (eye_blink[0].phase == BLINK_WAITING && eye_blink[1].phase == BLINK_WAITING
        && next_blink_at == UINT32_MAX) {
        next_blink_at = now + rand_ms(
            BLINK_INTERVAL_MIN + interval_adj,
            BLINK_INTERVAL_MAX + interval_adj);
    }

    // Apply per-eye blink to eyelids
    for (int i = 0; i < 2; i++) {
        if (eye_blink[i].t > 0.0f) {
            float expr_lid = s->eye[i].top_lid_mid.dy;
            float closed_lid = expr_lid + BLINK_CLOSED_LID;
            s->eye[i].top_lid_mid.dy = lerpf(expr_lid, closed_lid, eye_blink[i].t);
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

    /* ══════════════════════════════════════════════════════
       6-B. Per-expression micro-animations (data-driven)
       ══════════════════════════════════════════════════════ */
    if (current_expr_id < (expression_id_t)(sizeof(EXPR_MICRO_TABLE) / sizeof(EXPR_MICRO_TABLE[0]))) {
        const expr_micro_entry_t *entry = &EXPR_MICRO_TABLE[current_expr_id];
        for (int ci = 0; ci < entry->count; ci++) {
            const expr_micro_cfg_t *cfg = &entry->cfg[ci];
            float delta = sinf(2.0f * 3.14159265f * cfg->freq_hz * total_s) * cfg->amplitude;
            if (cfg->gated) {
                float gate = sinf(2.0f * 3.14159265f * cfg->gate_freq_hz * total_s);
                if (gate <= 0.4f) continue;
            }
            switch (cfg->target) {
            case MICRO_TGT_BOT_LID:
                for (int i = 0; i < 2; i++)
                    if (cfg->eye_mask == 0 || cfg->eye_mask == (uint8_t)(i + 1))
                        s->eye[i].bot_lid_mid.dy += delta;
                break;
            case MICRO_TGT_TOP_LID:
                for (int i = 0; i < 2; i++)
                    if (cfg->eye_mask == 0 || cfg->eye_mask == (uint8_t)(i + 1))
                        s->eye[i].top_lid_mid.dy += delta;
                break;
            case MICRO_TGT_BROW_ARCH:
                for (int i = 0; i < 2; i++)
                    if (cfg->eye_mask == 0 || cfg->eye_mask == (uint8_t)(i + 1))
                        s->brow[i].arch.dy += delta;
                break;
            case MICRO_TGT_PUPIL:
                for (int i = 0; i < 2; i++)
                    if (cfg->eye_mask == 0 || cfg->eye_mask == (uint8_t)(i + 1))
                        s->eye[i].pupil_scale += delta;
                break;
            case MICRO_TGT_FACE_SQ:
                s->face.squash_x  += delta;
                s->face.stretch_y -= delta * 0.5f;
                break;
            case MICRO_TGT_SHINE:
                for (int i = 0; i < 2; i++)
                    if (cfg->eye_mask == 0 || cfg->eye_mask == (uint8_t)(i + 1))
                        s->eye[i].shine_intensity += delta;
                break;
            case MICRO_TGT_IRIS_DX:
                for (int i = 0; i < 2; i++)
                    if (cfg->eye_mask == 0 || cfg->eye_mask == (uint8_t)(i + 1))
                        s->eye[i].iris_center.dx += delta;
                break;
            case MICRO_TGT_BLUSH:
                s->decor.blush = clampf(s->decor.blush + delta, 0.0f, 1.0f);
                break;
            default: break;
            }
        }
    }

    /* ══════════════════════════════════════════════════════
       6-C. Eye contact simulation
       ══════════════════════════════════════════════════════ */
    {
        float ec_dx = 0.0f, ec_dy = 0.0f;
        bool suppressed = (current_expr_id == EMOTION_SLEEPY ||
                           current_expr_id == EMOTION_BORED);

        switch (ec_phase) {
        case EC_IDLE:
            if (!suppressed && now >= next_ec_at) {
                ec_phase = EC_CONVERGING;
                ec_start = now;
            }
            break;
        case EC_CONVERGING: {
            float t = clampf((float)(now - ec_start) / 200.0f, 0.0f, 1.0f);
            ec_dx = -gaze_x * t;
            ec_dy = -gaze_y * t;
            if (t >= 1.0f) {
                ec_phase   = EC_HOLDING;
                ec_start   = now;
                ec_hold_ms = rand_ms(200, 400);
            }
            break;
        }
        case EC_HOLDING:
            ec_dx = -gaze_x;
            ec_dy = -gaze_y;
            if (now - ec_start >= ec_hold_ms) {
                ec_phase = EC_RELEASING;
                ec_start = now;
            }
            break;
        case EC_RELEASING: {
            float t = clampf((float)(now - ec_start) / 150.0f, 0.0f, 1.0f);
            ec_dx = -gaze_x * (1.0f - t);
            ec_dy = -gaze_y * (1.0f - t);
            if (t >= 1.0f) {
                ec_phase   = EC_IDLE;
                next_ec_at = now + rand_ms(8000, 15000);
            }
            break;
        }
        }
        for (int i = 0; i < 2; i++) {
            s->eye[i].iris_center.dx += ec_dx;
            s->eye[i].iris_center.dy += ec_dy;
        }
    }

    /* ── 3-A COLD shiver: 8 Hz squash/stretch ──────────────── */
    if (current_expr_id == EMOTION_COLD) {
        float shiver = sinf((float)now * 2.0f * 3.14159265f * 8.0f / 1000.0f) * 0.025f;
        s->face.squash_x  += shiver;
        s->face.stretch_y -= shiver * 0.6f;
    }

    /* ── 3-B BORED sigh: periodic eyelid droop ─────────────── */
    if (current_expr_id == EMOTION_BORED) {
        if (sigh_phase == SIGH_IDLE && now >= next_sigh_at) {
            sigh_phase = SIGH_CLOSING;
            sigh_start = now;
        }
        if (sigh_phase != SIGH_IDLE) {
            uint32_t dt = now - sigh_start;
            float sigh_t = 0.0f;
            switch (sigh_phase) {
            case SIGH_CLOSING:
                sigh_t = clampf((float)dt / 400.0f, 0.0f, 1.0f);
                s->eye[0].top_lid_mid.dy += ease_out(sigh_t) * 0.45f;
                s->eye[1].top_lid_mid.dy += ease_out(sigh_t) * 0.45f;
                if (dt >= 400) { sigh_phase = SIGH_HOLD; sigh_start = now; }
                break;
            case SIGH_HOLD:
                s->eye[0].top_lid_mid.dy += 0.45f;
                s->eye[1].top_lid_mid.dy += 0.45f;
                if (dt >= 200) { sigh_phase = SIGH_OPENING; sigh_start = now; }
                break;
            case SIGH_OPENING:
                sigh_t = clampf((float)dt / 600.0f, 0.0f, 1.0f);
                s->eye[0].top_lid_mid.dy += (1.0f - sigh_t) * 0.45f;
                s->eye[1].top_lid_mid.dy += (1.0f - sigh_t) * 0.45f;
                if (dt >= 600) {
                    sigh_phase = SIGH_IDLE;
                    next_sigh_at = now + rand_ms(8000, 15000);
                }
                break;
            default: break;
            }
        }
    } else {
        sigh_phase = SIGH_IDLE;
    }

    /* ── 3-C SLEEPY startle: brief eye-open ────────────────── */
    if (current_expr_id == EMOTION_SLEEPY) {
        if (startle_phase == STARTLE_IDLE && now >= next_startle_at) {
            startle_phase = STARTLE_OPEN;
            startle_start = now;
        }
        if (startle_phase != STARTLE_IDLE) {
            uint32_t dt = now - startle_start;
            float t = 0.0f;
            switch (startle_phase) {
            case STARTLE_OPEN:
                t = clampf((float)dt / 120.0f, 0.0f, 1.0f);
                s->eye[0].top_lid_mid.dy -= ease_out(t) * 0.55f;
                s->eye[1].top_lid_mid.dy -= ease_out(t) * 0.55f;
                s->eye[0].pupil_scale    += ease_out(t) * 0.3f;
                s->eye[1].pupil_scale    += ease_out(t) * 0.3f;
                if (dt >= 120) { startle_phase = STARTLE_HOLD; startle_start = now; }
                break;
            case STARTLE_HOLD:
                s->eye[0].top_lid_mid.dy -= 0.55f;
                s->eye[1].top_lid_mid.dy -= 0.55f;
                s->eye[0].pupil_scale    += 0.3f;
                s->eye[1].pupil_scale    += 0.3f;
                if (dt >= 500) { startle_phase = STARTLE_CLOSE; startle_start = now; }
                break;
            case STARTLE_CLOSE:
                t = clampf((float)dt / 300.0f, 0.0f, 1.0f);
                s->eye[0].top_lid_mid.dy -= (1.0f - t) * 0.55f;
                s->eye[1].top_lid_mid.dy -= (1.0f - t) * 0.55f;
                s->eye[0].pupil_scale    += (1.0f - t) * 0.3f;
                s->eye[1].pupil_scale    += (1.0f - t) * 0.3f;
                if (dt >= 300) {
                    startle_phase = STARTLE_IDLE;
                    next_startle_at = now + rand_ms(12000, 20000);
                }
                break;
            default: break;
            }
        }
    } else {
        startle_phase = STARTLE_IDLE;
    }

    /* ══════════════════════════════════════════════════════
       7. Entry Impact (squash-and-stretch)
       ══════════════════════════════════════════════════════ */
    if (g_impact.active) {
        uint32_t elapsed = now - g_impact.start_ms;
        if (elapsed >= g_impact.duration_ms) {
            g_impact.active = false;
        } else {
            float env;
            if (g_impact.peak2_ms > 0) {
                /* Two-peak (EXCITED): two independent bell curves */
                float e1 = 0.0f, e2 = 0.0f;
                if (elapsed < g_impact.peak2_ms) {
                    float t1 = (float)elapsed / (float)g_impact.peak2_ms;
                    e1 = sinf(t1 * 3.14159265f);
                }
                if (elapsed >= g_impact.peak2_ms) {
                    float t2 = (float)(elapsed - g_impact.peak2_ms)
                             / (float)(g_impact.duration_ms - g_impact.peak2_ms);
                    t2 = clampf(t2, 0.0f, 1.0f);
                    e2 = sinf(t2 * 3.14159265f);
                }
                s->face.squash_x  += g_impact.sq_peak * e1 + g_impact.sq2_peak * e2;
                s->face.stretch_y += g_impact.st_peak * e1 + g_impact.st2_peak * e2;
                env = e1 + e2;
            } else {
                /* Single-peak: bell curve over [0, duration_ms] */
                float t = (float)elapsed / (float)g_impact.duration_ms;
                env = sinf(t * 3.14159265f);
                s->face.squash_x  += g_impact.sq_peak * env;
                s->face.stretch_y += g_impact.st_peak * env;
            }
            if (g_impact.pupil_peak != 0.0f) {
                s->eye[0].pupil_scale += g_impact.pupil_peak * env;
                s->eye[1].pupil_scale += g_impact.pupil_peak * env;
            }
            if (g_impact.brow_peak != 0.0f) {
                s->brow[0].arch.dy += g_impact.brow_peak * env;
                s->brow[1].arch.dy += g_impact.brow_peak * env;
            }
        }
    }
}
