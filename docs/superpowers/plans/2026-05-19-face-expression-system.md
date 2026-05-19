# Face Expression System — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the monolithic `expressive_eyes` expression system with a component-based, LVGL-animated, sprite-swappable 4-layer face system.

**Architecture:** Four layers — Face Model (pure data, keypoint-driven params for 7 components), Face Animator (LVGL `lv_anim` per component, independent timing/curves), Face Renderer (scanline dispatch to sprite draw functions, z-order compositing), Face API (high-level `face_set_expression()` + low-level `face_set_component()`).

**Tech Stack:** C (ESP-IDF v6.0.1), LVGL v8.x (animation only, no display driver), FreeRTOS, ESP32-S3, GC9A01 240x240 SPI LCD

---

## File Structure

```
components/face_system/
├── face_model.h            — types, keypoints, face_state_t, expression_def_t, sprite_set_t
├── face_model.c            — NEUTRAL default, 13 expression presets, classic sprite definition
├── face_animator.h         — animator public API (init, set_expression, set_component)
├── face_animator.c         — LVGL lv_anim creation, diff/lerp, interrupt handling
├── face_renderer.h         — renderer public API (render_frame, set_sprite, post_line_cb)
├── face_renderer.c         — scanline loop, z-order dispatch, background fill
├── face_api.h              — unified public API (includes animator + renderer)
├── sprite_classic.h        — classic sprite renderer declarations
├── sprite_classic.c        — draw functions migrated from expressive_eyes + new brow/mouth
├── face_palette.h          — color palette definitions (migrated from expressive_eyes)
└── CMakeLists.txt
```

**Modified files:**
- `CMakeLists.txt` (root) — add LVGL managed dependency
- `main/CMakeLists.txt` — add `face_system` to REQUIRES
- `main/main.c` — LVGL init + lv_tick timer
- `main/app_display.c` — switch to face_api, port micro-animations
- `main/app_display.h` — keep emotion_t enum, add compatibility

---

### Task 1: Create directory + CMakeLists + LVGL dependency

**Files:**
- Create: `components/face_system/CMakeLists.txt`
- Modify: `main/CMakeLists.txt`

- [ ] **Step 1: Add LVGL managed dependency**

Create or update `main/idf_component.yml`:
```yaml
dependencies:
  lvgl/lvgl: "^8.3"
```

Run: `idf.py reconfigure`
Expected: LVGL component downloaded to `managed_components/lvgl__lvgl/`

- [ ] **Step 2: Create face_system component CMakeLists**

Write `components/face_system/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "face_model.c" "face_animator.c" "face_renderer.c" "sprite_classic.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl gc9a01
)
```

- [ ] **Step 3: Update main/CMakeLists.txt**

Read `main/CMakeLists.txt`, change REQUIRES line from:
```cmake
    REQUIRES driver expressive_eyes gc9a01 harti_imu harti_temp
```
To:
```cmake
    REQUIRES driver expressive_eyes gc9a01 harti_imu harti_temp face_system
```

- [ ] **Step 4: Commit**

```bash
git add components/face_system/CMakeLists.txt main/CMakeLists.txt main/idf_component.yml
git commit -m "chore: add face_system component skeleton and LVGL dependency
Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 2: face_model.h — all data types

**Files:**
- Create: `components/face_system/face_model.h`
- Create: `components/face_system/face_palette.h`

- [ ] **Step 1: Write face_palette.h (color definitions, migrated from expressive_eyes)**

Write `components/face_system/face_palette.h`:
```c
#ifndef FACE_PALETTE_H
#define FACE_PALETTE_H

#include <stdint.h>

#define RGB565(r,g,b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

typedef enum {
    PAL_BG = 0,
    PAL_BG_EDGE,
    PAL_SCLERA,
    PAL_IRIS,
    PAL_PUPIL,
    PAL_BLUSH,
    PAL_TEAR,
    PAL_SHINE,
    PAL_STAR,
    PAL_SKIN,     // new: face skin tone
    PAL_BROW,     // new: eyebrow color
    PAL_MOUTH,    // new: mouth/lip color
    PAL_COUNT
} palette_index_t;

// Classic white theme
static const uint16_t PALETTE_WHITE[PAL_COUNT] = {
    [PAL_BG]      = RGB565(255, 255, 255),
    [PAL_BG_EDGE] = RGB565(235, 235, 235),
    [PAL_SCLERA]  = RGB565(255, 255, 255),
    [PAL_IRIS]    = RGB565(40, 40, 40),
    [PAL_PUPIL]   = RGB565(0, 0, 0),
    [PAL_BLUSH]   = RGB565(255, 160, 160),
    [PAL_TEAR]    = RGB565(180, 210, 255),
    [PAL_SHINE]   = RGB565(255, 255, 255),
    [PAL_STAR]    = RGB565(255, 220, 0),
    [PAL_SKIN]    = RGB565(255, 240, 225),
    [PAL_BROW]    = RGB565(60, 50, 45),
    [PAL_MOUTH]   = RGB565(200, 120, 120),
};

// Classic black theme
static const uint16_t PALETTE_BLACK[PAL_COUNT] = {
    [PAL_BG]      = RGB565(0, 0, 0),
    [PAL_BG_EDGE] = RGB565(0, 0, 0),
    [PAL_SCLERA]  = RGB565(58, 58, 62),
    [PAL_IRIS]    = RGB565(82, 80, 85),
    [PAL_PUPIL]   = RGB565(4, 4, 6),
    [PAL_BLUSH]   = RGB565(30, 18, 18),
    [PAL_TEAR]    = RGB565(30, 40, 55),
    [PAL_SHINE]   = RGB565(255, 255, 255),
    [PAL_STAR]    = RGB565(255, 255, 100),
    [PAL_SKIN]    = RGB565(20, 18, 20),
    [PAL_BROW]    = RGB565(50, 48, 50),
    [PAL_MOUTH]   = RGB565(40, 30, 30),
};

#endif
```

- [ ] **Step 2: Write face_model.h**

Write `components/face_system/face_model.h`:
```c
#ifndef FACE_MODEL_H
#define FACE_MODEL_H

#include <stdint.h>
#include <stdbool.h>
#include "face_palette.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Keypoint (normalized -1.0..1.0) ─────────────────────── */

typedef struct {
    float dx;  // horizontal offset from component origin
    float dy;  // vertical offset from component origin
} face_kpt_t;

/* ── Per-component parameter structs ─────────────────────── */

typedef struct {
    float roundness;   // 0 = sharp, 1 = very round
} face_params_t;

typedef struct {
    face_kpt_t inner;  // inner brow point (near nose)
    face_kpt_t arch;   // peak of the arch
    face_kpt_t tail;   // outer tail point
    float thickness;   // 0.5 thin .. 1.5 thick
} brow_params_t;

typedef struct {
    face_kpt_t inner_corner;
    face_kpt_t outer_corner;
    face_kpt_t top_lid_mid;
    face_kpt_t bot_lid_mid;
    face_kpt_t iris_center;
    float pupil_scale;       // 0.0 .. 1.0
    float shine_intensity;   // 0.0 .. 1.0
} eye_params_t;

typedef struct {
    face_kpt_t left_corner;
    face_kpt_t right_corner;
    face_kpt_t upper_lip_mid;
    face_kpt_t lower_lip_mid;
    float openness;  // 0 = closed, 1 = fully open
} mouth_params_t;

typedef struct {
    float blush;     // 0..1
    float tears;     // 0..1
    float stars;     // 0..1
    float sweat;     // 0..1
    float sparkle;   // 0..1
} decor_params_t;

/* ── Component enum ──────────────────────────────────────── */

typedef enum {
    COMPONENT_FACE = 0,
    COMPONENT_BROW_LEFT,
    COMPONENT_BROW_RIGHT,
    COMPONENT_EYE_LEFT,
    COMPONENT_EYE_RIGHT,
    COMPONENT_MOUTH,
    COMPONENT_DECOR,
    COMPONENT_COUNT
} face_component_t;

/* ── Full face state ─────────────────────────────────────── */

typedef struct {
    face_params_t face;
    brow_params_t brow[2];   // [0] = left, [1] = right
    eye_params_t  eye[2];    // [0] = left, [1] = right
    mouth_params_t mouth;
    decor_params_t decor;
} face_state_t;

/* ── Expression preset ───────────────────────────────────── */

typedef uint8_t expression_id_t;

typedef enum {
    PATH_LINEAR = 0,
    PATH_EASE_OUT,
    PATH_EASE_IN,
    PATH_EASE_IN_OUT,
    PATH_OVERSPEED,
} anim_path_type_t;

typedef struct {
    uint32_t duration_ms;
    uint32_t delay_ms;
    anim_path_type_t path_type;
} component_timing_t;

typedef struct {
    const char *name;
    face_state_t target;
    component_timing_t timing[COMPONENT_COUNT];
} expression_def_t;

/* ── Sprite set ──────────────────────────────────────────── */

typedef uint8_t sprite_id_t;
struct sprite_set_s;  // forward decl

typedef void (*sprite_draw_func_t)(int y, const face_state_t *st,
                                   const struct sprite_set_s *sp, uint16_t *buf);

typedef struct sprite_set_s {
    const char *name;

    // geometry (screen pixels)
    float eye_radius;       // eye size (default 36)
    float eye_half_spacing; // half-distance between eyes (default 26)
    float mouth_y_center;   // mouth vertical center from screen center (default 50)
    float brow_y_offset;    // brow vertical offset from eye center (default -38)
    float blush_y_offset;   // blush vertical offset from eye center (default 35)

    // draw functions (one per z-order pass, see render order)
    sprite_draw_func_t draw_face;         // pass 1: background + skin
    sprite_draw_func_t draw_blush;        // pass 2: blush under eyes
    sprite_draw_func_t draw_mouth;        // pass 3: mouth
    sprite_draw_func_t draw_eye_left;     // pass 4
    sprite_draw_func_t draw_eye_right;    // pass 5
    sprite_draw_func_t draw_brow_left;    // pass 6
    sprite_draw_func_t draw_brow_right;   // pass 7
    sprite_draw_func_t draw_decor_overlay; // pass 8: tears/stars/sweat/sparkle

    // palette
    const uint16_t *pal;
} sprite_set_t;

/* ── Extern declarations for built-in data ───────────────── */

extern const face_state_t FACE_STATE_NEUTRAL;

extern const expression_def_t EXPRESSION_DEFS[];
extern const uint8_t EXPRESSION_COUNT;

extern const sprite_set_t SPRITE_CLASSIC;

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 3: Commit**

```bash
git add components/face_system/face_model.h components/face_system/face_palette.h
git commit -m "feat: add face_model.h - component types, keypoints, state, expression, sprite
Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 3: face_model.c — NEUTRAL state + 13 expression presets

**Files:**
- Create: `components/face_system/face_model.c`

- [ ] **Step 1: Define NEUTRAL face_state and lerp helpers**

Write `components/face_system/face_model.c`:
```c
#include "face_model.h"
#include <string.h>

/* ── Neutral component states ────────────────────────────── */

static const brow_params_t BROW_NEUTRAL = {
    .inner = {0, 0}, .arch = {0, 0}, .tail = {0, 0},
    .thickness = 1.0f,
};

static const eye_params_t EYE_NEUTRAL = {
    .inner_corner = {0, 0}, .outer_corner = {0, 0},
    .top_lid_mid = {0, 0}, .bot_lid_mid = {0, 0},
    .iris_center = {0, 0},
    .pupil_scale = 0.6f, .shine_intensity = 0.88f,
};

static const mouth_params_t MOUTH_NEUTRAL = {
    .left_corner = {0, 0}, .right_corner = {0, 0},
    .upper_lip_mid = {0, 0}, .lower_lip_mid = {0, 0},
    .openness = 0.0f,
};

static const face_params_t FACE_NEUTRAL = {
    .roundness = 0.5f,
};

static const decor_params_t DECOR_NEUTRAL = {0};

const face_state_t FACE_STATE_NEUTRAL = {
    .face = {.roundness = 0.5f},
    .brow = {BROW_NEUTRAL, BROW_NEUTRAL},
    .eye  = {EYE_NEUTRAL, EYE_NEUTRAL},
    .mouth = MOUTH_NEUTRAL,
    .decor = DECOR_NEUTRAL,
};

/* ── Expression presets (13 emotions) ────────────────────── */

const expression_def_t EXPRESSION_DEFS[] = {
    // [0] NEUTRAL
    {
        .name = "NEUTRAL",
        .target = FACE_STATE_NEUTRAL,
        .timing = {
            [COMPONENT_FACE]       = {300, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {300, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {300, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {300, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {300, 0, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {300, 50, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {300, 100, PATH_EASE_OUT},
        },
    },
    // [1] HAPPY
    {
        .name = "HAPPY",
        .target = {
            .face = {.roundness = 0.6f},
            .brow = {
                {.inner = {0, -0.1f}, .arch = {0, -0.45f}, .tail = {0.1f, -0.3f}, .thickness = 1.0f},
                {.inner = {0, -0.1f}, .arch = {0, -0.45f}, .tail = {-0.1f, -0.3f}, .thickness = 1.0f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.08f}, .bot_lid_mid = {0, 0.12f},
                 .iris_center = {0, -0.1f},
                 .pupil_scale = 0.65f, .shine_intensity = 0.9f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.08f}, .bot_lid_mid = {0, 0.12f},
                 .iris_center = {0, -0.1f},
                 .pupil_scale = 0.65f, .shine_intensity = 0.9f},
            },
            .mouth = {
                .left_corner = {0.2f, 0}, .right_corner = {-0.2f, 0},
                .upper_lip_mid = {0, -0.15f}, .lower_lip_mid = {0, 0.05f},
                .openness = 0.0f,
            },
            .decor = {.blush = 0.55f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {250, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {250, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {250, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {200, 30, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {200, 30, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {350, 120, PATH_OVERSPEED},
            [COMPONENT_DECOR]      = {300, 80, PATH_EASE_OUT},
        },
    },
    // [2] SAD
    {
        .name = "SAD",
        .target = {
            .face = {.roundness = 0.4f},
            .brow = {
                {.inner = {0, -0.1f}, .arch = {0.05f, 0}, .tail = {0.1f, 0.35f}, .thickness = 1.0f},
                {.inner = {0, -0.1f}, .arch = {-0.05f, 0}, .tail = {-0.1f, 0.35f}, .thickness = 1.0f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.18f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.3f},
                 .pupil_scale = 0.55f, .shine_intensity = 0.5f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.18f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.3f},
                 .pupil_scale = 0.55f, .shine_intensity = 0.5f},
            },
            .mouth = {
                .left_corner = {0.05f, 0.15f}, .right_corner = {-0.05f, 0.15f},
                .upper_lip_mid = {0, 0.1f}, .lower_lip_mid = {0, 0.15f},
                .openness = 0.1f,
            },
            .decor = {.blush = 0.2f, .tears = 0.6f, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {400, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {400, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {400, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {350, 50, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_RIGHT]  = {350, 50, PATH_EASE_IN_OUT},
            [COMPONENT_MOUTH]      = {500, 150, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {400, 200, PATH_EASE_IN},
        },
    },
    // [3] SURPRISED
    {
        .name = "SURPRISED",
        .target = {
            .face = {.roundness = 0.65f},
            .brow = {
                {.inner = {0, -0.5f}, .arch = {0, -0.7f}, .tail = {0, -0.55f}, .thickness = 0.9f},
                {.inner = {0, -0.5f}, .arch = {0, -0.7f}, .tail = {0, -0.55f}, .thickness = 0.9f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.15f}, .bot_lid_mid = {0, 0.15f},
                 .iris_center = {0, 0},
                 .pupil_scale = 0.45f, .shine_intensity = 1.0f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.15f}, .bot_lid_mid = {0, 0.15f},
                 .iris_center = {0, 0},
                 .pupil_scale = 0.45f, .shine_intensity = 1.0f},
            },
            .mouth = {
                .left_corner = {0, 0.1f}, .right_corner = {0, 0.1f},
                .upper_lip_mid = {0, -0.1f}, .lower_lip_mid = {0, 0.25f},
                .openness = 0.5f,
            },
            .decor = {.blush = 0.15f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0.2f},
        },
        .timing = {
            [COMPONENT_FACE]       = {150, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {120, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {120, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {120, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {120, 0, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {180, 80, PATH_OVERSPEED},
            [COMPONENT_DECOR]      = {200, 50, PATH_EASE_OUT},
        },
    },
    // [4] SLEEPY
    {
        .name = "SLEEPY",
        .target = {
            .face = {.roundness = 0.5f},
            .brow = {
                {.inner = {0, 0.05f}, .arch = {0, 0.15f}, .tail = {0, 0.1f}, .thickness = 1.0f},
                {.inner = {0, 0.05f}, .arch = {0, 0.15f}, .tail = {0, 0.1f}, .thickness = 1.0f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.3f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.2f},
                 .pupil_scale = 0.5f, .shine_intensity = 0.3f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.3f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.2f},
                 .pupil_scale = 0.5f, .shine_intensity = 0.3f},
            },
            .mouth = {
                .left_corner = {0, 0.05f}, .right_corner = {0, 0.05f},
                .upper_lip_mid = {0, 0.05f}, .lower_lip_mid = {0, 0.0f},
                .openness = 0.2f,
            },
            .decor = {.blush = 0.1f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {600, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {600, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {600, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {500, 30, PATH_EASE_IN},
            [COMPONENT_EYE_RIGHT]  = {500, 30, PATH_EASE_IN},
            [COMPONENT_MOUTH]      = {600, 100, PATH_EASE_IN_OUT},
            [COMPONENT_DECOR]      = {500, 50, PATH_EASE_IN_OUT},
        },
    },
    // [5] ANGRY
    {
        .name = "ANGRY",
        .target = {
            .face = {.roundness = 0.4f},
            .brow = {
                {.inner = {0.05f, -0.15f}, .arch = {0, -0.3f}, .tail = {-0.1f, 0.25f}, .thickness = 1.3f},
                {.inner = {-0.05f, -0.15f}, .arch = {0, -0.3f}, .tail = {0.1f, 0.25f}, .thickness = 1.3f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.05f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.7f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.05f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.7f},
            },
            .mouth = {
                .left_corner = {0.1f, 0.1f}, .right_corner = {-0.1f, 0.1f},
                .upper_lip_mid = {0, 0.05f}, .lower_lip_mid = {0, 0.1f},
                .openness = 0.0f,
            },
            .decor = {.blush = 0, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {200, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {200, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {200, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {180, 20, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {180, 20, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {250, 80, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {200, 0, PATH_EASE_OUT},
        },
    },
    // [6] BORED
    {
        .name = "BORED",
        .target = {
            .face = {.roundness = 0.5f},
            .brow = {
                {.inner = {0, 0}, .arch = {0, 0.15f}, .tail = {0, 0.1f}, .thickness = 0.9f},
                {.inner = {0, 0}, .arch = {0, 0.15f}, .tail = {0, 0.1f}, .thickness = 0.9f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.2f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, -0.2f},
                 .pupil_scale = 0.55f, .shine_intensity = 0.5f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.2f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, -0.2f},
                 .pupil_scale = 0.55f, .shine_intensity = 0.5f},
            },
            .mouth = {
                .left_corner = {0.05f, 0.05f}, .right_corner = {-0.05f, 0.05f},
                .upper_lip_mid = {0, 0.05f}, .lower_lip_mid = {0, 0.02f},
                .openness = 0.05f,
            },
            .decor = {.blush = 0, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {500, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {500, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {500, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {450, 40, PATH_EASE_IN},
            [COMPONENT_EYE_RIGHT]  = {450, 40, PATH_EASE_IN},
            [COMPONENT_MOUTH]      = {500, 100, PATH_EASE_IN_OUT},
            [COMPONENT_DECOR]      = {400, 0, PATH_EASE_OUT},
        },
    },
    // [7] EXCITED
    {
        .name = "EXCITED",
        .target = {
            .face = {.roundness = 0.7f},
            .brow = {
                {.inner = {0, -0.25f}, .arch = {0, -0.5f}, .tail = {0.05f, -0.35f}, .thickness = 0.85f},
                {.inner = {0, -0.25f}, .arch = {0, -0.5f}, .tail = {-0.05f, -0.35f}, .thickness = 0.85f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.1f}, .bot_lid_mid = {0, 0.1f},
                 .iris_center = {0, -0.1f},
                 .pupil_scale = 0.7f, .shine_intensity = 1.0f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.1f}, .bot_lid_mid = {0, 0.1f},
                 .iris_center = {0, -0.1f},
                 .pupil_scale = 0.7f, .shine_intensity = 1.0f},
            },
            .mouth = {
                .left_corner = {0.25f, -0.1f}, .right_corner = {-0.25f, -0.1f},
                .upper_lip_mid = {0, -0.2f}, .lower_lip_mid = {0, 0.1f},
                .openness = 0.3f,
            },
            .decor = {.blush = 0.4f, .tears = 0, .stars = 0.65f, .sweat = 0, .sparkle = 0.3f},
        },
        .timing = {
            [COMPONENT_FACE]       = {180, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {150, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {150, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {150, 20, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {150, 20, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {200, 60, PATH_OVERSPEED},
            [COMPONENT_DECOR]      = {200, 30, PATH_EASE_OUT},
        },
    },
    // [8] CONFUSED
    {
        .name = "CONFUSED",
        .target = {
            .face = {.roundness = 0.5f},
            .brow = {
                {.inner = {0, -0.15f}, .arch = {0, -0.25f}, .tail = {0, 0}, .thickness = 1.0f},
                {.inner = {0, 0}, .arch = {0, 0.1f}, .tail = {0, 0.2f}, .thickness = 1.0f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.1f}, .bot_lid_mid = {0, 0},
                 .iris_center = {0.3f, 0.1f},
                 .pupil_scale = 0.5f, .shine_intensity = 0.6f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.05f}, .bot_lid_mid = {0, 0.08f},
                 .iris_center = {0.3f, 0.1f},
                 .pupil_scale = 0.5f, .shine_intensity = 0.6f},
            },
            .mouth = {
                .left_corner = {0, 0.05f}, .right_corner = {-0.15f, 0.05f},
                .upper_lip_mid = {0, 0.05f}, .lower_lip_mid = {0, 0.02f},
                .openness = 0.1f,
            },
            .decor = {.blush = 0.1f, .tears = 0, .stars = 0, .sweat = 0.2f, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {300, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {300, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {300, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {250, 30, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {250, 30, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {350, 120, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {300, 50, PATH_EASE_OUT},
        },
    },
    // [9] CONTENT
    {
        .name = "CONTENT",
        .target = {
            .face = {.roundness = 0.6f},
            .brow = {
                {.inner = {0, -0.1f}, .arch = {0, -0.3f}, .tail = {0, -0.15f}, .thickness = 0.9f},
                {.inner = {0, -0.1f}, .arch = {0, -0.3f}, .tail = {0, -0.15f}, .thickness = 0.9f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.28f}, .bot_lid_mid = {0, 0.02f},
                 .iris_center = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.4f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.28f}, .bot_lid_mid = {0, 0.02f},
                 .iris_center = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.4f},
            },
            .mouth = {
                .left_corner = {0.15f, -0.05f}, .right_corner = {-0.15f, -0.05f},
                .upper_lip_mid = {0, -0.1f}, .lower_lip_mid = {0, 0.02f},
                .openness = 0.0f,
            },
            .decor = {.blush = 0.75f, .tears = 0, .stars = 0.2f, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {500, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {500, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {500, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {450, 50, PATH_EASE_IN},
            [COMPONENT_EYE_RIGHT]  = {450, 50, PATH_EASE_IN},
            [COMPONENT_MOUTH]      = {500, 100, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {450, 150, PATH_EASE_IN_OUT},
        },
    },
    // [10] COLD
    {
        .name = "COLD",
        .target = {
            .face = {.roundness = 0.4f},
            .brow = {
                {.inner = {0, 0.05f}, .arch = {0, 0.2f}, .tail = {0, 0.1f}, .thickness = 1.1f},
                {.inner = {0, 0.05f}, .arch = {0, 0.2f}, .tail = {0, 0.1f}, .thickness = 1.1f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.2f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.2f},
                 .pupil_scale = 0.4f, .shine_intensity = 0.3f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.2f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.2f},
                 .pupil_scale = 0.4f, .shine_intensity = 0.3f},
            },
            .mouth = {
                .left_corner = {0, 0.05f}, .right_corner = {0, 0.05f},
                .upper_lip_mid = {0, 0.02f}, .lower_lip_mid = {0, 0.08f},
                .openness = 0.0f,
            },
            .decor = {.blush = 0.3f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {400, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {400, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {400, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {350, 40, PATH_EASE_IN},
            [COMPONENT_EYE_RIGHT]  = {350, 40, PATH_EASE_IN},
            [COMPONENT_MOUTH]      = {400, 100, PATH_EASE_IN_OUT},
            [COMPONENT_DECOR]      = {350, 60, PATH_EASE_IN_OUT},
        },
    },
    // [11] WARM
    {
        .name = "WARM",
        .target = {
            .face = {.roundness = 0.65f},
            .brow = {
                {.inner = {0, -0.15f}, .arch = {0, -0.35f}, .tail = {0, -0.2f}, .thickness = 0.9f},
                {.inner = {0, -0.15f}, .arch = {0, -0.35f}, .tail = {0, -0.2f}, .thickness = 0.9f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.03f}, .bot_lid_mid = {0, 0.1f},
                 .iris_center = {0, -0.15f},
                 .pupil_scale = 0.65f, .shine_intensity = 0.85f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.03f}, .bot_lid_mid = {0, 0.1f},
                 .iris_center = {0, -0.15f},
                 .pupil_scale = 0.65f, .shine_intensity = 0.85f},
            },
            .mouth = {
                .left_corner = {0.18f, -0.08f}, .right_corner = {-0.18f, -0.08f},
                .upper_lip_mid = {0, -0.15f}, .lower_lip_mid = {0, 0.02f},
                .openness = 0.05f,
            },
            .decor = {.blush = 0.5f, .tears = 0, .stars = 0.3f, .sweat = 0, .sparkle = 0.1f},
        },
        .timing = {
            [COMPONENT_FACE]       = {350, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {300, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {300, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {280, 30, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {280, 30, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {350, 80, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {300, 50, PATH_EASE_OUT},
        },
    },
    // [12] HEART_EYES
    {
        .name = "HEART_EYES",
        .target = {
            .face = {.roundness = 0.7f},
            .brow = {
                {.inner = {0, -0.2f}, .arch = {0, -0.45f}, .tail = {0.05f, -0.3f}, .thickness = 0.85f},
                {.inner = {0, -0.2f}, .arch = {0, -0.45f}, .tail = {-0.05f, -0.3f}, .thickness = 0.85f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.05f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0},
                 .pupil_scale = 0.0f, .shine_intensity = 0.9f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.05f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0},
                 .pupil_scale = 0.0f, .shine_intensity = 0.9f},
            },
            .mouth = {
                .left_corner = {0.2f, -0.1f}, .right_corner = {-0.2f, -0.1f},
                .upper_lip_mid = {0, -0.2f}, .lower_lip_mid = {0, 0.05f},
                .openness = 0.15f,
            },
            .decor = {.blush = 0.65f, .tears = 0, .stars = 0.9f, .sweat = 0, .sparkle = 0.5f},
        },
        .timing = {
            [COMPONENT_FACE]       = {250, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {200, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {200, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {200, 20, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {200, 20, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {280, 60, PATH_OVERSPEED},
            [COMPONENT_DECOR]      = {250, 30, PATH_EASE_OUT},
        },
    },
};

const uint8_t EXPRESSION_COUNT = sizeof(EXPRESSION_DEFS) / sizeof(EXPRESSION_DEFS[0]);

/* ── Classic sprite set declaration (implemented in sprite_classic.c) ── */

extern const sprite_set_t SPRITE_CLASSIC;
```

- [ ] **Step 2: Commit**

```bash
git add components/face_system/face_model.c
git commit -m "feat: add face_model.c - NEUTRAL state + 13 expression presets
Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 4: sprite_classic.h/c — Classic sprite renderer

**Files:**
- Create: `components/face_system/sprite_classic.h`
- Create: `components/face_system/sprite_classic.c`

- [ ] **Step 1: Write sprite_classic.h**

Write `components/face_system/sprite_classic.h`:
```c
#ifndef SPRITE_CLASSIC_H
#define SPRITE_CLASSIC_H

#include "face_model.h"

extern const sprite_set_t SPRITE_CLASSIC;

#endif
```

- [ ] **Step 2: Write sprite_classic.c (core rendering framework + draw_face)**

The renderer migrates scanline logic from `expressive_eyes.c`. Each draw function gets the current `y` scanline, reads `face_state_t` params, transforms them to screen pixels via `sprite_set_t` geometry, and writes into `buf`.

Write `components/face_system/sprite_classic.c`:
```c
#include "sprite_classic.h"
#include "face_palette.h"
#include <math.h>
#include <string.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

/* ── Utility: dist squared ───────────────────────────────── */
static inline float dist_sq(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

/* ── Utility: fast integer sqrt (for bg gradient LUT) ────── */
static inline int fast_isqrt(int n) {
    int r = 0, bit = 1 << 14;
    while (bit > 0) {
        if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return r;
}

/* ── Utility: blend two RGB565 colors ────────────────────── */
static inline uint16_t blend_colors(uint16_t c1, uint16_t c2, float t) {
    if (t <= 0) return c1;
    if (t >= 1) return c2;
    int t256 = (int)(t * 256.0f);
    int r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
    int r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
    int r = r1 + (((r2 - r1) * t256 + 128) >> 8);
    int g = g1 + (((g2 - g1) * t256 + 128) >> 8);
    int b = b1 + (((b2 - b1) * t256 + 128) >> 8);
    return (r << 11) | (g << 5) | b;
}

/* ── Background gradient LUT ──────────────────────────────── */
#define BG_GRADIENT_MAX 171
static uint16_t bg_lut[BG_GRADIENT_MAX];
static const uint16_t *active_pal = NULL;

static void build_bg_lut(const uint16_t *pal) {
    for (int d = 0; d < BG_GRADIENT_MAX; d++) {
        float t = d / 160.0f;
        if (t > 1.0f) t = 1.0f;
        bg_lut[d] = blend_colors(pal[PAL_BG], pal[PAL_BG_EDGE], t);
    }
}

/* ── draw_face: radial gradient background ───────────────── */
static void draw_face(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int dy = y - CENTER_Y;
    int dy_sq = dy * dy;
    for (int x = 0; x < SCREEN_W; x++) {
        int dx = x - CENTER_X;
        int d = fast_isqrt(dx * dx + dy_sq);
        if (d >= BG_GRADIENT_MAX) d = BG_GRADIENT_MAX - 1;
        buf[x] = bg_lut[d];
    }
}

/* ── draw_eye: render one eye (migrated from expressive_eyes render_eye) ─ */
static void draw_eye_impl(int y, const eye_params_t *ep,
                          int eye_cx, int eye_cy, const uint16_t *pal, uint16_t *buf) {
    const float eye_r = 36.0f;
    const float iris_r = 30.0f;
    const float pupil_base_r = 13.0f;

    float fy = y - eye_cy;
    if (fy < -eye_r - 2 || fy > eye_r + 2) return;

    int x_start = eye_cx - (int)eye_r - 2;
    int x_end   = eye_cx + (int)eye_r + 2;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    // Map normalized keypoints to pixel offsets
    float iris_cx = ep->iris_center.dx * 7.0f;
    float iris_cy = ep->iris_center.dy * 7.0f;
    float iris_r_sq = iris_r * iris_r;
    float pupil_r = pupil_base_r * ep->pupil_scale;
    float pupil_r_sq = pupil_r * pupil_r;

    // Eyelid: map top_lid_mid.dy to lid openness
    // Positive top_lid_mid.dy = more closed (eyelid droops down)
    float lid_open = 1.0f - (ep->top_lid_mid.dy * 1.4f);
    if (lid_open < 0.05f) lid_open = 0.05f;
    if (lid_open > 1.0f) lid_open = 1.0f;

    float base_top = -eye_r * (1.0f - lid_open);
    float base_bot =  eye_r * (1.0f - lid_open);

    // Shine position
    float sh_cx = iris_cx - 6.0f, sh_cy = iris_cy - 7.0f;
    float sh_r = 5.5f, sh_r_sq = sh_r * sh_r;

    uint16_t iris_dark = blend_colors(pal[PAL_IRIS], pal[PAL_PUPIL], 0.65f);

    for (int x = x_start; x <= x_end; x++) {
        float fx = x - eye_cx;
        float r_sq = fx * fx + fy * fy;
        if (r_sq >= eye_r * eye_r) continue;

        // Arc-shaped eyelid
        float arc = 1.0f - (fx * fx) / (eye_r * eye_r);
        float top_lid = base_top + 5.0f * lid_open * arc;
        float bot_lid = base_bot - 3.0f * lid_open * arc;
        if (fy < top_lid || fy > bot_lid) continue;

        // Edge antialiasing
        float edge_dist = eye_r - sqrtf(r_sq);
        bool is_edge = (edge_dist < 1.5f);

        if (is_edge) {
            float iris_d_sq = dist_sq(fx, fy, iris_cx, iris_cy);
            float sh_d_sq = dist_sq(fx, fy, sh_cx, sh_cy);
            uint16_t inner;
            if (sh_d_sq < sh_r_sq && iris_d_sq < iris_r_sq)
                inner = blend_colors(pal[PAL_IRIS], pal[PAL_SHINE], 0.85f);
            else if (iris_d_sq < pupil_r_sq)
                inner = pal[PAL_PUPIL];
            else if (iris_d_sq < iris_r_sq) {
                float grad_t = sqrtf(iris_d_sq) / iris_r;
                inner = blend_colors(pal[PAL_IRIS], iris_dark, grad_t * grad_t);
            } else {
                continue;
            }
            buf[x] = blend_colors(buf[x], inner, edge_dist / 1.5f);
            continue;
        }

        // Main eye fill
        float iris_d_sq = dist_sq(fx, fy, iris_cx, iris_cy);
        float pupil_d_sq = dist_sq(fx, fy, iris_cx + ep->iris_center.dx * 2.0f,
                                             iris_cy + ep->iris_center.dy * 2.0f);
        float sh_d_sq = dist_sq(fx, fy, sh_cx, sh_cy);

        if (sh_d_sq < sh_r_sq && iris_d_sq < iris_r_sq) {
            float t = (1.0f - sh_d_sq / sh_r_sq) * ep->shine_intensity;
            buf[x] = blend_colors(pal[PAL_IRIS], pal[PAL_SHINE], t);
        } else if (iris_d_sq < iris_r_sq) {
            float grad_t = sqrtf(iris_d_sq) / iris_r;
            buf[x] = blend_colors(pal[PAL_IRIS], iris_dark, grad_t * grad_t);
        } else if (pupil_d_sq < pupil_r_sq) {
            buf[x] = pal[PAL_PUPIL];
        }
    }
}

static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)sp->eye_half_spacing;
    int eye_cy = CENTER_Y;
    draw_eye_impl(y, &st->eye[0], eye_cx, eye_cy, sp->pal, buf);
}

static void draw_eye_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing;
    int eye_cy = CENTER_Y;
    draw_eye_impl(y, &st->eye[1], eye_cx, eye_cy, sp->pal, buf);
}

/* ── draw_brow: arc from inner→arch→tail ─────────────────── */
static void draw_brow_impl(int y, const brow_params_t *bp, int eye_cx, int eye_cy,
                           const sprite_set_t *sp, const uint16_t *pal, uint16_t *buf) {
    float brow_y_px = eye_cy + sp->brow_y_offset;
    float dy = y - brow_y_px;
    float half_thick = bp->thickness * 4.0f;
    if (dy < -half_thick - 2 || dy > half_thick + 2) return;

    // Keypoints in screen pixels relative to eye center
    float inner_x = eye_cx + bp->inner.dx * 25.0f;
    float inner_y = brow_y_px + bp->inner.dy * 15.0f;
    float arch_x  = eye_cx + bp->arch.dx * 15.0f;
    float arch_y  = brow_y_px + bp->arch.dy * 20.0f;
    float tail_x  = eye_cx + bp->tail.dx * 30.0f;
    float tail_y  = brow_y_px + bp->tail.dy * 15.0f;

    // Quadratic bezier through the three points (approximate)
    int x_start = (int)inner_x - 4;
    int x_end   = (int)tail_x + 4;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    for (int x = x_start; x <= x_end; x++) {
        // t along the curve (0 at inner, 1 at tail)
        float t = (float)(x - inner_x) / (tail_x - inner_x + 0.001f);
        if (t < 0.0f || t > 1.0f) continue;

        // Quadratic bezier: B(t) = (1-t)^2*P0 + 2(1-t)t*P1 + t^2*P2
        float curve_y = (1-t)*(1-t)*inner_y + 2*(1-t)*t*arch_y + t*t*tail_y;
        float dist = fabsf(y - curve_y);
        if (dist < half_thick) {
            float alpha = (half_thick - dist) / half_thick * 0.85f;
            buf[x] = blend_colors(buf[x], pal[PAL_BROW], alpha);
        }
    }
}

static void draw_brow_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)sp->eye_half_spacing;
    draw_brow_impl(y, &st->brow[0], eye_cx, CENTER_Y, sp, sp->pal, buf);
}

static void draw_brow_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing;
    draw_brow_impl(y, &st->brow[1], eye_cx, CENTER_Y, sp, sp->pal, buf);
}

/* ── draw_mouth: simple mouth shape ──────────────────────── */
static void draw_mouth(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const mouth_params_t *mp = &st->mouth;
    int mouth_cy = CENTER_Y + (int)sp->mouth_y_center;

    float half_width = 25.0f;
    float dy = y - mouth_cy;
    int x_start = CENTER_X - (int)half_width - 3;
    int x_end   = CENTER_X + (int)half_width + 3;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    // Mouth corner and lip positions in screen pixels
    float lcx = CENTER_X + mp->left_corner.dx * half_width;
    float rcx = CENTER_X + mp->right_corner.dx * half_width;
    float uly = mouth_cy + mp->upper_lip_mid.dy * 15.0f;
    float lly = mouth_cy + mp->lower_lip_mid.dy * 15.0f;

    float openness_offset = mp->openness * 12.0f;

    for (int x = x_start; x <= x_end; x++) {
        float t = (x - lcx) / (rcx - lcx + 0.001f);
        if (t < 0.0f || t > 1.0f) continue;

        // Upper lip: bezier from left_corner up to upper_lip_mid down to right_corner
        float upper_y = (1-t)*(1-t)*mouth_cy + 2*(1-t)*t*uly + t*t*mouth_cy;
        // Lower lip: bezier from left_corner down to lower_lip_mid up to right_corner
        float lower_y = (1-t)*(1-t)*mouth_cy + 2*(1-t)*t*(lly + openness_offset) + t*t*mouth_cy;

        if (y >= upper_y - 1.5f && y <= lower_y + 1.5f) {
            // Inside mouth (if open)
            if (y > upper_y + 1.5f && y < lower_y - 1.5f && mp->openness > 0.05f) {
                // Dark interior
                buf[x] = blend_colors(buf[x], pal[PAL_PUPIL], 0.7f);
            } else {
                // Lip edge
                float alpha = 0.7f;
                buf[x] = blend_colors(buf[x], pal[PAL_MOUTH], alpha);
            }
        }
    }
}

/* ── draw_blush: rosy cheeks ──────────────────────────────── */
static void draw_blush(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    float level = st->decor.blush;
    if (level <= 0) return;
    const uint16_t *pal = sp->pal;

    int blush_cy = CENTER_Y + (int)sp->blush_y_offset;
    int left_cx  = CENTER_X - 60;
    int right_cx = CENTER_X + 60;
    float blush_r = 24.0f;
    float r_sq = blush_r * blush_r;

    float dy = y - blush_cy;
    if (dy < -blush_r || dy > blush_r) return;

    int x_start = left_cx - (int)blush_r - 1;
    int x_end = right_cx + (int)blush_r + 1;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    for (int x = x_start; x <= x_end; x++) {
        float d_left = dist_sq(x, y, left_cx, blush_cy);
        float d_right = dist_sq(x, y, right_cx, blush_cy);
        if (d_left < r_sq || d_right < r_sq) {
            float d = (d_left < d_right) ? d_left : d_right;
            float t = (1.0f - d / r_sq) * level * 0.7f;
            buf[x] = blend_colors(buf[x], pal[PAL_BLUSH], t);
        }
    }
}

/* ── draw_decor_overlay: tears, stars, sweat, sparkle ────── */
static void draw_decor_overlay(int y, const face_state_t *st,
                                const sprite_set_t *sp, uint16_t *buf) {
    const decor_params_t *dp = &st->decor;
    const uint16_t *pal = sp->pal;

    // Tears (below eyes)
    if (dp->tears > 0) {
        int tear_cy = CENTER_Y + 30;
        float dy = y - tear_cy;
        if (dy > 0 && dy < 30 * dp->tears) {
            int lcx = CENTER_X - 35, rcx = CENTER_X + 35;
            float r_left = 6.0f - dy * 0.08f;
            float r_right = 6.5f - dy * 0.08f;
            int x_start = lcx - (int)r_left - 2;
            int x_end = rcx + (int)r_right + 2;
            if (x_start < 0) x_start = 0;
            if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;
            for (int x = x_start; x <= x_end; x++) {
                if (dist_sq(x, y, lcx, tear_cy + 5) < r_left * r_left)
                    buf[x] = blend_colors(buf[x], pal[PAL_TEAR], 0.8f);
                if (dist_sq(x, y, rcx, tear_cy + 8) < r_right * r_right)
                    buf[x] = blend_colors(buf[x], pal[PAL_TEAR], 0.8f);
            }
        }
    }

    // Stars
    if (dp->stars > 0) {
        static const int star_pos[4][2] = {
            {CENTER_X - 20, CENTER_Y - 15}, {CENTER_X + 20, CENTER_Y - 15},
            {CENTER_X - 35, CENTER_Y + 5},   {CENTER_X + 35, CENTER_Y + 5},
        };
        for (int i = 0; i < 4; i++) {
            int sx = star_pos[i][0], sy = star_pos[i][1];
            if (y < sy - 7 || y > sy + 7) continue;
            int x_start = sx - 7, x_end = sx + 7;
            if (x_start < 0) x_start = 0;
            if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;
            for (int x = x_start; x <= x_end; x++) {
                float d_sq = dist_sq(x, y, sx, sy);
                if (d_sq < 36.0f) {
                    float t = (1.0f - d_sq / 36.0f) * dp->stars;
                    buf[x] = blend_colors(buf[x], pal[PAL_STAR], t * 0.7f);
                }
            }
        }
    }

    // Sweat drops
    if (dp->sweat > 0) {
        int sx = CENTER_X + 55, sy = CENTER_Y - 45;
        float d_sq = dist_sq(CENTER_X + 55, y, sx, sy);
        float r = 5.0f * dp->sweat;
        if (d_sq < r * r) {
            buf[CENTER_X + 55] = blend_colors(buf[CENTER_X + 55], pal[PAL_TEAR], dp->sweat * 0.6f);
        }
    }

    // Sparkle (subtle glitter overlay near eyes)
    if (dp->sparkle > 0) {
        static const int sparkle_pos[6][2] = {
            {CENTER_X - 45, CENTER_Y - 40}, {CENTER_X + 45, CENTER_Y - 40},
            {CENTER_X - 50, CENTER_Y + 35}, {CENTER_X + 50, CENTER_Y + 35},
            {CENTER_X - 25, CENTER_Y - 50}, {CENTER_X + 25, CENTER_Y - 50},
        };
        for (int i = 0; i < 6; i++) {
            int sx = sparkle_pos[i][0], sy = sparkle_pos[i][1];
            if (y < sy - 4 || y > sy + 4) continue;
            for (int x = sx - 4; x <= sx + 4; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                float d_sq = dist_sq(x, y, sx, sy);
                if (d_sq < 9.0f) {
                    float t = (1.0f - d_sq / 9.0f) * dp->sparkle * 0.5f;
                    buf[x] = blend_colors(buf[x], pal[PAL_SHINE], t);
                }
            }
        }
    }
}

/* ── Sprite definition ────────────────────────────────────── */

const sprite_set_t SPRITE_CLASSIC = {
    .name = "classic",
    .eye_radius = 36.0f,
    .eye_half_spacing = 26.0f,
    .mouth_y_center = 50.0f,
    .brow_y_offset = -38.0f,
    .blush_y_offset = 35.0f,
    .draw_face = draw_face,
    .draw_blush = draw_blush,
    .draw_mouth = draw_mouth,
    .draw_eye_left = draw_eye_left,
    .draw_eye_right = draw_eye_right,
    .draw_brow_left = draw_brow_left,
    .draw_brow_right = draw_brow_right,
    .draw_decor_overlay = draw_decor_overlay,
    .pal = PALETTE_BLACK,  // default
};
```

- [ ] **Step 3: Commit**

```bash
git add components/face_system/sprite_classic.h components/face_system/sprite_classic.c
git commit -m "feat: add sprite_classic - migrated scanline renderer from expressive_eyes
Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 5: face_animator.h/c — LVGL animation driver

**Files:**
- Create: `components/face_system/face_animator.h`
- Create: `components/face_system/face_animator.c`

- [ ] **Step 1: Write face_animator.h**

Write `components/face_system/face_animator.h`:
```c
#ifndef FACE_ANIMATOR_H
#define FACE_ANIMATOR_H

#include "face_model.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void animator_init(void);
void animator_tick(void);  // call lv_timer_handler, drive animations

// High-level: switch to expression with component-level timing
void animator_set_expression(expression_id_t id);

// Low-level: animate single component to target params
void animator_set_component(face_component_t comp,
                            const void *target_params,
                            uint32_t duration_ms,
                            uint32_t delay_ms,
                            anim_path_type_t path_type);

// Instant set (no animation)
void animator_set_component_instant(face_component_t comp, const void *target_params);

// Read current face state
const face_state_t *animator_get_state(void);

// Cancel all running animations (snapshot current interpolated state)
void animator_cancel_all(void);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 2: Write face_animator.c**

Write `components/face_system/face_animator.c`:
```c
#include "face_animator.h"
#include "lvgl.h"
#include <string.h>

/* ── Component lerp functions ────────────────────────────── */

static void face_params_lerp(const face_params_t *a, const face_params_t *b,
                             float t, face_params_t *out) {
    out->roundness = a->roundness + (b->roundness - a->roundness) * t;
}

static void brow_params_lerp(const brow_params_t *a, const brow_params_t *b,
                             float t, brow_params_t *out) {
    out->inner.dx = a->inner.dx + (b->inner.dx - a->inner.dx) * t;
    out->inner.dy = a->inner.dy + (b->inner.dy - a->inner.dy) * t;
    out->arch.dx = a->arch.dx + (b->arch.dx - a->arch.dx) * t;
    out->arch.dy = a->arch.dy + (b->arch.dy - a->arch.dy) * t;
    out->tail.dx = a->tail.dx + (b->tail.dx - a->tail.dx) * t;
    out->tail.dy = a->tail.dy + (b->tail.dy - a->tail.dy) * t;
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
    out->pupil_scale = a->pupil_scale + (b->pupil_scale - a->pupil_scale) * t;
    out->shine_intensity = a->shine_intensity + (b->shine_intensity - a->shine_intensity) * t;
}

static void mouth_params_lerp(const mouth_params_t *a, const mouth_params_t *b,
                              float t, mouth_params_t *out) {
    out->left_corner.dx = a->left_corner.dx + (b->left_corner.dx - a->left_corner.dx) * t;
    out->left_corner.dy = a->left_corner.dy + (b->left_corner.dy - a->left_corner.dy) * t;
    out->right_corner.dx = a->right_corner.dx + (b->right_corner.dx - a->right_corner.dx) * t;
    out->right_corner.dy = a->right_corner.dy + (b->right_corner.dy - a->right_corner.dy) * t;
    out->upper_lip_mid.dx = a->upper_lip_mid.dx + (b->upper_lip_mid.dx - a->upper_lip_mid.dx) * t;
    out->upper_lip_mid.dy = a->upper_lip_mid.dy + (b->upper_lip_mid.dy - a->upper_lip_mid.dy) * t;
    out->lower_lip_mid.dx = a->lower_lip_mid.dx + (b->lower_lip_mid.dx - a->lower_lip_mid.dx) * t;
    out->lower_lip_mid.dy = a->lower_lip_mid.dy + (b->lower_lip_mid.dy - a->lower_lip_mid.dy) * t;
    out->openness = a->openness + (b->openness - a->openness) * t;
}

static void decor_params_lerp(const decor_params_t *a, const decor_params_t *b,
                              float t, decor_params_t *out) {
    out->blush = a->blush + (b->blush - a->blush) * t;
    out->tears = a->tears + (b->tears - a->tears) * t;
    out->stars = a->stars + (b->stars - a->stars) * t;
    out->sweat = a->sweat + (b->sweat - a->sweat) * t;
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

/* Returns pointer to component params within face_state_t */
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

/* ── Animation state ─────────────────────────────────────── */

static face_state_t current_state;
static lv_anim_t anims[COMPONENT_COUNT];
static bool anim_active[COMPONENT_COUNT];

// "From" states for each component (snapshot at animation start)
static uint8_t from_states[COMPONENT_COUNT][sizeof(eye_params_t)]; // max size

/* ── LVGL path callback bridge ────────────────────────────── */

static lv_anim_path_cb_t path_map(anim_path_type_t type) {
    switch (type) {
        case PATH_LINEAR:       return lv_anim_path_linear;
        case PATH_EASE_OUT:     return lv_anim_path_ease_out;
        case PATH_EASE_IN:      return lv_anim_path_ease_in;
        case PATH_EASE_IN_OUT:  return lv_anim_path_ease_in_out;
        case PATH_OVERSPEED:    return lv_anim_path_overshoot;
        default:                return lv_anim_path_ease_out;
    }
}

/* ── Per-animation callback: lerp from→target based on progress ─ */

typedef struct {
    face_component_t comp;
    uint8_t target[sizeof(eye_params_t)];
} anim_user_data_t;

static anim_user_data_t anim_data[COMPONENT_COUNT];

static void anim_exec_cb(void *var, int32_t value) {
    // 'var' points to the component in current_state
    // 'value' is progress 0..1024 (LVGL anim range) → map to 0..1
    (void)var;
    int idx = -1;
    for (int i = 0; i < COMPONENT_COUNT; i++) {
        if (var == get_component_ptr(&current_state, (face_component_t)i)) {
            idx = i; break;
        }
    }
    if (idx < 0) return;
    float t = value / 1024.0f;
    if (t < 0) t = 0; if (t > 1) t = 1;

    face_component_t comp = (face_component_t)idx;
    lerp_funcs[comp](from_states[comp], anim_data[comp].target, t,
                     get_component_ptr(&current_state, comp));
}

static void anim_ready_cb(lv_anim_t *a) {
    for (int i = 0; i < COMPONENT_COUNT; i++) {
        if (&anims[i] == a) {
            anim_active[i] = false;
            break;
        }
    }
}

/* ── Public API ───────────────────────────────────────────── */

void animator_init(void) {
    current_state = FACE_STATE_NEUTRAL;
    memset(anim_active, 0, sizeof(anim_active));
    memset(anims, 0, sizeof(anims));
}

void animator_tick(void) {
    lv_timer_handler();
}

void animator_set_expression(expression_id_t id) {
    if (id >= EXPRESSION_COUNT) return;
    const expression_def_t *expr = &EXPRESSION_DEFS[id];

    for (int i = 0; i < COMPONENT_COUNT; i++) {
        // Cancel existing anim for this component
        if (anim_active[i]) {
            lv_anim_del(get_component_ptr(&current_state, (face_component_t)i), anim_exec_cb);
            anim_active[i] = false;
        }

        // Snapshot current state as new "from"
        size_t sz = param_sizes[i];
        void *src = get_component_ptr(&current_state, (face_component_t)i);
        memcpy(from_states[i], src, sz);

        // Copy target
        const void *target = get_component_ptr((face_state_t *)&expr->target, (face_component_t)i);
        memcpy(anim_data[i].target, target, sz);
        anim_data[i].comp = (face_component_t)i;

        // Create LVGL anim
        lv_anim_init(&anims[i]);
        lv_anim_set_var(&anims[i], src);
        lv_anim_set_values(&anims[i], 0, 1024);
        lv_anim_set_time(&anims[i], expr->timing[i].duration_ms);
        lv_anim_set_delay(&anims[i], expr->timing[i].delay_ms);
        lv_anim_set_path_cb(&anims[i], path_map(expr->timing[i].path_type));
        lv_anim_set_exec_cb(&anims[i], anim_exec_cb);
        lv_anim_set_ready_cb(&anims[i], anim_ready_cb);
        lv_anim_start(&anims[i]);
        anim_active[i] = true;
    }
}

void animator_set_component(face_component_t comp,
                            const void *target_params,
                            uint32_t duration_ms,
                            uint32_t delay_ms,
                            anim_path_type_t path_type) {
    if (comp >= COMPONENT_COUNT) return;
    int i = comp;

    if (anim_active[i]) {
        lv_anim_del(get_component_ptr(&current_state, comp), anim_exec_cb);
        anim_active[i] = false;
    }

    size_t sz = param_sizes[i];
    void *src = get_component_ptr(&current_state, comp);
    memcpy(from_states[i], src, sz);
    memcpy(anim_data[i].target, target_params, sz);

    lv_anim_init(&anims[i]);
    lv_anim_set_var(&anims[i], src);
    lv_anim_set_values(&anims[i], 0, 1024);
    lv_anim_set_time(&anims[i], duration_ms);
    lv_anim_set_delay(&anims[i], delay_ms);
    lv_anim_set_path_cb(&anims[i], path_map(path_type));
    lv_anim_set_exec_cb(&anims[i], anim_exec_cb);
    lv_anim_set_ready_cb(&anims[i], anim_ready_cb);
    lv_anim_start(&anims[i]);
    anim_active[i] = true;
}

void animator_set_component_instant(face_component_t comp, const void *target_params) {
    if (comp >= COMPONENT_COUNT) return;
    int i = comp;
    if (anim_active[i]) {
        lv_anim_del(get_component_ptr(&current_state, comp), anim_exec_cb);
        anim_active[i] = false;
    }
    void *dst = get_component_ptr(&current_state, comp);
    memcpy(dst, target_params, param_sizes[i]);
}

const face_state_t *animator_get_state(void) {
    return &current_state;
}

void animator_cancel_all(void) {
    for (int i = 0; i < COMPONENT_COUNT; i++) {
        if (anim_active[i]) {
            lv_anim_del(get_component_ptr(&current_state, (face_component_t)i), anim_exec_cb);
            anim_active[i] = false;
        }
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add components/face_system/face_animator.h components/face_system/face_animator.c
git commit -m "feat: add face_animator - LVGL per-component animation driver
Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 6: face_renderer.h/c — Renderer

**Files:**
- Create: `components/face_system/face_renderer.h`
- Create: `components/face_system/face_renderer.c`

- [ ] **Step 1: Write face_renderer.h**

Write `components/face_system/face_renderer.h`:
```c
#ifndef FACE_RENDERER_H
#define FACE_RENDERER_H

#include "face_model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Callback for external effects overlay (same signature as eyes_post_line_cb)
typedef void (*face_post_line_cb_t)(int y, uint16_t *line_buf, int width);
extern face_post_line_cb_t face_post_line_cb;

void renderer_init(void);
void renderer_set_sprite(const sprite_set_t *sprite);
const sprite_set_t *renderer_get_sprite(void);
void renderer_render_frame(const face_state_t *st);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 2: Write face_renderer.c**

Write `components/face_system/face_renderer.c`:
```c
#include "face_renderer.h"
#include "face_animator.h"  // for animator_get_state convenience
#include "gc9a01.h"

#define SCREEN_W 240
#define SCREEN_H 240

static uint16_t line_buf[SCREEN_W];
static const sprite_set_t *current_sprite = NULL;
face_post_line_cb_t face_post_line_cb = NULL;

void renderer_init(void) {
    current_sprite = NULL;
}

void renderer_set_sprite(const sprite_set_t *sprite) {
    current_sprite = sprite;
}

const sprite_set_t *renderer_get_sprite(void) {
    return current_sprite;
}

void renderer_render_frame(const face_state_t *st) {
    if (!current_sprite) return;
    const sprite_set_t *sp = current_sprite;

    gc9a01_set_window(0, 0, SCREEN_W - 1, SCREEN_H - 1);

    for (int y = 0; y < SCREEN_H; y++) {
        // z-order compositing (8 passes)
        // Pass 1: face/background
        sp->draw_face(y, st, sp, line_buf);

        // Pass 2: blush (under eyes)
        sp->draw_blush(y, st, sp, line_buf);

        // Pass 3: mouth
        sp->draw_mouth(y, st, sp, line_buf);

        // Pass 4 & 5: eyes
        sp->draw_eye_left(y, st, sp, line_buf);
        sp->draw_eye_right(y, st, sp, line_buf);

        // Pass 6 & 7: brows (over eyes)
        sp->draw_brow_left(y, st, sp, line_buf);
        sp->draw_brow_right(y, st, sp, line_buf);

        // Pass 8: decor overlay (tears, stars, etc.)
        sp->draw_decor_overlay(y, st, sp, line_buf);

        // External effects hook
        if (face_post_line_cb) {
            face_post_line_cb(y, line_buf, SCREEN_W);
        }

        gc9a01_send_pixels(line_buf, SCREEN_W);
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add components/face_system/face_renderer.h components/face_system/face_renderer.c
git commit -m "feat: add face_renderer - scanline z-order compositor
Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 7: face_api.h — Unified public header

**Files:**
- Create: `components/face_system/face_api.h`

- [ ] **Step 1: Write face_api.h (aggregates all subsystem headers)**

Write `components/face_system/face_api.h`:
```c
#ifndef FACE_API_H
#define FACE_API_H

#include "face_model.h"
#include "face_animator.h"
#include "face_renderer.h"
#include "sprite_classic.h"

#ifdef __cplusplus
extern "C" {
#endif

// One-stop initialization: animator + renderer + default sprite
static inline void face_init(void) {
    animator_init();
    renderer_init();
    renderer_set_sprite(&SPRITE_CLASSIC);
}

// High-level: switch expression
static inline void face_set_expression(expression_id_t id) {
    animator_set_expression(id);
}

// Low-level: single component
static inline void face_set_component(face_component_t comp,
                                      const void *target_params,
                                      uint32_t duration_ms,
                                      uint32_t delay_ms,
                                      anim_path_type_t path_type) {
    animator_set_component(comp, target_params, duration_ms, delay_ms, path_type);
}

static inline void face_set_component_instant(face_component_t comp,
                                              const void *target_params) {
    animator_set_component_instant(comp, target_params);
}

// Sprite switching
static inline void face_set_sprite(const sprite_set_t *sprite) {
    renderer_set_sprite(sprite);
}

// Render one frame (reads animator state, draws via current sprite)
static inline void face_render_frame(void) {
    renderer_render_frame(animator_get_state());
}

// LVGL tick
static inline void face_animator_tick(void) {
    animator_tick();
}

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 4: Commit**

```bash
git add components/face_system/face_api.h
git commit -m "feat: add face_api.h - unified header with one-shot init and render
Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 8: First build verification

**Files:** (all created above)

- [ ] **Step 1: Build**

Run:
```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py build
```

Expected: Compiles successfully. If LVGL header issues: check `managed_components/lvgl__lvgl/` exists (from Task 1).

If component registration errors: verify `main/CMakeLists.txt` has `face_system` in REQUIRES.

- [ ] **Step 2: Commit** (if any fixes applied)

```bash
git add -A && git commit -m "fix: compile fixes for face_system component
Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 9: Integration — main.c

**Files:**
- Modify: `main/main.c`

- [ ] **Step 1: Update main.c imports and init flow**

Read `main/main.c`, apply the following edits:

Add new includes at top (after existing includes):
```c
#include "face_api.h"
#include "lvgl.h"
#include "esp_timer.h"
```

Add LVGL tick timer callback (above `app_main`):
```c
static void lvgl_tick_cb(void *arg) {
    lv_tick_inc(1);  // 1ms tick for LVGL
}
```

Update `display_task` to use face_api:
```c
static void display_task(void *arg) {
    ESP_LOGI(TAG, "Display task started");
    face_set_expression(0);  // NEUTRAL

    while (1) {
        face_animator_tick();
        face_render_frame();
        effects_update(1.0f / 60.0f);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
```

Update `app_main`:
```c
void app_main(void) {
    ESP_LOGI(TAG, "Harti starting...");

    // LVGL init (animation only, no display driver)
    lv_init();

    // 1ms tick timer for LVGL animation heartbeat
    esp_timer_handle_t lvgl_timer;
    esp_timer_create_args_t lvgl_timer_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    esp_timer_create(&lvgl_timer_args, &lvgl_timer);
    esp_timer_start_periodic(lvgl_timer, 1000);  // 1000 us = 1 ms

    // Hardware init
    gc9a01_init();
    face_init();  // was display_init()

    // Register effects callback (compatible with existing hook signature)
    face_post_line_cb = effects_apply_line;

    // Start display task
    xTaskCreate(display_task, "display", DISPLAY_TASK_STACK,
                NULL, DISPLAY_TASK_PRIO, NULL);

    // Sensors / behavior / BLE
    QueueHandle_t sensor_queue = sensors_start();
    ble_start();
    behavior_start(sensor_queue);

    ESP_LOGI(TAG, "All tasks started");
}
```

- [ ] **Step 2: Build + flash verify**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py build
```

- [ ] **Step 3: Commit**

```bash
git add main/main.c
git commit -m "feat: integrate face_system into main.c with LVGL tick timer
Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 10: Integration — app_display.c (compatibility bridge)

**Files:**
- Modify: `main/app_display.c`
- Modify: `main/app_display.h`

- [ ] **Step 1: Simplify app_display.h**

Keep the `emotion_t` enum for backward compatibility, remove internal-only declarations.

Write `main/app_display.h`:
```c
#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EMOTION_NEUTRAL = 0,
    EMOTION_HAPPY,
    EMOTION_SAD,
    EMOTION_SURPRISED,
    EMOTION_SLEEPY,
    EMOTION_ANGRY,
    EMOTION_BORED,
    EMOTION_EXCITED,
    EMOTION_CONFUSED,
    EMOTION_CONTENT,
    EMOTION_COLD,
    EMOTION_WARM,
    EMOTION_HEART_EYES,
    EMOTION_COUNT
} emotion_t;

void display_init(void);
void display_set_emotion(emotion_t emotion);
void display_update(void);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 2: Rewrite app_display.c as thin wrapper**

Write `main/app_display.c`:
```c
#include "app_display.h"
#include "face_api.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "display";

// Micro-animation state
static float breath_phase = 0;
static int blink_state = 0;       // 0: waiting, 1: closing, 2: opening
static float blink_t = 0;
static int frames_until_next_blink = 180;

static float ease_in_out(float t) {
    return t < 0.5f ? 2 * t * t : 1 - powf(-2 * t + 2, 2) / 2;
}

void display_init(void) {
    face_init();
    ESP_LOGI(TAG, "Display initialized (face_system)");
}

void display_set_emotion(emotion_t emotion) {
    if (emotion >= EMOTION_COUNT) return;
    face_set_expression((expression_id_t)emotion);
}

void display_update(void) {
    // 1. Blink logic
    if (blink_state == 0) {
        frames_until_next_blink--;
        if (frames_until_next_blink <= 0) {
            blink_state = 1;
            blink_t = 0;
        }
    } else if (blink_state == 1) {
        blink_t += 0.25f;
        if (blink_t >= 1.0f) {
            blink_t = 1.0f;
            blink_state = 2;
        }
    } else {
        blink_t -= 0.15f;
        if (blink_t <= 0) {
            blink_t = 0;
            blink_state = 0;
            frames_until_next_blink = 120 + (esp_random() % 240);
        }
    }

    // 2. Breath micro-animation (face roundness oscillation)
    breath_phase += 0.03f;
    float breath = sinf(breath_phase) * 0.03f + 1.0f;
    face_params_t fp = {.roundness = 0.5f * breath};
    face_set_component_instant(COMPONENT_FACE, &fp);

    // 3. Blink: close lids by adjusting eye top_lid_mid.dy
    if (blink_state != 0) {
        eye_params_t ep;
        const face_state_t *st = animator_get_state();
        float t = ease_in_out(blink_t);  // normalize to 0-1 for closing

        // Left eye
        ep = st->eye[0];
        ep.top_lid_mid.dy = blink_t * 0.7f;  // push lid down
        face_set_component_instant(COMPONENT_EYE_LEFT, &ep);

        // Right eye
        ep = st->eye[1];
        ep.top_lid_mid.dy = blink_t * 0.7f;
        face_set_component_instant(COMPONENT_EYE_RIGHT, &ep);
    }

    // 4. Micro-saccades (small iris movements)
    // Handled by face_animator_set_component with short duration in a future enhancement,
    // or applied as instant micro-adjustments here
    static float sac_phase = 0;
    sac_phase += 0.08f;
    float micro_x = sinf(sac_phase * 1.3f) * 0.05f;
    float micro_y = cosf(sac_phase * 0.9f) * 0.05f;

    const face_state_t *st = animator_get_state();
    eye_params_t ep_l = st->eye[0];
    eye_params_t ep_r = st->eye[1];
    ep_l.iris_center.dx += micro_x;
    ep_l.iris_center.dy += micro_y;
    ep_r.iris_center.dx += micro_x;
    ep_r.iris_center.dy += micro_y;
    face_set_component_instant(COMPONENT_EYE_LEFT, &ep_l);
    face_set_component_instant(COMPONENT_EYE_RIGHT, &ep_r);

    // 5. LVGL tick + render
    face_animator_tick();
    face_render_frame();
}
```

Note: `animator_get_state()` needs to be declared in `face_animator.h` — already done in Task 5.

- [ ] **Step 3: Build + flash verify**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py build
```

- [ ] **Step 4: Commit**

```bash
git add main/app_display.c main/app_display.h
git commit -m "feat: port app_display to face_system, keep emotion_t compatibility
Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

### Task 11: Final integration — app_effects compatibility

**Files:**
- Verify: `main/app_effects.c` (no changes expected)

- [ ] **Step 1: Verify app_effects.c compatibility**

The existing `effects_apply_line` function has the same signature as `face_post_line_cb_t`:
```c
void effects_apply_line(int y, uint16_t *line_buf, int width);
```

In `main.c` we assign: `face_post_line_cb = effects_apply_line;`

This should work without changes to `app_effects.c`.

- [ ] **Step 2: Full build**

```bash
. ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py build
```

Expected: Clean compile, no warnings.

- [ ] **Step 3: Flash and verify (if hardware available)**

```bash
idf.py -p /dev/tty.usbmodem* flash monitor
```

Expected: Screen shows NEUTRAL expression, eyes render correctly. Behavior module triggers emotion changes normally.

- [ ] **Step 4: Commit**

```bash
git add -A && git diff --cached
git commit -m "feat: complete face_system integration - all modules migrated
Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>"
```

---

## Plan Self-Review

### Spec Coverage

| Spec Section | Task(s) |
|---|---|
| Face Model (types, keypoints, state, expression, sprite) | T2, T3 |
| Face Animator (LVGL init, per-component anim, interrupt) | T5 |
| Face Renderer (scanline, z-order, post_line hook) | T6 |
| Face API (two-layer, high + low) | T7 |
| Sprite Sets (function table, geometry, palette) | T3 (metadata), T4 (renderer) |
| Classic Sprite (migrated from expressive_eyes) | T4 |
| LVGL dependency | T1 |
| main.c integration | T9 |
| app_display migration | T10 |
| CMakeLists | T1, T8 |
| File structure | T1 |

### Consistency Check

- `face_state_t` defined in T2, consumed by T3-T7, T9-T10. Same struct everywhere.
- `sprite_set_t` function signatures match between T2 (typedef), T4 (implementations), T6 (calls).
- `animator_get_state()` declared in T5 header, used in T10.
- `face_post_line_cb` declared in T6 header, assigned in T9.
- `emotion_t` enum values 0-12 map directly to `expression_id_t` (0-12), which maps to `EXPRESSION_DEFS[]` indices.
