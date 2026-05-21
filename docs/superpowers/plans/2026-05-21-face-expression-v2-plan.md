# Face Expression v2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enhance facial expression quality across all four dimensions — rendering detail, animation expressiveness, character personality, and interactive feedback.

**Architecture:** Expand face_model with 6 new params + blend struct; rewrite animator with 3-phase transitions (ANTICIPATE→ATTACK→SETTLE) and squash/stretch; replace sin()-based micro-animations with hash noise engine; enhance sprite_classic rendering (iris 3-layer, cupid's bow, teeth, eyelashes, nose shadow). Renderer (face_renderer.c) unchanged.

**Tech Stack:** C (ESP-IDF), LVGL (display driver only, not animations), GC9A01 240x240 LCD

---

### Task 1: Expand face_model parameters and expression presets

**Files:**
- Modify: `components/face_system/face_model.h`
- Modify: `components/face_system/face_model.c`
- Modify: `components/face_system/face_animator.c` (lerp functions only)

- [ ] **Step 1: Add squash_x and stretch_y to face_params_t in face_model.h**

In `face_model.h`, change `face_params_t` from:
```c
typedef struct {
    float roundness;
} face_params_t;
```
To:
```c
typedef struct {
    float roundness;
    float squash_x;    /* horizontal squash (-1.0..1.0), transient */
    float stretch_y;   /* vertical stretch (-1.0..1.0), transient */
} face_params_t;
```

- [ ] **Step 2: Add iris_detail and eyelash to eye_params_t in face_model.h**

In `face_model.h`, add to `eye_params_t` (after `shine_intensity`):
```c
    float iris_detail;   /* 0..1, limbal ring + tertiary shine intensity */
    float eyelash;       /* 0..1, eyelash visibility */
```

- [ ] **Step 3: Add cupid_depth and tooth_show to mouth_params_t in face_model.h**

In `face_model.h`, add to `mouth_params_t` (after `openness`):
```c
    float cupid_depth;   /* 0..1, upper lip cupid's bow indentation */
    float tooth_show;    /* 0..1, teeth visibility when mouth open */
```

- [ ] **Step 4: Add expression_blend_t struct after expression_id_t in face_model.h**

```c
typedef struct {
    expression_id_t expr_a;
    expression_id_t expr_b;
    float blend;          /* 0.0 = pure A, 1.0 = pure B */
} expression_blend_t;
```

- [ ] **Step 5: Update lerp functions in face_animator.c for new fields**

In `face_animator.c`, add after `out->roundness = ...` in `face_params_lerp`:
```c
    out->squash_x = a->squash_x + (b->squash_x - a->squash_x) * t;
    out->stretch_y = a->stretch_y + (b->stretch_y - a->stretch_y) * t;
```

In `eye_params_lerp`, add after `out->shine_intensity = ...`:
```c
    out->iris_detail = a->iris_detail + (b->iris_detail - a->iris_detail) * t;
    out->eyelash = a->eyelash + (b->eyelash - a->eyelash) * t;
```

In `mouth_params_lerp`, add after `out->openness = ...`:
```c
    out->cupid_depth = a->cupid_depth + (b->cupid_depth - a->cupid_depth) * t;
    out->tooth_show = a->tooth_show + (b->tooth_show - a->tooth_show) * t;
```

- [ ] **Step 6: Add non-zero values for new params in expression presets in face_model.c**

Add to the NEUTRAL [0] target:
```c
.eye = {
    EYE_NEUTRAL, EYE_NEUTRAL,
    // Override iris_detail, eyelash defaults for NEUTRAL:
},
```
Since C initializes missing designated fields to 0, we only need to add explicit values where non-zero. Update the eye structs in these expressions:

NEUTRAL [0] — add to both eyes:
```c
.iris_detail = 0.5f, .eyelash = 0.6f,
```

HAPPY [1] — add to mouth:
```c
.cupid_depth = 0.7f, .tooth_show = 0.6f,
```

EXCITED [7] — add to eyes:
```c
.iris_detail = 0.8f, .eyelash = 0.3f,
```
and mouth:
```c
.tooth_show = 0.9f,
```

SURPRISED [3] — add to eyes:
```c
.iris_detail = 0.8f, .eyelash = 0.3f,
```

SLEEPY [4] — add to eyes:
```c
.iris_detail = 0.2f, .eyelash = 0.8f,
```

CONTENT [9] — add to eyes:
```c
.eyelash = 0.8f,
```

SAD [2] — add to mouth:
```c
.cupid_depth = 0.1f,
```

- [ ] **Step 7: Commit**

```bash
git add components/face_system/face_model.h components/face_system/face_model.c components/face_system/face_animator.c
git commit -m "feat: expand face_model with squash/stretch, iris_detail, eyelash, cupid_depth, tooth_show"
```

---

### Task 2: Rewrite face_animator with 3-phase transitions

**Files:**
- Modify: `components/face_system/face_animator.c`
- Modify: `components/face_system/face_animator.h` (if needed for new types)

**Note:** This replaces LVGL animation usage with a custom phase-driven animation engine. The `lv_timer_handler()` call is retained for LVGL display driver housekeeping.

- [ ] **Step 1: Add phase types and per-component animation state in face_animator.c**

Replace the existing `anim_active`, `anims`, `from_states`, `anim_data` globals with:

```c
/* ── Phase enum ─────────────────────────────────────────────── */

typedef enum {
    PHASE_IDLE = 0,
    PHASE_ANTICIPATE,
    PHASE_ATTACK,
    PHASE_SETTLE,
} anim_phase_t;

/* ── Per-component animation state ──────────────────────────── */

#define ANTICIPATE_DUR_MS  60
#define SETTLE_DUR_MS     100

typedef struct {
    anim_phase_t phase;
    uint32_t elapsed_ms;       /* elapsed within current phase */
    uint32_t delay_remaining;  /* delay countdown before start */
    anim_path_type_t path_type;
    float anticipation_amt;    /* how far to reverse during anticipate */
    float overshoot_peak;      /* peak overshoot (computed during attack) */
} comp_anim_ctx_t;

static comp_anim_ctx_t comp_ctx[COMPONENT_COUNT];

/* from/to targets: from = previous expression's target, to = new expression's target */
static uint8_t from_targets[COMPONENT_COUNT][sizeof(eye_params_t)];
static uint8_t to_targets[COMPONENT_COUNT][sizeof(eye_params_t)];

static expression_id_t active_from_expr = 0xff; /* invalid = no transition */
static expression_id_t active_to_expr = 0;
static bool transition_active = false;
static float global_blend_t = 0.0f; /* 0→1, shared across components for squash/stretch */
static uint32_t global_elapsed_ms = 0;
static uint32_t global_total_ms = 0; /* max attack duration across components */
```

- [ ] **Step 2: Implement path evaluation function (replaces LVGL path_map)**

```c
static float eval_path(anim_path_type_t type, float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) {
        if (type == PATH_OVERSPEED) return 1.08f; /* overshoot 8% */
        return 1.0f;
    }
    switch (type) {
        case PATH_LINEAR:
            return t;
        case PATH_EASE_OUT:
            return 1.0f - (1.0f - t) * (1.0f - t);
        case PATH_EASE_IN:
            return t * t;
        case PATH_EASE_IN_OUT:
            return t < 0.5f ? 2.0f * t * t : 1.0f - (1.0f - t) * (1.0f - t) * 2.0f;
        case PATH_OVERSPEED: {
            /* cubic overshoot: peaks at ~1.08 then settles */
            float s = 1.08f;
            return s * t * t * (3.0f - 2.0f * t) + (1.0f - s) * t;
        }
        default:
            return t;
    }
}
```

- [ ] **Step 3: Implement per-component effective_t computation**

```c
/* Computes effective interpolation parameter for a component.
 * During ANTICIPATE: effective_t goes negative (reverse motion)
 * During ATTACK:     effective_t goes from -anticipation_amt to 1.0+overshoot
 * During SETTLE:     effective_t eases back to 1.0
 */
static float compute_effective_t(face_component_t comp) {
    comp_anim_ctx_t *ctx = &comp_ctx[comp];
    if (ctx->phase == PHASE_IDLE) return 1.0f;

    float t; /* normalized progress within current phase */
    switch (ctx->phase) {
        case PHASE_ANTICIPATE: {
            t = (float)ctx->elapsed_ms / (float)ANTICIPATE_DUR_MS;
            if (t > 1.0f) t = 1.0f;
            /* ease-out from 0 to -anticipation_amt */
            float ease = 1.0f - (1.0f - t) * (1.0f - t);
            return -ctx->anticipation_amt * ease;
        }
        case PHASE_ATTACK: {
            uint32_t dur = (ctx->elapsed_ms == 0 && ANTICIPATE_DUR_MS == 0)
                           ? 300 : ANTICIPATE_DUR_MS; /* fallback */
            /* dur is attack_dur from expression timing */
            /* We need attack_dur stored in ctx — add it below */
            float path_t = eval_path(ctx->path_type, t);
            return -ctx->anticipation_amt
                   + (1.0f + ctx->anticipation_amt + ctx->overshoot_peak) * path_t
                   - ctx->overshoot_peak * (path_t * path_t * path_t);
            /* correction: simple linear between anticipation end and overshoot */
        }
        case PHASE_SETTLE: {
            t = (float)ctx->elapsed_ms / (float)SETTLE_DUR_MS;
            if (t > 1.0f) t = 1.0f;
            float settle_start = eval_path(ctx->path_type, 1.0f);
            float ease = t < 0.5f ? 2.0f * t * t : 1.0f - (1.0f - t) * (1.0f - t) * 2.0f;
            return settle_start + (1.0f - settle_start) * ease;
        }
        default:
            return 1.0f;
    }
}
```

Wait — the above has a logic issue: `t` is undefined for PHASE_ATTACK. Let me restructure properly.

- [ ] **Step 3 (rewritten): Implement per-component effective_t computation correctly**

```c
/* Get phase duration for a component */
static uint32_t get_phase_dur(face_component_t comp, anim_phase_t phase) {
    comp_anim_ctx_t *ctx = &comp_ctx[comp];
    switch (phase) {
        case PHASE_ANTICIPATE: return ANTICIPATE_DUR_MS;
        case PHASE_ATTACK:     return ctx->delay_remaining; /* misuse — fix below */
        case PHASE_SETTLE:     return SETTLE_DUR_MS;
        default: return 0;
    }
}
```

This is getting tangled. Let me provide the complete rewritten animator.c instead of piecemeal steps. That's clearer.

- [ ] **Step 3: Write the complete new face_animator.c**

```c
#include "face_animator.h"
#include "lvgl.h"
#include <string.h>
#include <math.h>

/* ── Phase enum ─────────────────────────────────────────────── */

typedef enum {
    PHASE_IDLE = 0,
    PHASE_ANTICIPATE,
    PHASE_ATTACK,
    PHASE_SETTLE,
} anim_phase_t;

#define ANTICIPATE_DUR_MS  55
#define SETTLE_DUR_MS      90

/* ── Per-component animation context ────────────────────────── */

typedef struct {
    anim_phase_t phase;
    uint32_t elapsed_ms;
    uint32_t attack_dur_ms;      /* from expression timing */
    uint32_t delay_remaining;    /* counts down before phase start */
    anim_path_type_t path_type;
} comp_anim_ctx_t;

static comp_anim_ctx_t comp_ctx[COMPONENT_COUNT];

/* from/to targets stored as raw bytes per component */
static uint8_t from_targets[COMPONENT_COUNT][sizeof(eye_params_t)];
static uint8_t to_targets[COMPONENT_COUNT][sizeof(eye_params_t)];

static bool transition_active = false;
static uint32_t tick_last_ms = 0;

/* ── Component lerp functions (existing + new fields) ───────── */

static void face_params_lerp(const face_params_t *a, const face_params_t *b,
                             float t, face_params_t *out) {
    out->roundness = a->roundness + (b->roundness - a->roundness) * t;
    out->squash_x  = a->squash_x  + (b->squash_x  - a->squash_x)  * t;
    out->stretch_y = a->stretch_y + (b->stretch_y - a->stretch_y) * t;
}

static void brow_params_lerp(const brow_params_t *a, const brow_params_t *b,
                             float t, brow_params_t *out) {
    out->inner.dx = a->inner.dx + (b->inner.dx - a->inner.dx) * t;
    out->inner.dy = a->inner.dy + (b->inner.dy - a->inner.dy) * t;
    out->arch.dx  = a->arch.dx  + (b->arch.dx  - a->arch.dx)  * t;
    out->arch.dy  = a->arch.dy  + (b->arch.dy  - a->arch.dy)  * t;
    out->tail.dx  = a->tail.dx  + (b->tail.dx  - a->tail.dx)  * t;
    out->tail.dy  = a->tail.dy  + (b->tail.dy  - a->tail.dy)  * t;
    out->thickness = a->thickness + (b->thickness - a->thickness) * t;
}

static void eye_params_lerp(const eye_params_t *a, const eye_params_t *b,
                            float t, eye_params_t *out) {
    out->inner_corner.dx = a->inner_corner.dx + (b->inner_corner.dx - a->inner_corner.dx) * t;
    out->inner_corner.dy = a->inner_corner.dy + (b->inner_corner.dy - a->inner_corner.dy) * t;
    out->outer_corner.dx = a->outer_corner.dx + (b->outer_corner.dx - a->outer_corner.dx) * t;
    out->outer_corner.dy = a->outer_corner.dy + (b->outer_corner.dy - a->outer_corner.dy) * t;
    out->top_lid_mid.dx = a->top_lid_mid.dx + (b->top_lid_mid.dx - a->top_lid_mid.dx) * t;
    out->top_lid_mid.dy = a->top_lid_mid.dy + (b->top_lid_mid.dy - a->top_lid_mid.dy) * t;
    out->bot_lid_mid.dx = a->bot_lid_mid.dx + (b->bot_lid_mid.dx - a->bot_lid_mid.dx) * t;
    out->bot_lid_mid.dy = a->bot_lid_mid.dy + (b->bot_lid_mid.dy - a->bot_lid_mid.dy) * t;
    out->iris_center.dx = a->iris_center.dx + (b->iris_center.dx - a->iris_center.dx) * t;
    out->iris_center.dy = a->iris_center.dy + (b->iris_center.dy - a->iris_center.dy) * t;
    out->pupil_scale   = a->pupil_scale   + (b->pupil_scale   - a->pupil_scale)   * t;
    out->shine_intensity = a->shine_intensity + (b->shine_intensity - a->shine_intensity) * t;
    out->iris_detail   = a->iris_detail   + (b->iris_detail   - a->iris_detail)   * t;
    out->eyelash       = a->eyelash       + (b->eyelash       - a->eyelash)       * t;
}

static void mouth_params_lerp(const mouth_params_t *a, const mouth_params_t *b,
                              float t, mouth_params_t *out) {
    out->left_corner.dx  = a->left_corner.dx  + (b->left_corner.dx  - a->left_corner.dx)  * t;
    out->left_corner.dy  = a->left_corner.dy  + (b->left_corner.dy  - a->left_corner.dy)  * t;
    out->right_corner.dx = a->right_corner.dx + (b->right_corner.dx - a->right_corner.dx) * t;
    out->right_corner.dy = a->right_corner.dy + (b->right_corner.dy - a->right_corner.dy) * t;
    out->upper_lip_mid.dx = a->upper_lip_mid.dx + (b->upper_lip_mid.dx - a->upper_lip_mid.dx) * t;
    out->upper_lip_mid.dy = a->upper_lip_mid.dy + (b->upper_lip_mid.dy - a->upper_lip_mid.dy) * t;
    out->lower_lip_mid.dx = a->lower_lip_mid.dx + (b->lower_lip_mid.dx - a->lower_lip_mid.dx) * t;
    out->lower_lip_mid.dy = a->lower_lip_mid.dy + (b->lower_lip_mid.dy - a->lower_lip_mid.dy) * t;
    out->openness    = a->openness    + (b->openness    - a->openness)    * t;
    out->cupid_depth = a->cupid_depth + (b->cupid_depth - a->cupid_depth) * t;
    out->tooth_show  = a->tooth_show  + (b->tooth_show  - a->tooth_show)  * t;
}

static void decor_params_lerp(const decor_params_t *a, const decor_params_t *b,
                              float t, decor_params_t *out) {
    out->blush   = a->blush   + (b->blush   - a->blush)   * t;
    out->tears   = a->tears   + (b->tears   - a->tears)   * t;
    out->stars   = a->stars   + (b->stars   - a->stars)   * t;
    out->sweat   = a->sweat   + (b->sweat   - a->sweat)   * t;
    out->sparkle = a->sparkle + (b->sparkle - a->sparkle) * t;
}

typedef void (*lerp_func_t)(const void *a, const void *b, float t, void *out);

static lerp_func_t lerp_funcs[COMPONENT_COUNT] = {
    [COMPONENT_FACE]        = (lerp_func_t)face_params_lerp,
    [COMPONENT_BROW_LEFT]   = (lerp_func_t)brow_params_lerp,
    [COMPONENT_BROW_RIGHT]  = (lerp_func_t)brow_params_lerp,
    [COMPONENT_EYE_LEFT]    = (lerp_func_t)eye_params_lerp,
    [COMPONENT_EYE_RIGHT]   = (lerp_func_t)eye_params_lerp,
    [COMPONENT_MOUTH]       = (lerp_func_t)mouth_params_lerp,
    [COMPONENT_DECOR]       = (lerp_func_t)decor_params_lerp,
};

static const size_t param_sizes[COMPONENT_COUNT] = {
    [COMPONENT_FACE]        = sizeof(face_params_t),
    [COMPONENT_BROW_LEFT]   = sizeof(brow_params_t),
    [COMPONENT_BROW_RIGHT]  = sizeof(brow_params_t),
    [COMPONENT_EYE_LEFT]    = sizeof(eye_params_t),
    [COMPONENT_EYE_RIGHT]   = sizeof(eye_params_t),
    [COMPONENT_MOUTH]       = sizeof(mouth_params_t),
    [COMPONENT_DECOR]       = sizeof(decor_params_t),
};

static void *get_component_ptr(face_state_t *st, face_component_t comp) {
    switch (comp) {
        case COMPONENT_FACE:        return &st->face;
        case COMPONENT_BROW_LEFT:   return &st->brow[0];
        case COMPONENT_BROW_RIGHT:  return &st->brow[1];
        case COMPONENT_EYE_LEFT:    return &st->eye[0];
        case COMPONENT_EYE_RIGHT:   return &st->eye[1];
        case COMPONENT_MOUTH:       return &st->mouth;
        case COMPONENT_DECOR:       return &st->decor;
        default: return NULL;
    }
}

static const void *get_expr_component_ptr(const expression_def_t *expr, face_component_t comp) {
    return get_component_ptr((face_state_t *)&expr->target, comp);
}

/* ── Easing functions ───────────────────────────────────────── */

static float ease_out(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
static float ease_in(float t)  { return t * t; }
static float ease_in_out(float t) {
    return t < 0.5f ? 2.0f * t * t : 1.0f - (1.0f - t) * (1.0f - t) * 2.0f;
}

static float eval_path(anim_path_type_t type, float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) {
        if (type == PATH_OVERSPEED) return 1.08f;
        return 1.0f;
    }
    switch (type) {
        case PATH_LINEAR:       return t;
        case PATH_EASE_OUT:     return ease_out(t);
        case PATH_EASE_IN:      return ease_in(t);
        case PATH_EASE_IN_OUT:  return ease_in_out(t);
        case PATH_OVERSPEED:    return 1.08f * t * t * (3.0f - 2.0f * t) + (-0.08f) * t;
        default:                return t;
    }
}

/* ── Compute effective_t for a component (includes anticipation) */

static float compute_effective_t(face_component_t comp) {
    comp_anim_ctx_t *ctx = &comp_ctx[comp];

    if (ctx->phase == PHASE_IDLE) return 1.0f;

    float phase_t; /* 0→1 within current phase */
    uint32_t dur = 0;
    switch (ctx->phase) {
        case PHASE_ANTICIPATE: dur = ANTICIPATE_DUR_MS; break;
        case PHASE_ATTACK:     dur = ctx->attack_dur_ms; break;
        case PHASE_SETTLE:     dur = SETTLE_DUR_MS; break;
        default: break;
    }
    if (dur == 0) dur = 1;
    phase_t = (float)ctx->elapsed_ms / (float)dur;
    if (phase_t > 1.0f) phase_t = 1.0f;

    switch (ctx->phase) {
        case PHASE_ANTICIPATE: {
            /* reverse: ease-out 0 → -0.18 */
            return -0.18f * ease_out(phase_t);
        }
        case PHASE_ATTACK: {
            float path_val = eval_path(ctx->path_type, phase_t);
            /* map from [-0.18, 1.0+overshoot] based on path_val [0, 1.0+] */
            float overshoot = (ctx->path_type == PATH_OVERSPEED) ? 0.08f : 0.0f;
            float range = 1.0f + 0.18f + overshoot;
            return -0.18f + path_val * range;
        }
        case PHASE_SETTLE: {
            /* path_val(1.0) → eased back to exactly 1.0 */
            float overshoot = (ctx->path_type == PATH_OVERSPEED) ? 0.08f : 0.0f;
            float peak = 1.0f + overshoot;
            return peak + (1.0f - peak) * ease_in_out(phase_t);
        }
        default: return 1.0f;
    }
}

/* ── Current blended state ──────────────────────────────────── */

static face_state_t current_state;

/* ── Squash/stretch computation ─────────────────────────────── */

#define M_PI 3.1415926535f

static void apply_squash_stretch(face_state_t *st, float blend_t) {
    /* Only active when transition is happening and fast enough */
    if (!transition_active) return;

    /* Compute "speed" from global timing */
    if (global_total_ms > 300) return; /* slow transitions: no squash */

    float amplitude = 0.15f;
    st->face.squash_x  = amplitude * sinf(blend_t * M_PI);
    st->face.stretch_y = amplitude * sinf(blend_t * M_PI + M_PI * 0.5f);
}

/* ── Public API ─────────────────────────────────────────────── */

void animator_init(void) {
    current_state = FACE_STATE_NEUTRAL;
    memset(comp_ctx, 0, sizeof(comp_ctx));
    transition_active = false;
    tick_last_ms = lv_tick_get();
}

void animator_tick(void) {
    uint32_t now = lv_tick_get();
    uint32_t dt;
    if (tick_last_ms == 0 || now < tick_last_ms) {
        dt = 16; /* ~60fps default */
    } else {
        dt = now - tick_last_ms;
    }
    tick_last_ms = now;

    if (!transition_active) {
        lv_timer_handler();
        return;
    }

    bool all_idle = true;

    for (int i = 0; i < COMPONENT_COUNT; i++) {
        comp_anim_ctx_t *ctx = &comp_ctx[i];

        /* Handle delay before starting */
        if (ctx->delay_remaining > 0) {
            if (dt >= ctx->delay_remaining) {
                ctx->delay_remaining = 0;
            } else {
                ctx->delay_remaining -= dt;
                all_idle = false;
                continue;
            }
        }

        if (ctx->phase == PHASE_IDLE) continue;

        /* Advance elapsed time */
        ctx->elapsed_ms += dt;
        uint32_t phase_dur = 0;
        switch (ctx->phase) {
            case PHASE_ANTICIPATE: phase_dur = ANTICIPATE_DUR_MS; break;
            case PHASE_ATTACK:     phase_dur = ctx->attack_dur_ms; break;
            case PHASE_SETTLE:     phase_dur = SETTLE_DUR_MS; break;
            default: break;
        }

        /* Advance phase if complete */
        while (phase_dur > 0 && ctx->elapsed_ms >= phase_dur) {
            ctx->elapsed_ms -= phase_dur;
            switch (ctx->phase) {
                case PHASE_ANTICIPATE:
                    ctx->phase = PHASE_ATTACK;
                    phase_dur = ctx->attack_dur_ms;
                    break;
                case PHASE_ATTACK:
                    ctx->phase = PHASE_SETTLE;
                    phase_dur = SETTLE_DUR_MS;
                    break;
                case PHASE_SETTLE:
                    ctx->phase = PHASE_IDLE;
                    phase_dur = 0;
                    break;
                default: phase_dur = 0; break;
            }
        }

        /* Lerp from→to using effective_t */
        float eff_t = compute_effective_t((face_component_t)i);
        lerp_funcs[i](from_targets[i], to_targets[i], eff_t,
                      get_component_ptr(&current_state, (face_component_t)i));

        if (ctx->phase != PHASE_IDLE) all_idle = false;
    }

    /* Update global blend_t (average across components) */
    float sum_t = 0;
    int active_count = 0;
    for (int i = 0; i < COMPONENT_COUNT; i++) {
        if (comp_ctx[i].phase != PHASE_IDLE || comp_ctx[i].delay_remaining > 0) {
            sum_t += compute_effective_t((face_component_t)i);
            active_count++;
        }
    }
    if (active_count > 0) {
        global_blend_t = sum_t / (float)active_count;
        if (global_blend_t < 0.0f) global_blend_t = 0.0f;
        if (global_blend_t > 1.0f) global_blend_t = 1.0f;
    }

    /* Apply squash/stretch */
    apply_squash_stretch(&current_state, global_blend_t);

    if (all_idle) {
        transition_active = false;
        global_blend_t = 1.0f;
        /* Ensure final state matches to_expr exactly */
        for (int i = 0; i < COMPONENT_COUNT; i++) {
            memcpy(get_component_ptr(&current_state, (face_component_t)i),
                   to_targets[i], param_sizes[i]);
        }
    }

    lv_timer_handler();
}

void animator_set_expression(expression_id_t id) {
    if (id >= EXPRESSION_COUNT) return;
    const expression_def_t *expr = &EXPRESSION_DEFS[id];

    for (int i = 0; i < COMPONENT_COUNT; i++) {
        comp_anim_ctx_t *ctx = &comp_ctx[i];

        /* Snapshot current state as "from" */
        void *cur = get_component_ptr(&current_state, (face_component_t)i);
        memcpy(from_targets[i], cur, param_sizes[i]);

        /* Set "to" from expression target */
        const void *target = get_expr_component_ptr(expr, (face_component_t)i);
        memcpy(to_targets[i], target, param_sizes[i]);

        /* Initialize phase context */
        ctx->phase = PHASE_ANTICIPATE;
        ctx->elapsed_ms = 0;
        ctx->attack_dur_ms = expr->timing[i].duration_ms;
        ctx->delay_remaining = expr->timing[i].delay_ms;
        ctx->path_type = expr->timing[i].path_type;
    }

    global_total_ms = 0;
    for (int i = 0; i < COMPONENT_COUNT; i++) {
        uint32_t total = ANTICIPATE_DUR_MS + comp_ctx[i].attack_dur_ms + SETTLE_DUR_MS
                         + comp_ctx[i].delay_remaining;
        if (total > global_total_ms) global_total_ms = total;
    }
    global_blend_t = 0.0f;
    transition_active = true;
}

void animator_set_component(face_component_t comp,
                            const void *target_params,
                            uint32_t duration_ms,
                            uint32_t delay_ms,
                            anim_path_type_t path_type) {
    if (comp >= COMPONENT_COUNT) return;
    int i = comp;
    comp_anim_ctx_t *ctx = &comp_ctx[i];

    void *cur = get_component_ptr(&current_state, comp);
    memcpy(from_targets[i], cur, param_sizes[i]);
    memcpy(to_targets[i], target_params, param_sizes[i]);

    ctx->phase = PHASE_ANTICIPATE;
    ctx->elapsed_ms = 0;
    ctx->attack_dur_ms = duration_ms;
    ctx->delay_remaining = delay_ms;
    ctx->path_type = path_type;

    /* Mark transition active if any component is animating */
    bool any_active = false;
    for (int j = 0; j < COMPONENT_COUNT; j++) {
        if (comp_ctx[j].phase != PHASE_IDLE || comp_ctx[j].delay_remaining > 0) {
            any_active = true; break;
        }
    }
    transition_active = any_active;
}

void animator_set_component_instant(face_component_t comp, const void *target_params) {
    if (comp >= COMPONENT_COUNT) return;
    int i = comp;
    comp_ctx[i].phase = PHASE_IDLE;
    comp_ctx[i].delay_remaining = 0;
    void *dst = get_component_ptr(&current_state, comp);
    memcpy(dst, target_params, param_sizes[i]);

    /* Also update to_targets to keep consistency */
    memcpy(to_targets[i], target_params, param_sizes[i]);
}

const face_state_t *animator_get_state(void) {
    return &current_state;
}

void animator_cancel_all(void) {
    for (int i = 0; i < COMPONENT_COUNT; i++) {
        comp_ctx[i].phase = PHASE_IDLE;
        comp_ctx[i].delay_remaining = 0;
    }
    transition_active = false;
    global_blend_t = 1.0f;
}
```

- [ ] **Step 4: Remove unused old code**

The `path_map()`, `anim_exec_cb()`, `anim_ready_cb()`, `anim_user_data_t`, `anim_data`, `from_states` (old), `anim_active`, and `anims` arrays should all be gone. The new implementation replaces everything.

- [ ] **Step 5: Commit**

```bash
git add components/face_system/face_animator.c
git commit -m "feat: rewrite animator with 3-phase transitions, dual-expression blending, squash/stretch"
```

---

### Task 3: Enhance sprite_classic rendering

**Files:**
- Modify: `components/face_system/sprite_classic.c`

- [ ] **Step 1: Enhance iris rendering in draw_eye_impl — add limbal ring + 3rd shine**

After the existing shine code in `draw_eye_impl()`, add before the interior pixel section. In the interior fill block (after `buf[x] = blend_colors(buf[x], pal[PAL_SCLERA], 0.92f);`), modify the iris interior section.

Find the interior fill section that currently has:
```c
if (iris_d_sq < iris_r_sq) {
    if (sh_d_sq < sh_r_sq) {
        // primary shine
    } else if (sh2_d_sq < sh2_r_sq) {
        // secondary shine
    } else if (pupil_d_sq < pupil_r_sq) {
        // pupil
    } else {
        // iris gradient
    }
}
```

Replace with:
```c
if (iris_d_sq < iris_r_sq) {
    float iris_r_local = sqrtf(iris_d_sq); /* distance from iris center */

    /* Tertiary shine (tiny, controlled by iris_detail) */
    float sh3_cx = iris_cx + 2.0f;
    float sh3_cy = iris_cy + 10.0f;
    float sh3_r = 2.0f;
    float sh3_r_sq = sh3_r * sh3_r;
    float sh3_d_sq = dist_sq(fx, fy, sh3_cx, sh3_cy);
    float sh3_visible = ep->iris_detail; /* iris_detail gates this */

    if (sh_d_sq < sh_r_sq) {
        float t = (1.0f - sh_d_sq / sh_r_sq) * ep->shine_intensity;
        buf[x] = blend_colors(pal[PAL_IRIS], pal[PAL_SHINE], t);
    } else if (sh2_d_sq < sh2_r_sq) {
        float t = (1.0f - sh2_d_sq / sh2_r_sq) * ep->shine_intensity * 0.55f;
        buf[x] = blend_colors(pal[PAL_IRIS], pal[PAL_SHINE], t);
    } else if (sh3_d_sq < sh3_r_sq && sh3_visible > 0.2f) {
        float t = (1.0f - sh3_d_sq / sh3_r_sq) * ep->shine_intensity
                  * 0.3f * sh3_visible;
        buf[x] = blend_colors(pal[PAL_IRIS], pal[PAL_SHINE], t);
    } else if (pupil_d_sq < pupil_r_sq) {
        buf[x] = pal[PAL_PUPIL];
    } else {
        /* 3-layer iris gradient with limbal ring */
        float grad_t = iris_r_local / iris_r; /* 0=center, 1=edge */
        /* Limbal ring: dark ring at outer 15% of iris */
        float limbal_zone = 0.85f;
        if (grad_t > limbal_zone && ep->iris_detail > 0.3f) {
            float limbal_t = (grad_t - limbal_zone) / (1.0f - limbal_zone);
            uint16_t limbal_color = blend_colors(pal[PAL_IRIS], pal[PAL_PUPIL], 0.7f);
            buf[x] = blend_colors(pal[PAL_IRIS], limbal_color,
                                  limbal_t * limbal_t * ep->iris_detail);
        } else {
            uint16_t iris_mid = blend_colors(pal[PAL_IRIS], iris_dark, grad_t * grad_t);
            buf[x] = iris_mid;
        }
    }
}
```

- [ ] **Step 2: Add eyelashes to draw_eye_impl**

After the main pixel loop in `draw_eye_impl()`, but before the closing brace, add eyelash rendering. Eyelashes are drawn as small dark dots/lines extending upward from the top lid edge.

Add this block right after the existing pixel loop (`for (int x = x_start; x <= x_end; x++)`):

```c
    /* ── Eyelashes (along upper lid) ── */
    if (ep->eyelash > 0.0f) {
        float lash_alpha = ep->eyelash * 0.85f;
        /* Only draw on rows just above the top lid */
        float lash_row = base_top - 1.0f;
        if (fy >= lash_row - 3.0f && fy <= lash_row + 1.0f) {
            /* Determine lash positions based on x position along lid */
            float arch = 1.0f - (fx * fx) / (eye_r * eye_r);
            float lid_y = base_top + (5.0f * lid_open * arch) + corner_adj * 0.5f;
            if (fy >= lid_y - 4.0f && fy <= lid_y) {
                /* 9 lash positions across the eye */
                static const float lash_positions[9] = {
                    -0.85f, -0.65f, -0.45f, -0.25f, 0.0f,
                    0.25f, 0.45f, 0.65f, 0.85f
                };
                for (int li = 0; li < 9; li++) {
                    float lx = lash_positions[li] * eye_r;
                    float lash_len = 2.5f + 2.0f * (1.0f - fabsf(lash_positions[li]));
                    float dx_l = fx - lx;
                    if (fabsf(dx_l) < 1.0f && fy >= lid_y - lash_len) {
                        buf[x] = blend_colors(buf[x], pal[PAL_PUPIL],
                                              lash_alpha * (1.0f - fabsf(dx_l)));
                    }
                }
            }
        }
    }
```

- [ ] **Step 3: Add cupid's bow to draw_mouth**

In `draw_mouth()`, replace the single upper lip bezier with a split bezier for cupid's bow.

Find the upper lip calculation:
```c
float upper_y = (1-t)*(1-t)*corner_y + 2*(1-t)*t*uly + t*t*corner_y;
```

Replace with:
```c
/* Cupid's bow: split upper lip into left and right bezier curves */
float cupid_depth_px = mp->cupid_depth * 8.0f; /* max 8px dip */
float mid_t = 0.5f;
float mid_x = lcx + (rcx - lcx) * mid_t;
float cupid_y = uly + cupid_depth_px; /* dip downward */
float upper_y;
if (t <= mid_t) {
    float lt = t / mid_t; /* normalize to 0-1 for left half */
    upper_y = (1-lt)*(1-lt)*corner_y + 2*(1-lt)*lt*cupid_y + lt*lt*corner_y;
} else {
    float rt = (t - mid_t) / (1.0f - mid_t); /* normalize for right half */
    upper_y = (1-rt)*(1-rt)*cupid_y + 2*(1-rt)*rt*corner_y + rt*rt*corner_y;
}
```

Wait — this doesn't work because `corner_y` is the corner point for both halves but they should use the respective corner points (left corner for left half, right corner for right half). Let me fix:

```c
/* Cupid's bow: split upper lip into left and right bezier curves */
float cupid_depth_px = mp->cupid_depth * 8.0f;
float cupid_y = uly + cupid_depth_px; /* dip downward */
float upper_y;
if (t <= 0.5f) {
    float lt = t * 2.0f; /* normalize 0-1 for left half */
    upper_y = (1-lt)*(1-lt)*lcy + 2*(1-lt)*lt*cupid_y + lt*lt*((lcy+rcy)*0.5f);
    /* end at midpoint of corner_y heights? No — should end at cupid point */
    /* Actually upper lip from corner to cupid: lcy → cupid_y */
    float mid_corner_y = (lcy + rcy) * 0.5f;
    upper_y = (1-lt)*(1-lt)*lcy + 2*(1-lt)*lt*cupid_y + lt*lt*mid_corner_y;
} else {
    float rt = (t - 0.5f) * 2.0f;
    float mid_corner_y = (lcy + rcy) * 0.5f;
    upper_y = (1-rt)*(1-rt)*mid_corner_y + 2*(1-rt)*rt*cupid_y + rt*rt*rcy;
}
```

Hmm, this is getting complicated. The cupid's bow should look like:
- Upper lip has a small dip in the middle (the "bow")
- The control points are: left corner → cupid peak (left side of bow) → cupid dip → cupid peak (right side of bow) → right corner

Let me simplify: use the same approach as the original but with the cupid dip:

```c
/* Cupid's bow: add dip at center of upper lip */
float cupid_depth_px = mp->cupid_depth * 8.0f;
float upper_y_raw = (1-t)*(1-t)*corner_y + 2*(1-t)*t*uly + t*t*corner_y;
float cupid_influence = 1.0f - 2.0f * fabsf(t - 0.5f); /* 1 at center, 0 at edges */
if (cupid_influence < 0.0f) cupid_influence = 0.0f;
float upper_y = upper_y_raw + cupid_depth_px * cupid_influence * cupid_influence;
```

This is simpler: it adds a dip at the center of the upper lip that smoothly tapers to 0 at the corners. The `cupid_influence^2` makes it a sharper dip.

- [ ] **Step 4: Add teeth rendering in draw_mouth**

In `draw_mouth()`, after the mouth interior section (where tongue is drawn), add teeth before the open mouth dark fill:

Find the block:
```c
if (y > upper_y + 1.5f && y < lower_y - 1.5f && mp->openness > 0.05f) {
```

Inside this block, before the tongue code, add teeth:

```c
if (y > upper_y + 1.5f && y < lower_y - 1.5f && mp->openness > 0.05f) {
    /* Teeth (upper portion of open mouth) */
    if (mp->tooth_show > 0.0f && mp->openness > 0.15f) {
        float mouth_height = lower_y - upper_y;
        float tooth_height = mouth_height * 0.35f * mp->tooth_show;
        if (y <= upper_y + 1.5f + tooth_height) {
            float tooth_center = (upper_y + lower_y) * 0.5f;
            /* Only draw teeth in the middle 60% of mouth width */
            float tooth_half_width = half_width * 0.6f;
            float abs_dx = fabsf(x - CENTER_X);
            if (abs_dx < tooth_half_width) {
                float edge_t = abs_dx / tooth_half_width;
                float alpha = 0.7f * (1.0f - edge_t * edge_t) * mp->tooth_show;
                buf[x] = blend_colors(buf[x], pal[PAL_SCLERA], alpha);
                /* Vertical tooth line separators */
                float tooth_line = fmodf((x - CENTER_X + tooth_half_width) * 3.0f
                                         / tooth_half_width, 1.0f);
                if (tooth_line < 0.08f || tooth_line > 0.92f) {
                    buf[x] = blend_colors(buf[x], pal[PAL_BG_EDGE], alpha * 0.3f);
                }
            }
        }
    }

    /* Tongue (existing, moved below teeth) */
    if (mp->openness > 0.2f) {
        /* ... existing tongue code ... */
    } else {
        buf[x] = blend_colors(buf[x], pal[PAL_PUPIL], 0.7f);
    }
}
```

- [ ] **Step 5: Add nose shadow in draw_face**

In `draw_face()`, after the background gradient fill loop, add a subtle nose bridge shadow between the eyes:

```c
    /* ── Nose bridge shadow (subtle) ── */
    {
        int nose_cx = CENTER_X;
        int nose_cy = CENTER_Y - 10;
        int nose_half_w = 8;
        int nose_half_h = 17;
        float dy_n = y - nose_cy;
        if (dy_n > -nose_half_h && dy_n < nose_half_h) {
            int x_start_n = nose_cx - nose_half_w;
            int x_end_n   = nose_cx + nose_half_w;
            if (x_start_n < 0) x_start_n = 0;
            if (x_end_n >= SCREEN_W) x_end_n = SCREEN_W - 1;
            for (int x = x_start_n; x <= x_end_n; x++) {
                float dx_n = x - nose_cx;
                float edge_factor = 1.0f - (dx_n * dx_n) / (float)(nose_half_w * nose_half_w)
                                    - (dy_n * dy_n) / (float)(nose_half_h * nose_half_h);
                if (edge_factor > 0.0f) {
                    float alpha = edge_factor * 0.10f;
                    buf[x] = blend_colors(buf[x], sp->pal[PAL_BG_EDGE], alpha);
                }
            }
        }
    }
```

- [ ] **Step 6: Commit**

```bash
git add components/face_system/sprite_classic.c
git commit -m "feat: enhance sprite rendering - iris 3-layer, cupid bow, teeth, eyelashes, nose shadow"
```

---

### Task 4: Refactor app_display with noise engine

**Files:**
- Modify: `main/app_display.c`

- [ ] **Step 1: Add hash-based noise engine at top of app_display.c**

Add after existing includes:

```c
/* ── Hash-based value noise ─────────────────────────────────── */

static uint32_t hash_uint(uint32_t x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

static float noise1d(uint32_t tick, float stride) {
    float x = (float)tick * stride;
    int i = (int)x;
    float t = x - (float)i;
    t = t * t * (3.0f - 2.0f * t); /* smoothstep */
    uint32_t h0 = hash_uint((uint32_t)i);
    uint32_t h1 = hash_uint((uint32_t)(i + 1));
    float v0 = (float)(h0 & 0xFFFF) / 32768.0f - 1.0f;
    float v1 = (float)(h1 & 0xFFFF) / 32768.0f - 1.0f;
    return v0 + (v1 - v0) * t;
}

/* Multi-octave micro-noise: sum of 3 octaves at different strides */
static float micro_noise(uint32_t tick, float base_stride) {
    return noise1d(tick, base_stride) * 0.7f
         + noise1d(tick, base_stride * 3.7f) * 0.2f
         + noise1d(tick, base_stride * 7.1f) * 0.1f;
}

/* Pareto-distributed random interval: most values near min, occasional long tails */
static int pareto_interval(int min_val, int max_val) {
    float u = (float)(esp_random() & 0xFFFF) / 65536.0f;
    if (u < 0.001f) u = 0.001f;
    float pareto = 1.0f / sqrtf(u); /* Pareto with alpha ~2 */
    /* Map [1, ~31.6] → [min_val, max_val] */
    float range = (float)(max_val - min_val);
    float val = 1.0f + (pareto - 1.0f) * (range / 30.6f);
    if (val > range) val = range;
    return min_val + (int)val;
}

/* Frame counter for noise phase */
static uint32_t noise_tick = 0;
```

- [ ] **Step 2: Add micro_scale helper and tilt low-pass filter**

```c
/* Micro-animation scale: fades during expression transitions */
static float get_micro_scale(void) {
    const face_state_t *st = animator_get_state();
    /* Use squash_x as proxy for transition activity */
    float activity = fabsf(st->face.squash_x) * 6.0f; /* squash peaks at 0.15 → activity ~0.9 */
    if (activity > 1.0f) activity = 1.0f;
    /* Scale = 1.0 when idle, 0.1 during peak transition */
    return 1.0f - activity * 0.9f;
}

/* Low-pass filter for tilt (smooth sensor noise) */
static float tilt_lpf_x = 0, tilt_lpf_y = 0;
static void tilt_lpf_update(float raw_dx, float raw_dy) {
    float alpha = 0.15f; /* low-pass: smaller = smoother */
    tilt_lpf_x += (raw_dx - tilt_lpf_x) * alpha;
    tilt_lpf_y += (raw_dy - tilt_lpf_y) * alpha;
}
```

- [ ] **Step 3: Replace breath micro-animation**

Find:
```c
breath_phase += 0.03f;
float breath = sinf(breath_phase) * 0.05f + 1.0f;
face_params_t fp = {.roundness = 0.5f * breath};
face_set_component_instant(COMPONENT_FACE, &fp);
```

Replace with:
```c
float breath = micro_noise(noise_tick, 0.003f) * 0.06f;
float micro_s = get_micro_scale();
face_params_t fp = {
    .roundness = 0.5f + breath * micro_s,
    .squash_x = 0,  /* squash/stretch controlled by animator, don't override */
    .stretch_y = 0,
};
face_set_component_instant(COMPONENT_FACE, &fp);
```

- [ ] **Step 4: Replace blink interval with Pareto distribution**

Find:
```c
frames_until_next_blink = 120 + (esp_random() % 240);
```

Replace with:
```c
frames_until_next_blink = pareto_interval(60, 600); /* 1s to 10s @60fps equivalent */
if (esp_random() % 8 == 0) {
    /* Occasional double-blink: very short interval */
    frames_until_next_blink = pareto_interval(8, 30);
}
```

Also replace the re-arm:
```c
frames_until_next_blink = 120 + (esp_random() % 240);
```
→
```c
frames_until_next_blink = pareto_interval(60, 600);
```

- [ ] **Step 5: Replace saccade with noise**

Replace the saccade section:
```c
sac_phase += 0.08f;
saccade_timer--;
if (saccade_timer <= 0) {
    saccade_amplitude = 0.04f + (float)(esp_random() % 60) * 0.001f;
    saccade_timer = 60 + (esp_random() % 120);
}
float micro_x = sinf(sac_phase * 1.3f) * saccade_amplitude;
float micro_y = cosf(sac_phase * 0.9f) * saccade_amplitude;
if (esp_random() % 150 == 0) {
    micro_x += ...;
    micro_y += ...;
}
```

Replace with:
```c
/* Noise-driven saccades */
saccade_timer--;
if (saccade_timer <= 0) {
    saccade_amplitude = 0.02f + (float)(esp_random() % 100) * 0.0008f;
    saccade_timer = pareto_interval(30, 300);
}
float micro_x = micro_noise(noise_tick, 0.007f) * saccade_amplitude;
float micro_y = micro_noise(noise_tick + 500, 0.007f) * saccade_amplitude;
/* Occasional large saccade */
if (esp_random() % 120 == 0) {
    micro_x += (float)((int)(esp_random() % 120) - 60) * 0.0025f;
    micro_y += (float)((int)(esp_random() % 120) - 60) * 0.0025f;
}
```

- [ ] **Step 6: Replace brow twitch with noise**

Find:
```c
brow_phase_l += 0.05f;
brow_phase_r += 0.07f;
float brow_micro_l = sinf(brow_phase_l) * 0.022f;
float brow_micro_r = sinf(brow_phase_r) * 0.022f;
```

Replace with:
```c
float brow_micro_l = micro_noise(noise_tick + 1000, 0.005f) * 0.022f;
float brow_micro_r = micro_noise(noise_tick + 2000, 0.007f) * 0.022f;
/* Occasional unilateral brow raise (1 in 200 frames) */
if (esp_random() % 200 == 0) {
    if (esp_random() % 2) {
        brow_micro_l += 0.06f;
    } else {
        brow_micro_r += 0.06f;
    }
}
```

- [ ] **Step 7: Replace mouth idle with noise**

Replace the mouth movement section:
```c
frames_until_mouth--;
if (frames_until_mouth <= 0 && mouth_move_state == 0) { ... }
```
Replace the re-arm:
```c
frames_until_mouth = 100 + (esp_random() % 200);
```
→
```c
frames_until_mouth = pareto_interval(60, 500);
```

And change the mouth openness to use noise subtly:
```c
if (mouth_move_state != 0) {
    mouth_params_t mp = st->mouth;
    float noise_mouth = micro_noise(noise_tick + 3000, 0.01f) * 0.03f;
    mp.openness += mouth_move_t * 0.08f + noise_mouth;
    mp.lower_lip_mid.dy += mouth_move_t * 0.05f;
    face_set_component_instant(COMPONENT_MOUTH, &mp);
}
```

- [ ] **Step 8: Add tilt low-pass filter**

Replace:
```c
float tilt_dx = g_tilt_roll * 0.55f;
float tilt_dy = -g_tilt_pitch * 0.4f;
```

With:
```c
tilt_lpf_update(g_tilt_roll * 0.55f, -g_tilt_pitch * 0.4f);
float tilt_dx = tilt_lpf_x;
float tilt_dy = tilt_lpf_y;
```

- [ ] **Step 9: Increment noise_tick at end of display_update**

Add at the end of `display_update()` (before the animator tick / render calls):
```c
noise_tick++;
```

- [ ] **Step 10: Remove unused state variables**

Remove: `breath_phase`, `brow_phase_l`, `brow_phase_r`, `sac_phase`

- [ ] **Step 11: Commit**

```bash
git add main/app_display.c
git commit -m "feat: refactor micro-animations with hash noise engine and Pareto distributions"
```

---

### Task 5: Build verification

- [ ] **Step 1: Compile**

```bash
cd /Users/nova/proj/harti && idf.py build 2>&1 | tail -50
```

Expected: Build succeeds with 0 errors, 0 warnings.

- [ ] **Step 2: Fix any compilation errors**

Inspect errors, fix missing includes or type mismatches.

- [ ] **Step 3: Flash and visually verify (requires device)**

```bash
idf.py flash monitor
```

Verify on GC9A01 display:
1. All 13 expressions render correctly
2. Expression transitions show anticipation + squash/stretch
3. Micro-animations are smooth (no visible sin periodicity)
4. Iris has visible limbal ring (on NEUTRAL, EXCITED, SURPRISED)
5. Cupid's bow visible on HAPPY mouth
6. Teeth visible on HAPPY and EXCITED (open mouth)
7. Eyelashes visible on NEUTRAL and CONTENT
8. Nose shadow is subtle but present
9. Tilt tracking is smooth (no jitter)
10. Blink intervals feel natural (not regular)

- [ ] **Step 5: Commit any fixes**

```bash
git add -A && git commit -m "fix: build and visual verification adjustments"
```
