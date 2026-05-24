# Vector Vivid Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add two new modules (face_vivid + face_temperament) to bring prop dynamics, facial micro-motion, transition pulses, and temperament coloring to the vector sprite.

**Architecture:** Two independent C modules placed under `components/face_system/`. `face_vivid` applies per-frame sin-based offsets to active props and facial parameters. `face_temperament` applies transition overshoot pulses and temperament-driven parameter scaling. Both are called from `face_render_frame()` after existing micro/prop animators, modifying the display_state copy in-place.

**Tech Stack:** C (ESP-IDF), no test framework — verification via `idf.py build` + visual inspection on device

---

### Task 1: Create face_vivid.h

**Files:**
- Create: `components/face_system/face_vivid.h`

- [ ] **Step 1: Write the header**

```c
#ifndef FACE_VIVID_H
#define FACE_VIVID_H

#include "face_model.h"

#ifdef __cplusplus
extern "C" {
#endif

void face_vivid_init(void);
void face_vivid_apply(face_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 2: Commit**

```bash
git add components/face_system/face_vivid.h
git commit -m "feat: add face_vivid.h — header for prop dynamics + micro-motion module"
```

---

### Task 2: face_vivid.c — skeleton + prop dynamics (Part B)

**Files:**
- Create: `components/face_system/face_vivid.c`

- [ ] **Step 1: Write face_vivid.c with init + prop dynamics**

```c
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

void face_vivid_apply(face_state_t *state) {
    uint32_t now = lv_tick_get();
    float total_s = (float)(now - vivid_t0) / 1000.0f;

    /* ── Part B: Prop dynamics ─────────────────────────── */

    for (int i = 0; i < (int)state->decor.prop_count; i++) {
        prop_instance_t *p = &state->decor.props[i];
        float freq = prop_freq(p->type);
        if (freq <= 0.0f) continue;

        /* Detect new/changed prop: update snapshot */
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
        float s = sinf(phase);

        switch (p->type) {
        case PROP_TEACUP_STEAM:
            p->distance = snapshots[i].base_distance + s * 0.03f;
            p->scale    = snapshots[i].base_scale    + s * 0.05f;
            break;
        case PROP_MUSIC_NOTE:
            p->angle    = snapshots[i].base_angle + s * 0.05f;
            p->scale    = snapshots[i].base_scale + sinf(phase * 2.0f) * 0.08f;
            break;
        case PROP_SUNGLASSES:
            p->distance = snapshots[i].base_distance + s * 0.02f;
            break;
        case PROP_HEART:
            p->scale    = snapshots[i].base_scale + s * 0.06f;
            break;
        case PROP_STAR_SMALL:
            p->angle    = snapshots[i].base_angle + total_s * 0.3f;
            p->scale    = snapshots[i].base_scale + sinf(phase) * 0.04f;
            break;
        case PROP_TEACUP:
            p->distance = snapshots[i].base_distance + s * 0.02f;
            break;
        default:
            break;
        }
    }

    snapshot_init = true;
}
```

- [ ] **Step 2: Build to verify no compile errors**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py build
```

Expected: COMPILE SUCCESS (face_vivid.c compiles, face_vivid_apply not yet called)

- [ ] **Step 3: Commit**

```bash
git add components/face_system/face_vivid.c
git commit -m "feat: add face_vivid.c — prop dynamic animation (Part B)"
```

---

### Task 3: face_vivid.c — micro-motion (Part C)

**Files:**
- Modify: `components/face_system/face_vivid.c:87` (append after prop dynamics loop, before `snapshot_init = true`)

- [ ] **Step 1: Add micro-motion code**

Insert between the closing `}` of the prop loop and `snapshot_init = true;`:

```c
    /* ── Part C: Facial micro-motion ───────────────────── */

    float micro_t = total_s * 2.0f * 3.14159265f;

    /* Eye position drift (0.7Hz, anti-phase between eyes) */
    float eye_dx = sinf(micro_t * 0.7f) * 0.02f;
    float eye_dy = cosf(micro_t * 0.7f) * 0.02f;
    state->eye[0].position.dx += eye_dx;
    state->eye[0].position.dy += eye_dy;
    state->eye[1].position.dx -= eye_dx;
    state->eye[1].position.dy -= eye_dy;

    /* Brow arch quiver (1.3Hz, 30% time gate) */
    float gate = sinf(micro_t * 2.7f + 0.3f);  /* unrelated freq for gating */
    if (gate > 0.4f) {  /* ~30% of the time */
        float arch_delta = sinf(micro_t * 1.3f) * 0.015f;
        state->brow[0].arch.dy += arch_delta;
        state->brow[1].arch.dy += arch_delta;
    }

    /* Mouth corner micro-shift (0.4Hz, 90? out of phase with breathing) */
    float mouth_d = sinf(micro_t * 0.4f + 1.5708f) * 0.01f;
    state->mouth.left_corner.dx  += mouth_d;
    state->mouth.right_corner.dx -= mouth_d;

    /* Pupil micro-pulse (1.8Hz, heartbeat-like) */
    float pupil_pulse = sinf(micro_t * 1.8f) * 0.03f;
    state->eye[0].pupil_scale += pupil_pulse;
    state->eye[1].pupil_scale += pupil_pulse;
```

- [ ] **Step 2: Build**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py build
```

Expected: COMPILE SUCCESS

- [ ] **Step 3: Commit**

```bash
git add components/face_system/face_vivid.c
git commit -m "feat: add facial micro-motion to face_vivid (Part C)"
```

---

### Task 4: Create face_temperament.h

**Files:**
- Create: `components/face_system/face_temperament.h`

- [ ] **Step 1: Write header**

```c
#ifndef FACE_TEMPERAMENT_H
#define FACE_TEMPERAMENT_H

#include "face_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float energy;          /* 0.0..1.0, expression intensity */
    float responsiveness;  /* 0.0..1.0, reaction speed */
    float expressiveness;  /* 0.0..1.0, exaggeration */
    float quirk;           /* 0.0..1.0, random micro-expression probability */
} temperament_profile_t;

extern const temperament_profile_t TEMPERAMENT_DEFAULT;   /* {0.6, 0.5, 0.5, 0.1} */
extern const temperament_profile_t TEMPERAMENT_CHILL;     /* {0.3, 0.3, 0.3, 0.2} */
extern const temperament_profile_t TEMPERAMENT_DRAMATIC;  /* {0.9, 0.8, 0.9, 0.3} */

void face_temperament_init(void);
void face_temperament_set_profile(const temperament_profile_t *p);
void face_temperament_notify_expression_change(expression_id_t old_id, expression_id_t new_id);
void face_temperament_apply(face_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 2: Commit**

```bash
git add components/face_system/face_temperament.h
git commit -m "feat: add face_temperament.h — header for transition pulses + temperament"
```

---

### Task 5: face_temperament.c — skeleton + transition pulses (Part A)

**Files:**
- Create: `components/face_system/face_temperament.c`

- [ ] **Step 1: Write skeleton + pulse system**

```c
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
    temperament_profile_t current;   /* with jitter applied */

    /* Transition pulse */
    bool pulse_active;
    uint32_t pulse_start;
    expression_id_t pulse_target;

    /* Pulse values per-component */
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

/* Returns 1.0 at t=0, 0.0 at t>=decay_ms */
static float decay(uint32_t elapsed, uint32_t decay_ms) {
    if (elapsed >= decay_ms) return 0.0f;
    return 1.0f - (float)elapsed / (float)decay_ms;
}

/* ═══════════════════════════════════════════════════════════
   Preset profiles
   ═══════════════════════════════════════════════════════════ */

const temperament_profile_t TEMPERAMENT_DEFAULT  = {0.6f, 0.5f, 0.5f, 0.1f};
const temperament_profile_t TEMPERAMENT_CHILL    = {0.3f, 0.3f, 0.3f, 0.2f};
const temperament_profile_t TEMPERAMENT_DRAMATIC = {0.9f, 0.8f, 0.9f, 0.3f};

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

void face_temperament_notify_expression_change(expression_id_t old_id, expression_id_t new_id) {
    /* Apply jitter to profile */
    ctx.current.energy         = clampf(ctx.profile.energy         + randf_range(-0.08f, 0.08f), 0.0f, 1.0f);
    ctx.current.responsiveness = clampf(ctx.profile.responsiveness + randf_range(-0.08f, 0.08f), 0.0f, 1.0f);
    ctx.current.expressiveness = clampf(ctx.profile.expressiveness + randf_range(-0.08f, 0.08f), 0.0f, 1.0f);
    ctx.current.quirk          = clampf(ctx.profile.quirk          + randf_range(-0.08f, 0.08f), 0.0f, 1.0f);

    /* ── Set up transition pulse ───────────────────── */

    memset(&ctx.pulse, 0, sizeof(ctx.pulse));
    ctx.pulse_active = true;
    ctx.pulse_start  = lv_tick_get();
    ctx.pulse_target = new_id;

    switch (new_id) {
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

    /* ── Quirk: random micro-expression ───────────── */

    if (randf_range(0.0f, 1.0f) < ctx.current.quirk) {
        int roll = rand() % 3;
        if (roll == 0) {
            /* Trigger a single blink */
            micro_animator_wink(rand() & 1);
        }
        /* roll==1 and roll==2 are "do nothing" passes — subtlety */
    }

    /* ── Responsiveness: override animator timing ─── */

    /* Scale factor: responsiveness 1.0 → 0.67x faster, 0.0 → 2.0x slower */
    float time_mult = 1.0f / (0.3f + ctx.current.responsiveness * 0.7f);

    expression_id_t safe_id = (new_id < EXPRESSION_COUNT) ? new_id : 0;
    const expression_def_t *def = &EXPRESSION_DEFS[safe_id];

    /* Re-trigger per-component animations with adjusted timing */
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

void face_temperament_apply(face_state_t *state) {
    /* ── Apply transition pulse decay ───────────────── */

    if (ctx.pulse_active) {
        uint32_t elapsed = lv_tick_get() - ctx.pulse_start;
        float d = decay(elapsed, PULSE_DECAY_MS);

        if (d <= 0.0f) {
            ctx.pulse_active = false;
        } else {
            state->mouth.left_corner.dx  += ctx.pulse.mouth_corner_dx * d;
            state->mouth.right_corner.dx -= ctx.pulse.mouth_corner_dx * d;
            state->mouth.right_corner.dy += ctx.pulse.mouth_corner_dy * d;
            state->mouth.left_corner.dy  += ctx.pulse.mouth_corner_dy * d;
            state->mouth.openness        += ctx.pulse.mouth_openness * d;

            state->brow[0].arch.dy += ctx.pulse.brow_arch_dy * d;
            state->brow[1].arch.dy += ctx.pulse.brow_arch_dy * d;
            state->brow[0].thickness += ctx.pulse.brow_thickness * d;
            state->brow[1].thickness += ctx.pulse.brow_thickness * d;

            state->eye[0].top_lid_mid.dy += ctx.pulse.eye_lid_dy * d;
            state->eye[1].top_lid_mid.dy += ctx.pulse.eye_lid_dy * d;
            state->eye[0].pupil_scale    += ctx.pulse.eye_pupil_scale * d;
            state->eye[1].pupil_scale    += ctx.pulse.eye_pupil_scale * d;
            state->eye[0].shine_intensity += ctx.pulse.eye_shine * d;
            state->eye[1].shine_intensity += ctx.pulse.eye_shine * d;

            state->decor.blush += ctx.pulse.blush * d;
        }
    }
}
```

- [ ] **Step 2: Build**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py build
```

Expected: COMPILE SUCCESS

- [ ] **Step 3: Commit**

```bash
git add components/face_system/face_temperament.c
git commit -m "feat: add face_temperament.c — transition pulses + temperament (Part A)"
```

---

### Task 6: face_temperament.c — temperament coloring (Part D)

**Files:**
- Modify: `components/face_system/face_temperament.c` (append before the final `}` of `face_temperament_apply`)

- [ ] **Step 1: Add temperament coloring**

Insert at the end of `face_temperament_apply()`, after the pulse block and before the closing `}`:

```c
    /* ── Temperament coloring ────────────────────────── */

    /* Energy: scale expression parameters toward (high) or away from (low) neutral */
    float energy_bias = (ctx.current.energy - 0.5f) * 0.2f;  /* -0.1 .. +0.1 */
    state->mouth.openness += energy_bias * 0.15f;
    state->eye[0].pupil_scale += energy_bias * 0.1f;
    state->eye[1].pupil_scale += energy_bias * 0.1f;

    /* Expressiveness: amplify brow depth and mouth width */
    float expr_bias = (ctx.current.expressiveness - 0.5f) * 0.06f;  /* -0.03 .. +0.03 */
    state->brow[0].arch.dy += ctx.current.expressiveness > 0.5f ? -expr_bias * 0.8f : 0;
    state->brow[1].arch.dy += ctx.current.expressiveness > 0.5f ? -expr_bias * 0.8f : 0;
    state->mouth.left_corner.dx  += expr_bias;
    state->mouth.right_corner.dx -= expr_bias;
```

- [ ] **Step 2: Build**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py build
```

Expected: COMPILE SUCCESS

- [ ] **Step 3: Commit**

```bash
git add components/face_system/face_temperament.c
git commit -m "feat: add temperament coloring to face_temperament (Part D)"
```

---

### Task 7: Integration — face_api.h + app_behavior.c

**Files:**
- Modify: `components/face_system/face_api.h:17-23`
- Modify: `main/app_behavior.c:29-36`

- [ ] **Step 1: Wire into face_api.h**

Change `face_init()` to include new modules:

```c
// Add includes at top (after existing #include "face_prop.h"):
#include "face_vivid.h"
#include "face_temperament.h"

// In face_init(), after prop_animator_init():
static inline void face_init(void) {
    animator_init();
    renderer_init();
    micro_animator_init();
    prop_animator_init();
    face_vivid_init();
    face_temperament_init();
    renderer_set_sprite(sprite_registry_default());
}

// In face_render_frame(), after prop_animator_apply(&display_state):
static inline void face_render_frame(void) {
    face_state_t display_state = *animator_get_state();
    micro_animator_apply(&display_state);
    prop_animator_apply(&display_state);
    face_vivid_apply(&display_state);
    face_temperament_apply(&display_state);
    renderer_render_frame(&display_state);
}

// Add new API (after existing functions):
static inline void face_set_temperament(const temperament_profile_t *p) {
    face_temperament_set_profile(p);
}
```

- [ ] **Step 2: Wire into app_behavior.c**

In `transition_to()`, add temperament notification AFTER `face_set_expression()` so timing overrides apply correctly:

Change from:
```c
static void transition_to(behavior_state_t state, emotion_t emo) {
    current_state = state;
    state_start_ticks = xTaskGetTickCount();
    face_set_expression((expression_id_t)emo);
    if (emo == EMOTION_NEUTRAL) {
        face_prop_clear(300);
    }
}
```

To:
```c
static expression_id_t prev_expression = 0; /* EMOTION_NEUTRAL */

static void transition_to(behavior_state_t state, emotion_t emo) {
    current_state = state;
    state_start_ticks = xTaskGetTickCount();
    face_set_expression((expression_id_t)emo);
    face_temperament_notify_expression_change(prev_expression, (expression_id_t)emo);
    prev_expression = (expression_id_t)emo;
    if (emo == EMOTION_NEUTRAL) {
        face_prop_clear(300);
    }
}
```

Add include at top of app_behavior.c:
```c
#include "face_temperament.h"
```

- [ ] **Step 3: Build**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py build
```

Expected: COMPILE SUCCESS

- [ ] **Step 4: Commit**

```bash
git add components/face_system/face_api.h main/app_behavior.c
git commit -m "feat: integrate face_vivid + face_temperament into render pipeline and behavior"
```

---

### Task 8: Flash + verify

- [ ] **Step 1: Flash to device**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py -p /dev/tty.usbmodem* flash
```

- [ ] **Step 2: Visual verification checklist**

Shake the device and observe:
- HAPPY: music note prop should wiggle side-to-side with bounce
- After 4s → CONTENT: teacup should float up-down with subtle pulse
- Cold event → COLD: sunglasses should have slight wobble
- Transition HAPPY→CONTENT: should have a brief mouth/brow overshoot bounce
- Micro-motion: when idle in NEUTRAL, eyes should have barely-perceptible drift, pupils pulsing
- Different shakes should feel slightly different (jitter variation)

- [ ] **Step 3: Commit (if any fixes applied)**

```bash
git add -A
git commit -m "fix: visual tuning after on-device testing"
```
