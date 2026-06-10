# Expression System Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add two new expressions (DIZZY, UPSIDE_DOWN), remap all sensor-to-emotion interactions, and add three micro-animation behaviors (COLD shiver, BORED sigh, SLEEPY startle).

**Architecture:** Changes flow from data (harti_config.h enums → face_model.c presets) to rendering (sprite_vector.c eye/mouth types) to animation overlay (face_micro.c), with the sensor pipeline (app_sensors.h/c) and behavior state machine (app_behavior.c) completing the loop. Each task is independently buildable and verifiable via `idf.py build`.

**Tech Stack:** ESP32-S3 / ESP-IDF, FreeRTOS, LVGL (tick API only), C99, custom scanline renderer

---

## File Map

| File | Change |
|------|--------|
| `main/harti_config.h` | Add `EMOTION_DIZZY = 14`, `EMOTION_UPSIDE_DOWN = 15` to `emotion_t` |
| `components/face_system/face_model.c` | Add `EXPRESSION_DEFS[14]` and `[15]` |
| `components/face_system/sprites/sprite_vector.c` | Add `V_EYE_DIZZY`, `V_EYE_UPSIDE_DOWN`, `V_DIZZY` mouth; update classifiers; add DIZZY stars to `draw_decor_overlay` |
| `components/face_system/face_micro.h` | Declare `micro_animator_set_expression()` |
| `components/face_system/face_micro.c` | Implement COLD shiver, BORED sigh, SLEEPY startle; implement `micro_animator_set_expression()` |
| `components/face_system/face_api.h` | Wire `micro_animator_set_expression()` into `face_set_expression()` |
| `main/app_sensors.h` | Add `EVT_FLIP_RESTORE` to `sensor_event_t` |
| `main/app_sensors.c` | Add flip-restore detection in `process_imu()` |
| `main/app_behavior.c` | Add `STATE_DIZZY`/`STATE_UPSIDE_DOWN`; rewrite `on_event()`; add DIZZY timer and UPSIDE_DOWN timeout to `check_idle()` |

---

## Task 1: Add new emotion IDs to harti_config.h

**Files:**
- Modify: `main/harti_config.h`

### Context

`emotion_t` in `harti_config.h` maps 1:1 to `expression_id_t`. Currently has values 0–13. New emotions must be inserted before `EMOTION_COUNT`.

Current tail of the enum:
```c
EMOTION_HEART_EYES,   // 12
EMOTION_THINKING,     // 13
EMOTION_COUNT
```

- [ ] **Step 1: Add DIZZY and UPSIDE_DOWN to emotion_t**

Find the line with `EMOTION_THINKING` and add two lines after it:

```c
    EMOTION_HEART_EYES,
    EMOTION_THINKING,
    EMOTION_DIZZY,        // 14 — shake → star-pupil dizzy
    EMOTION_UPSIDE_DOWN,  // 15 — flip → teary pouty
    EMOTION_COUNT
```

- [ ] **Step 2: Verify it builds cleanly**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build 2>&1 | tail -5
```

Expected: `Build successful` (or at most warnings about unused IDs — no errors).

- [ ] **Step 3: Commit**

```bash
git add main/harti_config.h
git commit -m "feat: add EMOTION_DIZZY(14) and EMOTION_UPSIDE_DOWN(15) to emotion_t"
```

---

## Task 2: Add expression presets to face_model.c

**Files:**
- Modify: `components/face_system/face_model.c`

### Context

`EXPRESSION_DEFS[]` is a static array in `face_model.c`. Currently 13 entries (indices 0–12, matching `EMOTION_NEUTRAL` through `EMOTION_THINKING`). Append two new entries.

Key parameter decisions:
- **DIZZY**: `iris_detail = 1.0f` (unique flag; all existing expressions ≤ 0.8f), `pupil_scale = 0.0f`, `sparkle = 1.0f`, mouth `openness = 0.15f / cupid_depth = 0.18f` (O-ring mouth trigger), all components 80ms PATH_EASE_OUT.
- **UPSIDE_DOWN**: `iris_detail = 0.9f` (unique flag), `pupil_scale = 0.45f`, `iris_center.dy = 0.2f` (pupil sinks), inner brow `dy = -0.2f` / tail `dy = 0.3f` (委屈弧), mouth corners `dy = 0.22f` (strong droop), `tears = 0.8f`, 150ms PATH_EASE_IN_OUT.

- [ ] **Step 1: Locate the end of EXPRESSION_DEFS array**

It ends after `// [13] THINKING` entry, before the closing `};`. Find that closing brace and add the two new entries immediately before it.

- [ ] **Step 2: Append EMOTION_DIZZY preset**

Add this block (index 14) before the array's closing `};`:

```c
    // [14] DIZZY — shake trigger, star-pupil eyes, O-ring mouth
    {
        .name = "DIZZY",
        .target = {
            .face = {.roundness = 0.6f},
            .brow = {
                {.inner = {0, 0.1f}, .arch = {0, 0.1f}, .tail = {0, 0.1f}, .thickness = 1.0f, .taper = 0.6f},
                {.inner = {0, 0.1f}, .arch = {0, 0.1f}, .tail = {0, 0.1f}, .thickness = 1.0f, .taper = 0.6f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.0f}, .bot_lid_mid = {0, 0.0f},
                 .iris_center = {0, 0},
                 .position = {0, 0},
                 .pupil_scale = 0.0f, .shine_intensity = 0.0f,
                 .iris_detail = 1.0f, .eyelash = 0.0f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.0f}, .bot_lid_mid = {0, 0.0f},
                 .iris_center = {0, 0},
                 .position = {0, 0},
                 .pupil_scale = 0.0f, .shine_intensity = 0.0f,
                 .iris_detail = 1.0f, .eyelash = 0.0f},
            },
            .mouth = {
                .left_corner = {0, 0}, .right_corner = {0, 0},
                .upper_lip_mid = {0, 0}, .lower_lip_mid = {0, 0},
                .openness = 0.15f, .cupid_depth = 0.18f,
            },
            .decor = {.blush = 0, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 1.0f},
        },
        .timing = {
            [COMPONENT_FACE]       = {80, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {80, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {80, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {80, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {80, 0, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {80, 0, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {80, 0, PATH_EASE_OUT},
        },
    },
```

- [ ] **Step 3: Append EMOTION_UPSIDE_DOWN preset**

Add this block (index 15) immediately after the DIZZY entry, still before the array's `};`:

```c
    // [15] UPSIDE_DOWN — flip trigger, teary pouty eyes, strong corner droop
    {
        .name = "UPSIDE_DOWN",
        .target = {
            .face = {.roundness = 0.5f},
            .brow = {
                {.inner = {0, -0.2f}, .arch = {0, 0.05f}, .tail = {0, 0.3f}, .thickness = 1.0f, .taper = 0.6f},
                {.inner = {0, -0.2f}, .arch = {0, 0.05f}, .tail = {0, 0.3f}, .thickness = 1.0f, .taper = 0.6f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.0f}, .bot_lid_mid = {0, 0.0f},
                 .iris_center = {0, 0.2f},
                 .position = {0, 0},
                 .pupil_scale = 0.45f, .shine_intensity = 0.6f,
                 .iris_detail = 0.9f, .eyelash = 0.0f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.0f}, .bot_lid_mid = {0, 0.0f},
                 .iris_center = {0, 0.2f},
                 .position = {0, 0},
                 .pupil_scale = 0.45f, .shine_intensity = 0.6f,
                 .iris_detail = 0.9f, .eyelash = 0.0f},
            },
            .mouth = {
                .left_corner = {0.05f, 0.22f}, .right_corner = {-0.05f, 0.22f},
                .upper_lip_mid = {0, 0.1f}, .lower_lip_mid = {0, 0.15f},
                .openness = 0.05f, .cupid_depth = 0.1f,
            },
            .decor = {.blush = 0, .tears = 0.8f, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {150, 0,  PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {150, 0,  PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {150, 0,  PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {150, 30, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_RIGHT]  = {150, 30, PATH_EASE_IN_OUT},
            [COMPONENT_MOUTH]      = {200, 80, PATH_EASE_IN_OUT},
            [COMPONENT_DECOR]      = {200, 100, PATH_EASE_IN_OUT},
        },
    },
```

- [ ] **Step 4: Build**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build 2>&1 | tail -5
```

Expected: `Build successful`

- [ ] **Step 5: Commit**

```bash
git add components/face_system/face_model.c
git commit -m "feat: add EXPRESSION_DEFS[14] DIZZY and [15] UPSIDE_DOWN presets"
```

---

## Task 3: Add new eye/mouth types and stars to sprite_vector.c

**Files:**
- Modify: `components/face_system/sprites/sprite_vector.c`

### Context

This file contains the full scanline renderer. Four changes are needed:

1. Extend the `eye_type_t` enum with `V_EYE_DIZZY` and `V_EYE_UPSIDE_DOWN`
2. Extend the `mouth_type_t` enum with `V_DIZZY`
3. Update `classify_eye()` — add two branches (DIZZY before HEART, UPSIDE_DOWN before NORMAL)
4. Update `classify_mouth()` — add V_DIZZY branch before `V_TALK`
5. Add render cases in `draw_eye_vector()` for both new eye types
6. Add render case in `draw_mouth()` for `V_DIZZY`
7. Add DIZZY 3-star block in `draw_decor_overlay()` gated at `sparkle > 0.9f`

The file uses `draw_star_scan(y, cx, cy, size, color, buf, screen_w)` (7-arg form, already present from PROP_STAR_SMALL usage).

**Rendering notes:**
- `V_EYE_DIZZY`: white ring outline + 5-pointed star fill inside (angular sector math). No pupil.
- `V_EYE_UPSIDE_DOWN`: white ring + Bayer-dithered tear fill in lower half + pupil offset downward via `iris_center.dy`.
- `V_DIZZY` mouth: hollow circle ring (outer_r=8 inner_r=5, centered slightly below mouth_y).

`bayer_accept(x, y, density)` is a helper already present in `face_common.h` (used in `draw_decor_overlay` for tear rendering).

---

### Sub-step A: Extend enums

- [ ] **Step 1: Add V_EYE_DIZZY and V_EYE_UPSIDE_DOWN to eye_type_t enum**

Current enum:
```c
typedef enum {
    V_EYE_NORMAL,
    V_EYE_HAPPY,
    V_EYE_SURPRISED,
    V_EYE_SAD,
    V_EYE_HEART,
    V_EYE_ANGRY,
    V_EYE_BORED,
    V_EYE_SLEEPY,
} eye_type_t;
```

Replace with:
```c
typedef enum {
    V_EYE_NORMAL,
    V_EYE_HAPPY,
    V_EYE_SURPRISED,
    V_EYE_SAD,
    V_EYE_HEART,
    V_EYE_ANGRY,
    V_EYE_BORED,
    V_EYE_SLEEPY,
    V_EYE_DIZZY,        // ⑨ Star-pupil — DIZZY
    V_EYE_UPSIDE_DOWN,  // ⑩ Teary — UPSIDE_DOWN
} eye_type_t;
```

- [ ] **Step 2: Add V_DIZZY to mouth_type_t enum**

Current enum:
```c
typedef enum {
    V_SMILE, V_SURPRISE, V_SAD, V_ANGRY, V_SLEEPY,
    V_LAUGH, V_KISS, V_NEUTRAL, V_TALK, V_CONFUSED
} mouth_type_t;
```

Replace with:
```c
typedef enum {
    V_SMILE, V_SURPRISE, V_SAD, V_ANGRY, V_SLEEPY,
    V_LAUGH, V_KISS, V_NEUTRAL, V_TALK, V_CONFUSED,
    V_DIZZY   // ⑪ O-ring mouth — DIZZY
} mouth_type_t;
```

---

### Sub-step B: Update classifiers

- [ ] **Step 3: Add DIZZY branch to classify_eye (before HEART check)**

Current HEART check:
```c
    /* ⑤ HEART: pupil scaled to near-zero → heart pupil */
    if (ep->pupil_scale < 0.05f) return V_EYE_HEART;
```

Replace with:
```c
    /* ⑨ DIZZY: star pupils — iris_detail=1.0f is the flag (no existing expr > 0.8f) */
    if (ep->pupil_scale < 0.05f && ep->iris_detail > 0.95f) return V_EYE_DIZZY;

    /* ⑤ HEART: pupil scaled to near-zero → heart pupil */
    if (ep->pupil_scale < 0.05f) return V_EYE_HEART;
```

- [ ] **Step 4: Add UPSIDE_DOWN branch to classify_eye (before NORMAL fallthrough)**

Current NORMAL fallthrough:
```c
    /* ① NORMAL: default round eye */
    return V_EYE_NORMAL;
```

Replace with:
```c
    /* ⑩ UPSIDE_DOWN: iris_detail=0.9f is the flag (no existing expr > 0.8f except DIZZY=1.0f) */
    if (ep->iris_detail > 0.85f) return V_EYE_UPSIDE_DOWN;

    /* ① NORMAL: default round eye */
    return V_EYE_NORMAL;
```

- [ ] **Step 5: Add V_DIZZY branch to classify_mouth (before V_TALK)**

Current V_TALK check:
```c
    if (o > 0.12f) return V_TALK;
```

Replace with:
```c
    /* ⑪ DIZZY O-ring: narrow openness band + shallow cupid — must precede V_TALK */
    if (o > 0.12f && o < 0.22f && cupid < 0.22f) return V_DIZZY;
    if (o > 0.12f) return V_TALK;
```

---

### Sub-step C: Add render cases in draw_eye_vector

`draw_eye_vector` has a `switch (etype)` block. Locate the `default:` case and add both new cases before it.

- [ ] **Step 6: Add V_EYE_DIZZY render case**

Inside `draw_eye_vector`, add before `default:`:

```c
        case V_EYE_DIZZY: {
            /* White ring outline + 5-pointed star fill (angular sector math) */
            float eye_r_sq = eye_r * eye_r;
            for (int x = x_start; x <= x_end; x++) {
                float fx = (float)(x - eye_cx);
                float r_sq = fx * fx + fy * fy;
                if (r_sq >= eye_r_sq) continue;
                float r = sqrtf(r_sq);
                /* Ring border (outer ~2px) */
                if (eye_r - r < 2.2f) { buf[x] = pal[PAL_SCLERA]; continue; }
                /* Star fill: divide circle into 10 alternating sectors */
                float ang = atan2f(fy, fx);
                float sector = fmodf(ang * 5.0f / (2.0f * 3.14159265f) + 10.0f, 1.0f);
                float outer_r = eye_r * 0.55f;
                float inner_r = eye_r * 0.22f;
                float r_limit;
                if (sector < 0.5f) {
                    r_limit = outer_r * (1.0f - sector * 2.0f) + inner_r * sector * 2.0f;
                } else {
                    r_limit = inner_r + (outer_r - inner_r) * (sector - 0.5f) * 2.0f;
                }
                if (r_sq <= r_limit * r_limit) buf[x] = pal[PAL_SCLERA];
            }
            break;
        }
```

- [ ] **Step 7: Add V_EYE_UPSIDE_DOWN render case**

Inside `draw_eye_vector`, add before `default:` (after V_EYE_DIZZY):

```c
        case V_EYE_UPSIDE_DOWN: {
            /* White ring + Bayer-dithered tear fill in lower half + offset pupil */
            float eye_r_sq = eye_r * eye_r;
            float pupil_dx = ep->iris_center.dx * eye_r * 0.5f;
            float pupil_dy = ep->iris_center.dy * eye_r * 0.6f;
            float pupil_r  = eye_r * 0.28f;
            float pupil_r_sq = pupil_r * pupil_r;
            for (int x = x_start; x <= x_end; x++) {
                float fx = (float)(x - eye_cx);
                float r_sq = fx * fx + fy * fy;
                if (r_sq >= eye_r_sq) continue;
                float r = sqrtf(r_sq);
                /* Ring border */
                if (eye_r - r < 2.2f) { buf[x] = pal[PAL_SCLERA]; continue; }
                /* Lower-half tear dither (fy > 0 = below center) */
                if (fy > 0.0f) {
                    float tear_t = fy / eye_r;
                    if (bayer_accept(x, y, tear_t * 0.45f)) { buf[x] = pal[PAL_SCLERA]; continue; }
                }
                /* Pupil (shifted downward via iris_center.dy) */
                float p_dx = fx - pupil_dx, p_dy = fy - pupil_dy;
                if (p_dx * p_dx + p_dy * p_dy < pupil_r_sq) buf[x] = pal[PAL_SCLERA];
            }
            break;
        }
```

---

### Sub-step D: Add V_DIZZY mouth render case

`draw_mouth` has a `switch` on `mouth_type_t`. Locate the `default:` case and add V_DIZZY before it.

- [ ] **Step 8: Add V_DIZZY mouth render case**

`my` is the mouth center Y coordinate computed earlier in `draw_mouth`. Add before `default:`:

```c
        case V_DIZZY: {
            /* Small hollow circle ring — O-shaped surprised mouth */
            int cx = CENTER_X;
            int cy = my + 4;
            int outer_r = 8;
            int inner_r = 5;
            if (y < cy - outer_r || y > cy + outer_r) return;
            for (int x = cx - outer_r; x <= cx + outer_r; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                int dx = x - cx, dy2 = y - cy;
                int d2 = dx * dx + dy2 * dy2;
                if (d2 <= outer_r * outer_r && d2 >= inner_r * inner_r)
                    buf[x] = pal[PAL_MOUTH];
            }
            return;
        }
```

---

### Sub-step E: Add DIZZY stars to draw_decor_overlay

- [ ] **Step 9: Add DIZZY 3-star block in draw_decor_overlay**

In `draw_decor_overlay`, the existing sparkle block checks `dp->sparkle > 0.2f` and draws 6 small dot sparkles. Append a new block immediately after that section (still inside `draw_decor_overlay`):

```c
    /* ── DIZZY sparkle: 3 fixed ✦ stars around face (sparkle=1.0f flag) ── */
    if (dp->sparkle > 0.9f) {
        static const int dizzy_star_pos[3][2] = {
            {CENTER_X - 50, CENTER_Y - 45},
            {CENTER_X + 48, CENTER_Y - 40},
            {CENTER_X + 55, CENTER_Y + 10},
        };
        for (int i = 0; i < 3; i++) {
            draw_star_scan(y, (float)dizzy_star_pos[i][0],
                           (float)dizzy_star_pos[i][1],
                           16.0f, pal[PAL_SCLERA], buf, SCREEN_W);
        }
    }
```

- [ ] **Step 10: Build**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build 2>&1 | tail -5
```

Expected: `Build successful`

- [ ] **Step 11: Commit**

```bash
git add components/face_system/sprites/sprite_vector.c
git commit -m "feat: add V_EYE_DIZZY, V_EYE_UPSIDE_DOWN, V_DIZZY mouth, DIZZY stars to sprite_vector"
```

---

## Task 4: Add micro-animation state tracking to face_micro.h and face_micro.c

**Files:**
- Modify: `components/face_system/face_micro.h`
- Modify: `components/face_system/face_micro.c`

### Context

`face_micro.c` currently knows nothing about the current expression. We need:
1. A new `micro_animator_set_expression(expression_id_t id)` function declared in the header and implemented in the .c file.
2. Three expression-linked behaviors added to `micro_animator_apply()`:
   - **COLD shiver**: 8 Hz sine wave on `squash_x` and `stretch_y`
   - **BORED sigh**: periodic eye-close sequence with random interval
   - **SLEEPY startle**: periodic brief eye-open sequence with random interval

`face_micro.c` already has `lv_tick_get()`, `rand_ms()`, `randf_range()`, `lerpf()`, `clampf()`, and `ease_out()` helpers. The `micro_animator_apply(face_state_t *s)` function receives the full display state to modify in-place. The expression IDs come from `harti_config.h` which is not currently included — it must be added.

---

### Sub-step A: face_micro.h

- [ ] **Step 1: Add micro_animator_set_expression declaration to face_micro.h**

After the existing `void micro_animator_wink(int eye);` line, add:

```c
// Notify micro-animator of expression change. Resets expression-linked state machines.
void micro_animator_set_expression(expression_id_t id);
```

---

### Sub-step B: face_micro.c — includes and state variables

- [ ] **Step 2: Add harti_config.h include at top of face_micro.c**

After `#include "face_micro.h"` add:

```c
#include "../../main/harti_config.h"
```

- [ ] **Step 3: Add expression-tracking state variables**

After the existing `static bool enabled = true;` line in the State section, add:

```c
/* ── Expression-linked micro-animations ───────────────────── */
static expression_id_t current_expr_id = 0;

/* BORED sigh state machine */
typedef enum { SIGH_IDLE, SIGH_CLOSING, SIGH_HOLD, SIGH_OPENING } sigh_phase_t;
static sigh_phase_t sigh_phase = SIGH_IDLE;
static uint32_t     sigh_start = 0;
static uint32_t     next_sigh_at = 0;

/* SLEEPY startle state machine */
typedef enum { STARTLE_IDLE, STARTLE_OPEN, STARTLE_HOLD, STARTLE_CLOSE } startle_phase_t;
static startle_phase_t startle_phase = STARTLE_IDLE;
static uint32_t        startle_start = 0;
static uint32_t        next_startle_at = 0;
```

---

### Sub-step C: face_micro.c — micro_animator_set_expression

- [ ] **Step 4: Implement micro_animator_set_expression**

Add this function after `micro_animator_init()`:

```c
void micro_animator_set_expression(expression_id_t id) {
    if (id == current_expr_id) return;
    current_expr_id = id;
    /* Reset expression-linked state machines on expression change */
    sigh_phase = SIGH_IDLE;
    next_sigh_at = lv_tick_get() + rand_ms(3000, 8000);
    startle_phase = STARTLE_IDLE;
    next_startle_at = lv_tick_get() + rand_ms(5000, 12000);
}
```

---

### Sub-step D: face_micro.c — three micro-animations in micro_animator_apply

All three blocks are added inside `micro_animator_apply()`, after the existing tilt-tracking code and before the function's closing brace.

- [ ] **Step 5: Add COLD shiver**

```c
    /* ── 3-A COLD shiver: 8 Hz squash/stretch oscillation ─── */
    if (current_expr_id == EMOTION_COLD) {
        float shiver = sinf((float)now * 2.0f * 3.14159265f * 8.0f / 1000.0f) * 0.025f;
        s->face.squash_x   += shiver;
        s->face.stretch_y  -= shiver * 0.6f;
    }
```

- [ ] **Step 6: Add BORED sigh state machine**

```c
    /* ── 3-B BORED sigh: periodic eyelid droop ────────────── */
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
```

- [ ] **Step 7: Add SLEEPY startle state machine**

```c
    /* ── 3-C SLEEPY startle: brief eye-open then re-close ─── */
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
                s->eye[0].top_lid_mid.dy  -= ease_out(t) * 0.55f;
                s->eye[1].top_lid_mid.dy  -= ease_out(t) * 0.55f;
                s->eye[0].pupil_scale     += ease_out(t) * 0.3f;
                s->eye[1].pupil_scale     += ease_out(t) * 0.3f;
                if (dt >= 120) { startle_phase = STARTLE_HOLD; startle_start = now; }
                break;
            case STARTLE_HOLD:
                s->eye[0].top_lid_mid.dy  -= 0.55f;
                s->eye[1].top_lid_mid.dy  -= 0.55f;
                s->eye[0].pupil_scale     += 0.3f;
                s->eye[1].pupil_scale     += 0.3f;
                if (dt >= 500) { startle_phase = STARTLE_CLOSE; startle_start = now; }
                break;
            case STARTLE_CLOSE:
                t = clampf((float)dt / 300.0f, 0.0f, 1.0f);
                s->eye[0].top_lid_mid.dy  -= (1.0f - t) * 0.55f;
                s->eye[1].top_lid_mid.dy  -= (1.0f - t) * 0.55f;
                s->eye[0].pupil_scale     += (1.0f - t) * 0.3f;
                s->eye[1].pupil_scale     += (1.0f - t) * 0.3f;
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
```

- [ ] **Step 8: Build**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build 2>&1 | tail -5
```

Expected: `Build successful`

- [ ] **Step 9: Commit**

```bash
git add components/face_system/face_micro.h components/face_system/face_micro.c
git commit -m "feat: add micro_animator_set_expression + COLD shiver, BORED sigh, SLEEPY startle"
```

---

## Task 5: Wire micro_animator_set_expression into face_api.h

**Files:**
- Modify: `components/face_system/face_api.h`

### Context

`face_set_expression()` currently only calls `animator_set_expression(id)`. It must also call `micro_animator_set_expression(id)` so the micro-animation state machines reset and activate correctly on every expression change.

- [ ] **Step 1: Update face_set_expression in face_api.h**

Current function:
```c
static inline void face_set_expression(expression_id_t id) {
    animator_set_expression(id);
}
```

Replace with:
```c
static inline void face_set_expression(expression_id_t id) {
    animator_set_expression(id);
    micro_animator_set_expression(id);
}
```

- [ ] **Step 2: Build**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build 2>&1 | tail -5
```

Expected: `Build successful`

- [ ] **Step 3: Commit**

```bash
git add components/face_system/face_api.h
git commit -m "feat: wire micro_animator_set_expression into face_set_expression"
```

---

## Task 6: Add EVT_FLIP_RESTORE sensor event

**Files:**
- Modify: `main/app_sensors.h`
- Modify: `main/app_sensors.c`

### Context

When the device is flipped Z-down, `flip_armed` is set to `false` and `EVT_FLIP` fires. To detect recovery, we need a separate counter that fires `EVT_FLIP_RESTORE` when `az > +0.7f` is sustained for 30 frames (300ms at 100Hz) while `!flip_armed` (meaning a flip was previously detected).

---

### Sub-step A: app_sensors.h

- [ ] **Step 1: Add EVT_FLIP_RESTORE to sensor_event_t**

Current enum ends at:
```c
    EVT_TILT,           // 倾斜检测 (value: 0前 1左 2右 3后)
} sensor_event_t;
```

Replace with:
```c
    EVT_TILT,           // 倾斜检测 (value: 0前 1左 2右 3后)
    EVT_FLIP_RESTORE,   // Z轴从负回正（设备翻回正面朝上）
} sensor_event_t;
```

---

### Sub-step B: app_sensors.c

- [ ] **Step 2: Add flip_restore_frames counter near other flip state variables**

After this line in the static variable block:
```c
static bool  flip_armed = true;
```

Add:
```c
static int   flip_restore_frames = 0;
```

- [ ] **Step 3: Add flip-restore detection in process_imu()**

After the existing flip detection block (which ends at `flip_z_down_frames = 0;`), add:

```c
    // ── Flip restore detection (Z-axis back to positive, sustained > 300ms) ──
    if (!flip_armed && az > 0.7f) {
        flip_restore_frames++;
        if (flip_restore_frames >= 30) {
            sensor_event_msg_t msg = { .type = EVT_FLIP_RESTORE, .value = 0 };
            xQueueSend(event_queue, &msg, 0);
            flip_armed = true;
            flip_restore_frames = 0;
            ESP_LOGI(TAG, "FLIP_RESTORE detected");
        }
    } else if (az <= 0.7f) {
        flip_restore_frames = 0;
    }
```

The existing flip detection block for reference (what comes just before this insertion):
```c
    } else {
        if (flip_z_down_frames > 0 && flip_z_down_frames < FLIP_MIN_FRAMES) {
            flip_armed = true;
        }
        flip_z_down_frames = 0;
    }
    // ← INSERT NEW BLOCK HERE
```

- [ ] **Step 4: Build**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build 2>&1 | tail -5
```

Expected: `Build successful`

- [ ] **Step 5: Commit**

```bash
git add main/app_sensors.h main/app_sensors.c
git commit -m "feat: add EVT_FLIP_RESTORE sensor event with 300ms detection hysteresis"
```

---

## Task 7: Rewrite behavior state machine in app_behavior.c

**Files:**
- Modify: `main/app_behavior.c`

### Context

This is the largest change. Current state machine has `STATE_IDLE` through `STATE_SLEEPY`. Changes:

1. **Add states**: `STATE_DIZZY`, `STATE_UPSIDE_DOWN`
2. **Add state variables**: `dizzy_start_ticks` (for 2s auto-recovery timer) and `upside_down_start_ticks` (for 30s timeout)
3. **Rewrite `on_event()`**:
   - `EVT_SHAKE` → `STATE_DIZZY` (was: direction-based SURPRISED/HAPPY/EXCITED split with bug)
   - `EVT_TAP value==1` → `STATE_HAPPY` (was: no reaction)
   - `EVT_TAP value==2` → `STATE_SURPRISED` (was: SAD)
   - `EVT_TAP value>=3` → `STATE_SAD` (was: only value>=2 triggered SAD)
   - `EVT_FLIP` → `STATE_UPSIDE_DOWN` (was: SURPRISED)
   - `EVT_FLIP_RESTORE` → `NEUTRAL` if currently in `STATE_UPSIDE_DOWN`
   - `EVT_TWIST` → `STATE_EXCITED` (was: SURPRISED — bug fix)
   - `EVT_TILT` → keep existing CONFUSED/SURPRISED split (already correct)
   - `EVT_WARM_UP` → add `face_prop_show(PROP_HEART, ...)` (was: only WARM)
   - `EVT_COLD_DOWN` → keep (COLD micro-animation handled by face_micro now)
4. **Update `check_idle()`**: add DIZZY 2s auto-recovery and UPSIDE_DOWN 30s timeout
5. **Fix transition_to()**: update the `prev_expression`-linked prop cleanup switch to handle new expressions

---

### Sub-step A: States and variables

- [ ] **Step 1: Add new behavior states to behavior_state_t enum**

Current enum:
```c
typedef enum {
    STATE_IDLE,
    STATE_HAPPY,
    STATE_CONTENT,
    STATE_SURPRISED,
    STATE_THINKING,
    STATE_CONFUSED,
    STATE_SAD,
    STATE_WARM,
    STATE_COLD,
    STATE_BORED,
    STATE_SLEEPY,
} behavior_state_t;
```

Replace with:
```c
typedef enum {
    STATE_IDLE,
    STATE_HAPPY,
    STATE_CONTENT,
    STATE_SURPRISED,
    STATE_THINKING,
    STATE_CONFUSED,
    STATE_SAD,
    STATE_WARM,
    STATE_COLD,
    STATE_BORED,
    STATE_SLEEPY,
    STATE_DIZZY,
    STATE_UPSIDE_DOWN,
} behavior_state_t;
```

- [ ] **Step 2: Add timer state variables**

After `static expression_id_t prev_expression = 0;`, add:

```c
static TickType_t dizzy_start_ticks = 0;       // for 2s DIZZY auto-recovery
static TickType_t upside_down_start_ticks = 0; // for 30s UPSIDE_DOWN timeout
```

---

### Sub-step B: Rewrite on_event()

- [ ] **Step 3: Rewrite the EVT_SHAKE case**

Current:
```c
    case EVT_SHAKE: {
        face_prop_show(PROP_STAR_SMALL, 11.0f, 0.55f, 180);
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        if (msg->value >= 1.5f) {
            transition_to(STATE_SURPRISED, EMOTION_SURPRISED);
        } else if (msg->value >= 0.5f) {
            transition_to(STATE_HAPPY, EMOTION_HAPPY);
        } else {
            transition_to(STATE_SURPRISED, EMOTION_EXCITED);
        }
        break;
    }
```

Replace with:
```c
    case EVT_SHAKE: {
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); }
        if (current_state == STATE_DIZZY) {
            /* Re-shake while dizzy: reset the recovery timer */
            dizzy_start_ticks = xTaskGetTickCount();
            ESP_LOGI(TAG, "SHAKE re-triggered, resetting DIZZY timer");
        } else {
            transition_to(STATE_DIZZY, EMOTION_DIZZY);
            dizzy_start_ticks = xTaskGetTickCount();
            ESP_LOGI(TAG, "SHAKE → DIZZY");
        }
        break;
    }
```

- [ ] **Step 4: Rewrite the EVT_TAP case**

Current:
```c
    case EVT_TAP:
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        if (msg->value >= 2.0f) {
            transition_to(STATE_SAD, EMOTION_SAD);
        }
        break;
```

Replace with:
```c
    case EVT_TAP:
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); }
        if (msg->value >= 3.0f) {
            transition_to(STATE_SAD, EMOTION_SAD);
            ESP_LOGI(TAG, "TAP x3 → SAD");
        } else if (msg->value >= 2.0f) {
            transition_to(STATE_SURPRISED, EMOTION_SURPRISED);
            ESP_LOGI(TAG, "TAP x2 → SURPRISED");
        } else {
            transition_to(STATE_HAPPY, EMOTION_HAPPY);
            ESP_LOGI(TAG, "TAP x1 → HAPPY");
        }
        break;
```

- [ ] **Step 5: Rewrite EVT_FLIP and add EVT_FLIP_RESTORE**

Current EVT_FLIP:
```c
    case EVT_FLIP:
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        transition_to(STATE_SURPRISED, EMOTION_SURPRISED);
        break;
```

Replace with:
```c
    case EVT_FLIP:
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); }
        transition_to(STATE_UPSIDE_DOWN, EMOTION_UPSIDE_DOWN);
        upside_down_start_ticks = xTaskGetTickCount();
        ESP_LOGI(TAG, "FLIP → UPSIDE_DOWN");
        break;

    case EVT_FLIP_RESTORE:
        if (current_state == STATE_UPSIDE_DOWN) {
            transition_to(STATE_IDLE, EMOTION_NEUTRAL);
            ESP_LOGI(TAG, "FLIP_RESTORE → NEUTRAL");
        }
        break;
```

- [ ] **Step 6: Fix EVT_TWIST (SURPRISED → EXCITED)**

Current:
```c
    case EVT_TWIST:
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        transition_to(STATE_SURPRISED, EMOTION_SURPRISED);
        ESP_LOGI(TAG, "TWIST → SURPRISED");
        break;
```

Replace with:
```c
    case EVT_TWIST:
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); }
        transition_to(STATE_SURPRISED, EMOTION_EXCITED);
        ESP_LOGI(TAG, "TWIST → EXCITED");
        break;
```

- [ ] **Step 7: Update EVT_WARM_UP to also show PROP_HEART**

Current:
```c
    case EVT_WARM_UP:
        face_prop_show(PROP_TEACUP, 9.8f, 0.6f, 250);
        if (was_idle) { transition_to(STATE_WARM, EMOTION_WARM); break; }
        transition_to(STATE_WARM, EMOTION_WARM);
        break;
```

Replace with:
```c
    case EVT_WARM_UP:
        face_prop_show(PROP_TEACUP, 9.8f, 0.6f, 250);
        face_prop_show(PROP_HEART, 11.5f, 0.5f, 400);
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); }
        transition_to(STATE_WARM, EMOTION_WARM);
        ESP_LOGI(TAG, "WARM_UP → WARM");
        break;
```

---

### Sub-step C: Update check_idle() for DIZZY and UPSIDE_DOWN timers

- [ ] **Step 8: Add DIZZY and UPSIDE_DOWN timeout handling to check_idle()**

At the top of `check_idle()`, after the `TickType_t` variable declarations, add two new timeout blocks. They must come before the existing SURPRISED/HAPPY/etc. timeouts:

```c
    /* DIZZY: auto-recover to NEUTRAL after 2000ms */
    if (current_state == STATE_DIZZY) {
        int dizzy_ms = (int)((now - dizzy_start_ticks) * portTICK_PERIOD_MS);
        if (dizzy_ms >= 2000) {
            transition_to(STATE_IDLE, EMOTION_NEUTRAL);
            ESP_LOGI(TAG, "DIZZY timeout → NEUTRAL (%dms)", dizzy_ms);
        }
        return;
    }

    /* UPSIDE_DOWN: safety timeout after 30s in case EVT_FLIP_RESTORE is missed */
    if (current_state == STATE_UPSIDE_DOWN) {
        int ud_s = (int)((now - upside_down_start_ticks) * portTICK_PERIOD_MS / 1000);
        if (ud_s >= 30) {
            transition_to(STATE_IDLE, EMOTION_NEUTRAL);
            ESP_LOGI(TAG, "UPSIDE_DOWN 30s timeout → NEUTRAL");
        }
        return;
    }
```

---

### Sub-step D: Build and commit

- [ ] **Step 9: Build**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build 2>&1 | tail -5
```

Expected: `Build successful`

- [ ] **Step 10: Commit**

```bash
git add main/app_behavior.c
git commit -m "feat: rewrite behavior — DIZZY/UPSIDE_DOWN states, tap 3-tier, twist→EXCITED, warm+heart"
```

---

## Self-Review

### Spec coverage

| Spec requirement | Covered by |
|-----------------|------------|
| EMOTION_DIZZY = 14, EMOTION_UPSIDE_DOWN = 15 | Task 1 |
| EXPRESSION_DEFS[14] DIZZY params (iris_detail=1.0f, sparkle=1.0f, etc.) | Task 2 |
| EXPRESSION_DEFS[15] UPSIDE_DOWN params (iris_detail=0.9f, tears=0.8f, brow.inner.dy=-0.2f, etc.) | Task 2 |
| V_EYE_DIZZY star-pupil render | Task 3 |
| V_EYE_UPSIDE_DOWN teary render | Task 3 |
| V_DIZZY O-ring mouth | Task 3 |
| classify_eye DIZZY before HEART (pupil_scale<0.05 && iris_detail>0.95) | Task 3 |
| classify_eye UPSIDE_DOWN before NORMAL (iris_detail>0.85) | Task 3 |
| DIZZY 3 fixed stars around face via sparkle>0.9f | Task 3 |
| micro_animator_set_expression() API | Task 4 |
| COLD shiver 8Hz squash_x/stretch_y | Task 4 |
| BORED sigh 400ms→200ms→600ms state machine | Task 4 |
| SLEEPY startle 120ms→500ms→300ms state machine | Task 4 |
| face_set_expression calls micro_animator_set_expression | Task 5 |
| EVT_FLIP_RESTORE in sensor_event_t | Task 6 |
| flip-restore detection (az>0.7 sustained 30 frames, !flip_armed) | Task 6 |
| tap 1→HAPPY, 2→SURPRISED, 3→SAD | Task 7 |
| shake → DIZZY (re-shake resets timer) | Task 7 |
| flip → UPSIDE_DOWN | Task 7 |
| EVT_FLIP_RESTORE → NEUTRAL if STATE_UPSIDE_DOWN | Task 7 |
| twist → EXCITED (bug fix) | Task 7 |
| warm → WARM + PROP_HEART | Task 7 |
| DIZZY 2s auto-recovery timer | Task 7 |
| UPSIDE_DOWN 30s safety timeout | Task 7 |
| STATE_DIZZY, STATE_UPSIDE_DOWN in behavior | Task 7 |

All spec requirements accounted for.

### Constraint check

- **Strict black/white**: All new rendering uses `pal[PAL_SCLERA]`, `pal[PAL_MOUTH]`, `pal[PAL_BG]` — palette values; no hardcoded RGB. ✓
- **iris_detail = 1.0f exclusive to DIZZY**: checked in classify_eye first. ✓
- **iris_detail = 0.9f exclusive to UPSIDE_DOWN**: next highest in any expression is SURPRISED at 0.8f. ✓
- **micro-animations are pure offset additives**: all state machines do `+= offset` on display_state; never call `transition_to` or write `current_state`. ✓
- **existing sparkle dots unaffected**: gated at `> 0.2f`, DIZZY stars at `> 0.9f` — no conflict. ✓
