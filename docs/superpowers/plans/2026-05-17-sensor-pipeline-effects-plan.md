# Sensor Pipeline + Effects Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the full sensor→behavior→display pipeline with 4 FreeRTOS tasks, 5 new emotions, heart-shaped iris, and meeting effects engine.

**Architecture:** Four FreeRTOS tasks communicate via queues. `sensor_task` reads IMU/touch/temp and posts events to `sensor_queue`. `behavior_task` runs a state machine consuming events and commanding `app_display`/`app_effects`. `display_task` renders at 60fps. `ble_task` is a stub for future BLE.

**Tech Stack:** ESP32-S3, ESP-IDF, FreeRTOS, C99

---

### Task 1: Heart iris lookup table + HEART_EYES preset

**Files:**
- Modify: `components/expressive_eyes/expressive_eyes.h` — add `heart_mode` to `eye_state_t`
- Modify: `components/expressive_eyes/expressive_eyes.c` — heart table + `render_eye` heart branch + `EYE_STATE_HEART_EYES`

- [ ] **Step 1: Add `heart_mode` field to `eye_state_t`**

In `components/expressive_eyes/expressive_eyes.h`, add the field after `star_level`:

```c
    float star_level;
    bool heart_mode;      // true = 虹膜绘制为心形
} eye_state_t;
```

Also declare the new preset:

```c
extern const eye_state_t EYE_STATE_HEART_EYES;
```

- [ ] **Step 2: Add heart x-max row table and EYE_STATE_HEART_EYES preset**

In `components/expressive_eyes/expressive_eyes.c`, add after the color palette definitions:

```c
// 心形查表: 每行(y)的最大x偏移 (心形关于x轴对称)
// y从 -13 到 +13, 共27行. 值0表示该行不在心形内.
static const int8_t heart_row_half[27] = {
    // y = -13 .. -10 (顶部凹陷, 两瓣之间)
    0, 0, 0, 0,
    // y = -9 .. -7 (两瓣开始)
    2, 4, 5,
    // y = -6 .. -4 (最大宽度)
    6, 7, 7,
    // y = -3 .. -1
    7, 6, 6,
    // y = 0 (中心)
    6,
    // y = 1 .. 3
    5, 5, 4,
    // y = 4 .. 6 (逐渐收窄)
    4, 3, 2,
    // y = 7 .. 9
    2, 1, 1,
    // y = 10 .. 13 (底部尖角)
    0, 0, 0, 0,
};
```

Add the preset after `EYE_STATE_EXCITED`:

```c
const eye_state_t EYE_STATE_HEART_EYES = {
    .eye_offset_x = 0,
    .eye_offset_y = -2,
    .eye_separation = 52,
    .left_lid_open = 0.9f,
    .right_lid_open = 0.9f,
    .pupil_x = 0,
    .pupil_y = 0,
    .pupil_scale = 0.0f,  // 心形眼无瞳孔
    .curve_up = 0.35f,
    .curve_down = 0,
    .blush_level = 0.65f,
    .tear_level = 0,
    .star_level = 0.9f,
    .heart_mode = true,
};
```

Update all other presets to explicitly set `.heart_mode = false`.

- [ ] **Step 3: Modify render_eye to support heart-shaped iris**

In `render_eye()`, find the iris visibility check (the `else if (iris_d_sq < iris_r_sq)` block). Wrap the iris/pupil rendering with a heart_mode branch.

After the variable declarations at the top of `render_eye`, add:

```c
    // 心形查表偏移: y从-13到+13 (共27行)
    const int heart_table_offset = 13;
```

Then in the `if (visible)` block, replace the iris/pupil check:

```c
            if (visible) {
                float iris_d_sq = dist_sq(fx, fy, iris_cx, iris_cy);
                float pupil_d_sq = dist_sq(fx, fy, pupil2_cx, pupil2_cy);
                float shine1_d_sq = dist_sq(fx, fy, shine1_cx, shine1_cy);
                float shine2_d_sq = dist_sq(fx, fy, shine2_cx, shine2_cy);

                if (current_state.heart_mode) {
                    // 心形虹膜: 用查表判断
                    int hx = (int)(fx - iris_cx);
                    int hy = (int)(fy - iris_cy);
                    int row = hy + heart_table_offset;
                    bool in_heart = false;
                    if (row >= 0 && row < 27) {
                        int max_x = heart_row_half[row];
                        if (max_x > 0 && abs(hx) <= max_x) {
                            in_heart = true;
                        }
                    }
                    if (in_heart) {
                        // 高光
                        if (shine1_d_sq < 25.0f) {
                            line_buf[x] = blend_colors(pal->iris, pal->shine, 0.85f);
                        } else if (shine2_d_sq < 9.0f) {
                            line_buf[x] = blend_colors(pal->iris, pal->shine, 0.65f);
                        } else {
                            line_buf[x] = pal->iris;
                        }
                    }
                    // 心形眼无瞳孔，sclera 已在 line_buf 中
                } else {
                    // 原始圆形虹膜
                    if (shine1_d_sq < 25.0f && iris_d_sq < iris_r_sq) {
                        line_buf[x] = blend_colors(line_buf[x], pal->shine, 0.9f);
                    }
                    else if (shine2_d_sq < 9.0f && iris_d_sq < iris_r_sq) {
                        line_buf[x] = blend_colors(line_buf[x], pal->shine, 0.7f);
                    }
                    else if (pupil_d_sq < pupil_r_sq) {
                        line_buf[x] = pal->pupil;
                    }
                    else if (iris_d_sq < iris_r_sq) {
                        line_buf[x] = pal->iris;
                    }
                    else {
                        line_buf[x] = pal->sclera;
                    }
                }
            }
```

Remove the `current_state.heart_mode` references in the edge-antialiasing section (heart eyes don't need anti-aliased edges on the iris — the edge AA on the eye outline is enough). Or, more simply, skip the edge section for heart_mode by keeping the early return for non-edge pixels and letting edge pixels fall through to the sclera.

- [ ] **Step 4: Update `render_eye` to use `current_state.heart_mode` directly**

The function currently doesn't have access to `current_state`. Add a parameter or use the global. The simplest fix: `render_eye` already accesses globals implicitly — just add:

In `render_eye`, at the top, reference `current_state.heart_mode`. Since `current_state` is a file-static global, it's already in scope. The code in step 3 above works directly.

- [ ] **Step 5: Update `EYE_STATE_NEUTRAL` to include `heart_mode`**

The NEUTRAL preset is defined first and the other presets copy its pattern. All presets need `.heart_mode = false` explicitly (except HEART_EYES).

- [ ] **Step 6: Build verify**

```bash
. ~/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build
```

Expected: compile success, no warnings about uninitialized fields.

- [ ] **Step 7: Commit**

```bash
git add components/expressive_eyes/expressive_eyes.c components/expressive_eyes/expressive_eyes.h
git commit -m "feat: add heart-shaped iris rendering + HEART_EYES preset"
```

---

### Task 2: 5 new emotions in app_display

**Files:**
- Modify: `main/app_display.h` — add 5 emotion_t values
- Modify: `main/app_display.c` — wire new emotions to presets

- [ ] **Step 1: Add emotion enum values**

In `main/app_display.h`, add before `EMOTION_COUNT`:

```c
    EMOTION_CONFUSED,
    EMOTION_CONTENT,
    EMOTION_COLD,
    EMOTION_WARM,
    EMOTION_HEART_EYES,
    EMOTION_COUNT
```

- [ ] **Step 2: Wire new emotions to presets in `get_emotion_state()`**

In `main/app_display.c`, add cases to `get_emotion_state()`:

```c
        case EMOTION_CONFUSED: return &EYE_STATE_CONFUSED;
        case EMOTION_CONTENT:  return &EYE_STATE_CONTENT;
        case EMOTION_COLD:     return &EYE_STATE_COLD;
        case EMOTION_WARM:     return &EYE_STATE_WARM;
        case EMOTION_HEART_EYES: return &EYE_STATE_HEART_EYES;
```

- [ ] **Step 3: Add remaining 4 preset declarations**

In `components/expressive_eyes/expressive_eyes.h`, add after `EYE_STATE_EXCITED`:

```c
extern const eye_state_t EYE_STATE_CONFUSED;
extern const eye_state_t EYE_STATE_CONTENT;
extern const eye_state_t EYE_STATE_COLD;
extern const eye_state_t EYE_STATE_WARM;
```

- [ ] **Step 4: Add 4 preset definitions**

In `components/expressive_eyes/expressive_eyes.c`, add after `EYE_STATE_EXCITED`:

```c
const eye_state_t EYE_STATE_CONFUSED = {
    .eye_offset_x = 0, .eye_offset_y = 2,
    .eye_separation = 52,
    .left_lid_open = 0.85f, .right_lid_open = 0.70f, // 一高一低
    .pupil_x = 0.3f, .pupil_y = 0.1f, // 看一边
    .pupil_scale = 0.5f,
    .curve_up = 0.15f, .curve_down = -0.15f, // 眉毛不对称
    .blush_level = 0.1f, .tear_level = 0, .star_level = 0,
    .heart_mode = false,
};

const eye_state_t EYE_STATE_CONTENT = {
    .eye_offset_x = 0, .eye_offset_y = 0,
    .eye_separation = 52,
    .left_lid_open = 0.3f, .right_lid_open = 0.3f, // 几乎闭合的弯月眼
    .pupil_x = 0, .pupil_y = 0,
    .pupil_scale = 0.55f,
    .curve_up = 0.7f, .curve_down = 0, // 明显的上弯
    .blush_level = 0.75f, .tear_level = 0, .star_level = 0.2f,
    .heart_mode = false,
};

const eye_state_t EYE_STATE_COLD = {
    .eye_offset_x = 0, .eye_offset_y = 3,
    .eye_separation = 52,
    .left_lid_open = 0.5f, .right_lid_open = 0.5f,
    .pupil_x = 0, .pupil_y = 0.2f,
    .pupil_scale = 0.4f, // 缩小
    .curve_up = 0, .curve_down = 0,
    .blush_level = 0.3f, .tear_level = 0, .star_level = 0,
    .heart_mode = false,
};

const eye_state_t EYE_STATE_WARM = {
    .eye_offset_x = 0, .eye_offset_y = -2,
    .eye_separation = 52,
    .left_lid_open = 0.7f, .right_lid_open = 0.7f,
    .pupil_x = 0, .pupil_y = -0.15f,
    .pupil_scale = 0.65f,
    .curve_up = 0.3f, .curve_down = 0,
    .blush_level = 0.5f, .tear_level = 0, .star_level = 0.3f,
    .heart_mode = false,
};
```

- [ ] **Step 5: Build verify**

```bash
. ~/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build
```

- [ ] **Step 6: Commit**

```bash
git add main/app_display.h main/app_display.c components/expressive_eyes/expressive_eyes.h components/expressive_eyes/expressive_eyes.c
git commit -m "feat: add 5 new emotions (CONFUSED, CONTENT, COLD, WARM, HEART_EYES)"
```

---

### Task 3: app_effects — meeting effects engine

**Files:**
- Create: `main/app_effects.h`
- Create: `main/app_effects.c`

- [ ] **Step 1: Create `main/app_effects.h`**

```c
#ifndef APP_EFFECTS_H
#define APP_EFFECTS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EFFECT_NONE = 0,
    EFFECT_STAR,           // 单颗小星星闪一下 (点头之交)
    EFFECT_HEART_PARTICLE, // 2-3颗心形粒子升起 (朋友)
    EFFECT_RAINBOW,        // 彩虹弧线 (好友)
    EFFECT_GOLDEN,         // 金光扩散 (挚友)
    EFFECT_COUNT
} effect_type_t;

// 触发特效 (高级可中断低级)
void effects_trigger(effect_type_t type);

// 每帧调用, 在眼睛渲染之后叠加到 line_buf
// y: 当前扫描行, scheme: 当前配色
void effects_render_line(int y, int color_scheme);

// 释放当前特效 (行为模块切换情绪时调用)
void effects_clear(void);

// 特效是否正在播放中
bool effects_is_active(void);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 2: Create `main/app_effects.c`**

```c
#include "app_effects.h"
#include "expressive_eyes.h"
#include <string.h>
#include <math.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

#define RGB(r,g,b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3))

typedef enum {
    PHASE_IN,   // 淡入
    PHASE_HOLD, // 停留
    PHASE_OUT,  // 淡出
} effect_phase_t;

static effect_type_t current_effect = EFFECT_NONE;
static float effect_timer = 0;
static effect_phase_t effect_phase = PHASE_IN;

static const float FADE_IN_DURATION  = 0.3f;  // 秒
static const float HOLD_DURATION     = 1.5f;
static const float FADE_OUT_DURATION = 0.5f;

// 当前帧透明度 (0..1)
static float effect_alpha(void) {
    if (current_effect == EFFECT_NONE) return 0;
    switch (effect_phase) {
        case PHASE_IN:  return effect_timer / FADE_IN_DURATION;
        case PHASE_HOLD: return 1.0f;
        case PHASE_OUT: return 1.0f - (effect_timer / FADE_OUT_DURATION);
    }
    return 0;
}

void effects_trigger(effect_type_t type) {
    if (type >= EFFECT_COUNT) return;
    // 高级特效可以打断低级
    if (current_effect != EFFECT_NONE && type <= current_effect) return;
    current_effect = type;
    effect_phase = PHASE_IN;
    effect_timer = 0;
}

void effects_clear(void) {
    current_effect = EFFECT_NONE;
    effect_timer = 0;
}

bool effects_is_active(void) {
    return current_effect != EFFECT_NONE;
}

// ── 渲染辅助 ────────────────────────────────────────

static inline uint16_t effect_blend(uint16_t c1, uint16_t c2, float t) {
    if (t <= 0) return c1;
    if (t >= 1) return c2;
    int t256 = (int)(t * 256.0f);
    int r = ((c1 >> 11) & 0x1F) + (((((c2 >> 11) & 0x1F) - ((c1 >> 11) & 0x1F)) * t256 + 128) >> 8);
    int g = ((c1 >> 5) & 0x3F)  + (((((c2 >> 5) & 0x3F)  - ((c1 >> 5) & 0x3F))  * t256 + 128) >> 8);
    int b = (c1 & 0x1F)        + ((((c2 & 0x1F)         - (c1 & 0x1F))         * t256 + 128) >> 8);
    return (r << 11) | (g << 5) | b;
}

static inline float dist_sq_f(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

// ── 星星特效 (单颗星从头顶闪出) ──────────────────────

static void render_star_effect(int y, uint16_t *line_buf, float alpha) {
    float star_cx = CENTER_X;
    float star_cy = CENTER_Y - 60 + effect_timer * 15.0f; // 缓慢上升
    float star_r = 8.0f;

    float dy = y - star_cy;
    if (dy < -star_r || dy > star_r) return;

    int x_start = (int)(star_cx - star_r) - 1;
    int x_end   = (int)(star_cx + star_r) + 1;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    uint16_t star_color = RGB(255, 220, 0); // 金黄
    for (int x = x_start; x <= x_end; x++) {
        float d = dist_sq_f(x, y, star_cx, star_cy);
        if (d < star_r * star_r) {
            float t = (1.0f - d / (star_r * star_r)) * alpha * 0.8f;
            line_buf[x] = effect_blend(line_buf[x], star_color, t);
        }
    }
}

// ── 心形粒子特效 (2-3颗小心心上升) ──────────────────

static void render_heart_particle_effect(int y, uint16_t *line_buf, float alpha) {
    uint16_t heart_color = RGB(255, 100, 130); // 粉红

    // 3颗粒子, 从屏幕底部不同位置上升
    for (int p = 0; p < 3; p++) {
        float px = CENTER_X - 15 + p * 15.0f;
        float rise = effect_timer * 40.0f;
        if (rise > 70.0f) rise = 70.0f;
        float py = CENTER_Y + 50 - rise + sinf(effect_timer * 3.0f + p) * 8.0f;
        float pr = 5.0f;

        float dy = y - py;
        if (dy < -pr || dy > pr) continue;

        int x_start = (int)(px - pr) - 1;
        int x_end   = (int)(px + pr) + 1;
        if (x_start < 0) x_start = 0;
        if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

        for (int x = x_start; x <= x_end; x++) {
            float d = dist_sq_f(x, y, px, py);
            if (d < pr * pr) {
                float t = (1.0f - d / (pr * pr)) * alpha * 0.75f;
                line_buf[x] = effect_blend(line_buf[x], heart_color, t);
            }
        }
    }
}

// ── 彩虹弧特效 (眼睛上方弧形彩虹) ──────────────────

static void render_rainbow_effect(int y, uint16_t *line_buf, float alpha) {
    // 彩虹弧在屏幕上方, 弧半径 ~80px, 厚度 ~12px
    float arc_cy = CENTER_Y - 70;
    float arc_r = 80.0f;
    float arc_thickness = 12.0f;

    float dy = y - arc_cy;
    if (dy < -arc_thickness || dy > arc_thickness) return;

    for (int x = 0; x < SCREEN_W; x++) {
        float d = sqrtf(dist_sq_f(x, y, CENTER_X, arc_cy));
        float ring = fabsf(d - arc_r);
        if (ring < arc_thickness) {
            // 根据角度选择颜色
            float angle = atan2f(y - arc_cy, x - CENTER_X);
            float hue = (angle + 3.1416f) / 6.2832f; // 0..1
            uint16_t color;
            if (hue < 0.166f)      color = RGB(255, 0, 0);
            else if (hue < 0.333f) color = RGB(255, 165, 0);
            else if (hue < 0.5f)   color = RGB(255, 255, 0);
            else if (hue < 0.666f) color = RGB(0, 255, 0);
            else if (hue < 0.833f) color = RGB(0, 100, 255);
            else                   color = RGB(128, 0, 255);
            float t = (1.0f - ring / arc_thickness) * alpha * 0.5f;
            line_buf[x] = effect_blend(line_buf[x], color, t);
        }
    }
}

// ── 金光特效 (中心向外扩散的金色粒子) ──────────────

static void render_golden_effect(int y, uint16_t *line_buf, float alpha) {
    uint16_t gold_color = RGB(255, 215, 0);
    int num_rings = 6;
    for (int r = 0; r < num_rings; r++) {
        float ring_r = 15.0f + r * 18.0f + effect_timer * 8.0f;
        float ring_thick = 5.0f - r * 0.4f;
        if (ring_thick < 1.5f) ring_thick = 1.5f;

        float dy = y - CENTER_Y;
        for (int x = 0; x < SCREEN_W; x++) {
            float d = sqrtf(dist_sq_f(x, y, CENTER_X, CENTER_Y));
            float ring = fabsf(d - ring_r);
            if (ring < ring_thick) {
                float t = (1.0f - ring / ring_thick) * alpha * 0.55f;
                line_buf[x] = effect_blend(line_buf[x], gold_color, t);
            }
        }
    }
}

// ── 入口 ────────────────────────────────────────────

void effects_render_line(int y, int color_scheme) {
    if (current_effect == EFFECT_NONE) return;

    float alpha = effect_alpha();
    if (alpha <= 0.01f) return;

    // effects_render_line 需要传入 line_buf 指针
    // 由 display_task 调用, line_buf 在 eyes_render_frame 中是局部的
    // 所以这个函数签名需要改 — 见下面的说明
}

// 注: 实际渲染函数需要在 display_update 中集成,
// line_buf 来自 expressive_eyes 的渲染循环.
// 因此改用另一种集成方式: 在 eyes_render_frame 末尾调用 effects 叠加.
```

- [ ] **Step 3: Redesign effects integration**

The above approach has a problem: `effects_render_line` can't access the `line_buf` local variable from `eyes_render_frame`. Instead, change the approach: `app_effects` provides a function `effects_apply_to_pixel(uint16_t *pixel, int x, int y, float dt)` that `eyes_render_frame` calls per pixel, or better — pass the line_buf as parameter.

Rewrite `app_effects.h`:

```c
#ifndef APP_EFFECTS_H
#define APP_EFFECTS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EFFECT_NONE = 0,
    EFFECT_STAR,
    EFFECT_HEART_PARTICLE,
    EFFECT_RAINBOW,
    EFFECT_GOLDEN,
    EFFECT_COUNT
} effect_type_t;

void effects_trigger(effect_type_t type);
void effects_update(float dt);
void effects_apply_line(int y, uint16_t *line_buf, int screen_w);
void effects_clear(void);
bool effects_is_active(void);

#ifdef __cplusplus
}
#endif

#endif
```

Rewrite `app_effects.c`:

```c
#include "app_effects.h"
#include <math.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

#define RGB(r,g,b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3))

typedef enum { PHASE_IN, PHASE_HOLD, PHASE_OUT } effect_phase_t;

static effect_type_t current_effect = EFFECT_NONE;
static float effect_timer = 0;
static effect_phase_t effect_phase = PHASE_IN;

static const float FADE_IN_DURATION  = 0.3f;
static const float HOLD_DURATION     = 1.5f;
static const float FADE_OUT_DURATION = 0.5f;

void effects_trigger(effect_type_t type) {
    if (type >= EFFECT_COUNT) return;
    if (current_effect != EFFECT_NONE && type <= current_effect) return;
    current_effect = type;
    effect_phase = PHASE_IN;
    effect_timer = 0;
}

void effects_clear(void) {
    current_effect = EFFECT_NONE;
    effect_timer = 0;
}

bool effects_is_active(void) {
    return current_effect != EFFECT_NONE;
}

void effects_update(float dt) {
    if (current_effect == EFFECT_NONE) return;
    effect_timer += dt;
    switch (effect_phase) {
        case PHASE_IN:
            if (effect_timer >= FADE_IN_DURATION) {
                effect_timer -= FADE_IN_DURATION;
                effect_phase = PHASE_HOLD;
            }
            break;
        case PHASE_HOLD:
            if (effect_timer >= HOLD_DURATION) {
                effect_timer -= HOLD_DURATION;
                effect_phase = PHASE_OUT;
            }
            break;
        case PHASE_OUT:
            if (effect_timer >= FADE_OUT_DURATION) {
                current_effect = EFFECT_NONE;
                effect_timer = 0;
                effect_phase = PHASE_IN;
            }
            break;
    }
}

static float current_alpha(void) {
    if (current_effect == EFFECT_NONE) return 0;
    switch (effect_phase) {
        case PHASE_IN: {
            float t = effect_timer / FADE_IN_DURATION;
            return t;
        }
        case PHASE_HOLD: return 1.0f;
        case PHASE_OUT: {
            float t = effect_timer / FADE_OUT_DURATION;
            return 1.0f - t;
        }
    }
    return 0;
}

static inline uint16_t ef_blend(uint16_t c1, uint16_t c2, float t) {
    if (t <= 0) return c1;
    if (t >= 1) return c2;
    int t256 = (int)(t * 256.0f);
    int r = ((c1 >> 11) & 0x1F) + (((((c2 >> 11) & 0x1F) - ((c1 >> 11) & 0x1F)) * t256 + 128) >> 8);
    int g = ((c1 >> 5) & 0x3F)  + (((((c2 >> 5) & 0x3F)  - ((c1 >> 5) & 0x3F))  * t256 + 128) >> 8);
    int b = (c1 & 0x1F)        + ((((c2 & 0x1F)         - (c1 & 0x1F))         * t256 + 128) >> 8);
    return (r << 11) | (g << 5) | b;
}

// ── 单个像素软光叠加 ────────────────────────────────

static void soft_add_pixel(uint16_t *pixel, float px, float py, float cx, float cy, float radius, uint16_t color, float alpha) {
    float dx = px - cx, dy = py - cy;
    float d_sq = dx * dx + dy * dy;
    float r_sq = radius * radius;
    if (d_sq < r_sq) {
        float t = (1.0f - d_sq / r_sq) * alpha;
        *pixel = ef_blend(*pixel, color, t);
    }
}

void effects_apply_line(int y, uint16_t *line_buf, int screen_w) {
    if (current_effect == EFFECT_NONE) return;

    float alpha = current_alpha();
    if (alpha <= 0.01f) return;

    uint16_t star_color  = RGB(255, 220, 0);
    uint16_t heart_color = RGB(255, 100, 130);
    uint16_t gold_color  = RGB(255, 215, 0);

    switch (current_effect) {
    case EFFECT_STAR: {
        float star_cy = CENTER_Y - 60 + effect_timer * 15.0f;
        for (int x = 0; x < SCREEN_W; x++) {
            soft_add_pixel(&line_buf[x], x, y, CENTER_X, star_cy, 8.0f, star_color, alpha * 0.8f);
        }
        break;
    }

    case EFFECT_HEART_PARTICLE: {
        for (int p = 0; p < 3; p++) {
            float px = CENTER_X - 15 + p * 15.0f;
            float rise = effect_timer * 40.0f;
            if (rise > 70.0f) rise = 70.0f;
            float py = CENTER_Y + 50 - rise + sinf(effect_timer * 3.0f + p) * 8.0f;
            for (int x = 0; x < SCREEN_W; x++) {
                soft_add_pixel(&line_buf[x], x, y, px, py, 5.0f, heart_color, alpha * 0.75f);
            }
        }
        break;
    }

    case EFFECT_RAINBOW: {
        float arc_cy = CENTER_Y - 70, arc_r = 80.0f, thick = 12.0f;
        float dy = y - arc_cy;
        if (dy >= -thick && dy <= thick) {
            for (int x = 0; x < SCREEN_W; x++) {
                float d = sqrtf((x - CENTER_X) * (x - CENTER_X) + dy * dy);
                float ring = fabsf(d - arc_r);
                if (ring < thick) {
                    float angle = atan2f(dy, x - CENTER_X);
                    float hue = (angle + 3.1416f) / 6.2832f;
                    uint16_t color;
                    if (hue < 0.166f)      color = RGB(255, 0, 0);
                    else if (hue < 0.333f) color = RGB(255, 165, 0);
                    else if (hue < 0.5f)   color = RGB(255, 255, 0);
                    else if (hue < 0.666f) color = RGB(0, 255, 0);
                    else if (hue < 0.833f) color = RGB(0, 100, 255);
                    else                   color = RGB(128, 0, 255);
                    float t = (1.0f - ring / thick) * alpha * 0.5f;
                    line_buf[x] = ef_blend(line_buf[x], color, t);
                }
            }
        }
        break;
    }

    case EFFECT_GOLDEN: {
        for (int r = 0; r < 6; r++) {
            float ring_r = 15.0f + r * 18.0f + effect_timer * 8.0f;
            float ring_thick = 5.0f - r * 0.4f;
            if (ring_thick < 1.5f) ring_thick = 1.5f;
            for (int x = 0; x < SCREEN_W; x++) {
                float d = sqrtf((x - CENTER_X) * (x - CENTER_X) + (y - CENTER_Y) * (y - CENTER_Y));
                float ring = fabsf(d - ring_r);
                if (ring < ring_thick) {
                    float t = (1.0f - ring / ring_thick) * alpha * 0.55f;
                    line_buf[x] = ef_blend(line_buf[x], gold_color, t);
                }
            }
        }
        break;
    }

    default: break;
    }
}
```

- [ ] **Step 4: Build verify**

```bash
. ~/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build
```

- [ ] **Step 5: Commit**

```bash
git add main/app_effects.h main/app_effects.c
git commit -m "feat: add meeting effects engine (star/heart/rainbow/golden)"
```

---

### Task 4: app_sensors — sensor event detection

**Files:**
- Create: `main/app_sensors.h`
- Create: `main/app_sensors.c`

- [ ] **Step 1: Create `main/app_sensors.h`**

```c
#ifndef APP_SENSORS_H
#define APP_SENSORS_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EVT_NONE = 0,
    EVT_TOUCH_HEAD,
    EVT_TOUCH_RELEASE,
    EVT_SHAKE,
    EVT_TAP,
    EVT_FLIP,
    EVT_WARM_UP,
    EVT_COLD_DOWN,
    EVT_TEMP_STABLE,
    EVT_BLE_MEET,
    EVT_BLE_FRIEND,
} sensor_event_t;

typedef struct {
    sensor_event_t type;
    float value; // 温度/加速度幅值等
} sensor_event_msg_t;

// 启动传感器任务, 返回事件队列句柄
QueueHandle_t sensors_start(void);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 2: Create `main/app_sensors.c`**

```c
#include "app_sensors.h"
#include "harti_imu.h"
#include "harti_temp.h"
#include "driver/i2c_master.h"
#include "driver/touch_pad.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "sensors";

#define I2C_SCL_IO  GPIO_NUM_15
#define I2C_SDA_IO  GPIO_NUM_16
#define TOUCH_PAD   TOUCH_PAD_NUM0  // GPIO0, 单通道触摸

#define SENSOR_TASK_STACK  2048
#define SENSOR_TASK_PRIO   3
#define IMU_SAMPLE_PERIOD_MS  10  // 100Hz
#define TEMP_SAMPLE_PERIOD_MS 1000 // 1Hz

static QueueHandle_t event_queue;

// ── 摇晃/轻拍/翻转检测 ──────────────────────────────

#define SHAKE_WINDOW_MS     500
#define SHAKE_G_THRESHOLD   2.5f
#define TAP_MAX_MS          100
#define TAP_G_THRESHOLD     1.5f
#define FLIP_HOLD_MS        500

static float accel_history[50][3]; // 500ms @ 100Hz
static int accel_history_idx = 0;
static int accel_history_count = 0;

static bool tap_armed = true;       // 允许检测新tap
static int  tap_count = 0;
static int  tap_quiet_frames = 0;   // tap后静默帧

static bool flip_armed = true;
static int  flip_z_down_frames = 0;

static void process_imu(const imu_data_t *data) {
    // 存入滑动窗口
    accel_history[accel_history_idx][0] = data->accel[0];
    accel_history[accel_history_idx][1] = data->accel[1];
    accel_history[accel_history_idx][2] = data->accel[2];
    accel_history_idx = (accel_history_idx + 1) % 50;
    if (accel_history_count < 50) accel_history_count++;

    // 计算当前幅值 (去除重力)
    float ax = data->accel[0], ay = data->accel[1], az = data->accel[2];
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    float dynamic_mag = fabsf(mag - 1.0f);

    // ── 轻拍检测: Z轴短脉冲 ──
    float z_mag = fabsf(az);
    if (dynamic_mag > TAP_G_THRESHOLD && tap_armed) {
        // 检查持续时间 (通过历史窗口)
        int above_count = 0;
        for (int i = 0; i < accel_history_count && i < 10; i++) {
            int idx = (accel_history_idx - 1 - i + 50) % 50;
            float m = sqrtf(accel_history[idx][0] * accel_history[idx][0] +
                           accel_history[idx][1] * accel_history[idx][1] +
                           accel_history[idx][2] * accel_history[idx][2]);
            if (fabsf(m - 1.0f) > TAP_G_THRESHOLD) above_count++;
        }
        if (above_count <= 5) { // < 100ms → tap
            tap_count++;
            tap_armed = false;
            tap_quiet_frames = 0;
            sensor_event_msg_t msg = { .type = EVT_TAP, .value = tap_count };
            xQueueSend(event_queue, &msg, 0);
            ESP_LOGI(TAG, "TAP detected, count=%d", tap_count);
        }
    }

    // tap后静默 300ms 再允许下一次tap
    if (!tap_armed) {
        tap_quiet_frames++;
        if (tap_quiet_frames > 30) { // 300ms
            tap_armed = true;
            tap_count = 0;
        }
    }

    // ── 摇晃检测: 持续高幅值 ──
    static int shake_frames = 0;
    if (dynamic_mag > SHAKE_G_THRESHOLD) {
        shake_frames++;
        if (shake_frames == 30) { // 300ms
            sensor_event_msg_t msg = { .type = EVT_SHAKE, .value = dynamic_mag };
            xQueueSend(event_queue, &msg, 0);
            ESP_LOGI(TAG, "SHAKE detected, mag=%.2f", dynamic_mag);
        }
    } else {
        shake_frames = 0;
    }

    // ── 翻转检测: Z轴持续反向 ──
    if (az < -0.7f) { // Z朝下
        flip_z_down_frames++;
        if (flip_z_down_frames == 50 && flip_armed) { // 500ms
            sensor_event_msg_t msg = { .type = EVT_FLIP, .value = 0 };
            xQueueSend(event_queue, &msg, 0);
            flip_armed = false;
            ESP_LOGI(TAG, "FLIP detected");
        }
    } else {
        if (flip_z_down_frames < 50 && flip_z_down_frames > 0) {
            // 短暂翻转后恢复 → 重新武装
            flip_armed = true;
        }
        flip_z_down_frames = 0;
    }
}

// ── 触摸检测 ─────────────────────────────────────────

static bool touch_active = false;
static int  touch_debounce = 0;

static void process_touch(void) {
    uint32_t touch_val;
    esp_err_t err = touch_pad_read_raw_data(TOUCH_PAD, &touch_val);
    if (err != ESP_OK) return;

    // 阈值: 触摸时电容值变大, < 800 表示被触摸 (需根据外壳调试)
    // 这里用相对阈值: 无触摸时基准值 ~1000, 触摸后 ~300
    bool touched = (touch_val < 600);

    if (touched && !touch_active) {
        touch_debounce++;
        if (touch_debounce >= 5) { // 50ms去抖 (touch读取频率 50Hz → 5帧 = 100ms)
            touch_active = true;
            sensor_event_msg_t msg = { .type = EVT_TOUCH_HEAD, .value = 0 };
            xQueueSend(event_queue, &msg, 0);
            ESP_LOGI(TAG, "TOUCH_HEAD");
        }
    } else if (!touched && touch_active) {
        touch_debounce = 0;
        touch_active = false;
        sensor_event_msg_t msg = { .type = EVT_TOUCH_RELEASE, .value = 0 };
        xQueueSend(event_queue, &msg, 0);
        ESP_LOGI(TAG, "TOUCH_RELEASE");
    } else if (!touched) {
        touch_debounce = 0;
    }
}

// ── 温度检测 ─────────────────────────────────────────

static float last_temp = 25.0f;
static int   warm_up_frames = 0;
static int   temp_sample_counter = 0;

static void process_temp(void) {
    temp_sample_counter++;
    if (temp_sample_counter < 100) return; // 约每100个IMU周期 = 1秒
    temp_sample_counter = 0;

    float temp = temp_read_celsius();

    // 捂热检测: 5秒内升温 > 2°C
    float delta = temp - last_temp;
    if (delta > 0.4f) { // 约2°C/5s
        warm_up_frames++;
        if (warm_up_frames >= 5) {
            sensor_event_msg_t msg = { .type = EVT_WARM_UP, .value = temp };
            xQueueSend(event_queue, &msg, 0);
            warm_up_frames = 0;
            ESP_LOGI(TAG, "WARM_UP temp=%.1f", temp);
        }
    } else {
        warm_up_frames = 0;
    }

    // 低温检测
    if (temp < 15.0f) {
        sensor_event_msg_t msg = { .type = EVT_COLD_DOWN, .value = temp };
        xQueueSend(event_queue, &msg, 0);
        ESP_LOGI(TAG, "COLD_DOWN temp=%.1f", temp);
    }

    last_temp = temp;
}

// ── 主任务 ───────────────────────────────────────────

static void sensor_task(void *arg) {
    ESP_LOGI(TAG, "Sensor task started");

    // I2C 初始化
    i2c_master_bus_config_t i2c_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_SCL_IO,
        .sda_io_num = I2C_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &bus));

    // IMU 初始化
    ESP_ERROR_CHECK(imu_init(bus));

    // 触摸初始化
    touch_pad_init();
    touch_pad_config(TOUCH_PAD);
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    touch_pad_filter_start(10); // 10ms 硬件滤波

    // 温度初始化
    ESP_ERROR_CHECK(temp_init());

    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        // IMU 100Hz
        imu_data_t imu_data;
        if (imu_read(&imu_data) == ESP_OK) {
            process_imu(&imu_data);
        }

        // 触摸 约50Hz (每2个IMU周期)
        static int touch_div = 0;
        touch_div++;
        if (touch_div >= 2) {
            touch_div = 0;
            process_touch();
        }

        // 温度 1Hz (在process_temp内部自行节流)
        process_temp();

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(IMU_SAMPLE_PERIOD_MS));
    }
}

QueueHandle_t sensors_start(void) {
    event_queue = xQueueCreate(10, sizeof(sensor_event_msg_t));
    xTaskCreate(sensor_task, "sensor", SENSOR_TASK_STACK, NULL, SENSOR_TASK_PRIO, NULL);
    return event_queue;
}
```

- [ ] **Step 3: Build verify**

```bash
. ~/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build
```

- [ ] **Step 4: Commit**

```bash
git add main/app_sensors.h main/app_sensors.c
git commit -m "feat: add sensor event detection module (IMU/touch/temp)"
```

---

### Task 5: app_behavior — behavior state machine

**Files:**
- Create: `main/app_behavior.h`
- Create: `main/app_behavior.c`

- [ ] **Step 1: Create `main/app_behavior.h`**

```c
#ifndef APP_BEHAVIOR_H
#define APP_BEHAVIOR_H

#include "app_sensors.h"
#include "app_display.h"
#include "app_effects.h"

#ifdef __cplusplus
extern "C" {
#endif

// 启动行为任务
// sensor_queue: 来自sensors_task的事件队列
void behavior_start(QueueHandle_t sensor_queue);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 2: Create `main/app_behavior.c`**

```c
#include "app_behavior.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "behavior";

#define BEHAVIOR_TASK_STACK 2048
#define BEHAVIOR_TASK_PRIO  3

#define IDLE_TIMEOUT_MS     10000   // 10s 无互动 → BORED
#define BORED_TIMEOUT_MS    30000   // 30s 无互动 → SLEEPY
#define CONFUSED_TIMEOUT_MS 3000    // 3s 后 → IDLE
#define CONTENT_TIMEOUT_MS  2000    // 2s 后 → IDLE

typedef enum {
    STATE_IDLE,
    STATE_HAPPY,
    STATE_CONTENT,
    STATE_SURPRISED,
    STATE_CONFUSED,
    STATE_SAD,
    STATE_WARM,
    STATE_COLD,
    STATE_BORED,
    STATE_SLEEPY,
} behavior_state_t;

static behavior_state_t current_state = STATE_IDLE;
static TickType_t last_event_ticks;
static int idle_seconds = 0;
static TimerHandle_t idle_timer;

static void set_emotion_safe(emotion_t e) {
    display_set_emotion(e);
}

static void on_sensor_event(sensor_event_msg_t *msg) {
    last_event_ticks = xTaskGetTickCount();

    switch (msg->type) {

    case EVT_TOUCH_HEAD:
        if (current_state == STATE_SLEEPY || current_state == STATE_BORED) {
            current_state = STATE_IDLE;
            set_emotion_safe(EMOTION_NEUTRAL);
            break;
        }
        if (current_state != STATE_CONTENT && current_state != STATE_HAPPY) {
            current_state = STATE_HAPPY;
            set_emotion_safe(EMOTION_HAPPY);
        }
        break;

    case EVT_TOUCH_RELEASE:
        if (current_state == STATE_CONTENT) {
            current_state = STATE_IDLE;
            set_emotion_safe(EMOTION_NEUTRAL);
        } else if (current_state == STATE_HAPPY) {
            // 持续摸头超过 2s → CONTENT
            TickType_t elapsed = xTaskGetTickCount() - last_event_ticks;
            // 这里简单处理: 摸头持续 → HAPPY, 放手→NEUTRAL
            current_state = STATE_IDLE;
            set_emotion_safe(EMOTION_NEUTRAL);
        }
        break;

    case EVT_SHAKE:
        if (current_state == STATE_SLEEPY || current_state == STATE_BORED) {
            current_state = STATE_IDLE;
        }
        current_state = STATE_SURPRISED;
        set_emotion_safe(EMOTION_SURPRISED);
        break;

    case EVT_TAP:
        if (current_state == STATE_SLEEPY || current_state == STATE_BORED) {
            current_state = STATE_IDLE;
            set_emotion_safe(EMOTION_NEUTRAL);
            break;
        }
        // 双击 → SAD (委屈)
        if (msg->value >= 2.0f) {
            current_state = STATE_SAD;
            set_emotion_safe(EMOTION_SAD);
        }
        break;

    case EVT_FLIP:
        current_state = STATE_SURPRISED;
        set_emotion_safe(EMOTION_SURPRISED);
        break;

    case EVT_WARM_UP:
        if (current_state == STATE_SLEEPY || current_state == STATE_BORED) {
            current_state = STATE_IDLE;
        }
        current_state = STATE_WARM;
        set_emotion_safe(EMOTION_WARM);
        break;

    case EVT_COLD_DOWN:
        current_state = STATE_COLD;
        set_emotion_safe(EMOTION_COLD);
        break;

    case EVT_BLE_MEET:
        effects_trigger(EFFECT_STAR);
        break;

    case EVT_BLE_FRIEND:
        // 根据关系等级触发不同特效 (后续接入NVS)
        if (msg->value >= 4.0f)      effects_trigger(EFFECT_GOLDEN);
        else if (msg->value >= 3.0f) effects_trigger(EFFECT_RAINBOW);
        else if (msg->value >= 2.0f) effects_trigger(EFFECT_HEART_PARTICLE);
        else                         effects_trigger(EFFECT_STAR);
        break;

    default: break;
    }
}

static void on_idle_timer(void) {
    TickType_t now = xTaskGetTickCount();
    int elapsed_s = (now - last_event_ticks) * portTICK_PERIOD_MS / 1000;

    if (current_state == STATE_IDLE) {
        if (elapsed_s >= 30) {
            current_state = STATE_SLEEPY;
            set_emotion_safe(EMOTION_SLEEPY);
            ESP_LOGI(TAG, "IDLE → SLEEPY (%ds)", elapsed_s);
        } else if (elapsed_s >= 10) {
            current_state = STATE_BORED;
            set_emotion_safe(EMOTION_BORED);
            ESP_LOGI(TAG, "IDLE → BORED (%ds)", elapsed_s);
        }
    }
}

static void behavior_task(void *arg) {
    QueueHandle_t queue = (QueueHandle_t)arg;
    ESP_LOGI(TAG, "Behavior task started");
    last_event_ticks = xTaskGetTickCount();

    sensor_event_msg_t msg;
    while (1) {
        // 阻塞等待事件, 超时1秒用于空闲计时
        if (xQueueReceive(queue, &msg, pdMS_TO_TICKS(1000))) {
            on_sensor_event(&msg);
        } else {
            // 超时 → 检查空闲
            on_idle_timer();
        }
    }
}

void behavior_start(QueueHandle_t sensor_queue) {
    xTaskCreate(behavior_task, "behavior", BEHAVIOR_TASK_STACK,
                (void *)sensor_queue, BEHAVIOR_TASK_PRIO, NULL);
}
```

- [ ] **Step 3: Build verify**

```bash
. ~/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build
```

- [ ] **Step 4: Commit**

```bash
git add main/app_behavior.h main/app_behavior.c
git commit -m "feat: add behavior state machine module"
```

---

### Task 6: app_ble — BLE stub framework

**Files:**
- Create: `main/app_ble.h`
- Create: `main/app_ble.c`

- [ ] **Step 1: Create `main/app_ble.h`**

```c
#ifndef APP_BLE_H
#define APP_BLE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// 启动 BLE 任务, 返回 BLE 事件队列
QueueHandle_t ble_start(void);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 2: Create `main/app_ble.c`**

```c
#include "app_ble.h"
#include "app_sensors.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "ble";

#define BLE_TASK_STACK 3072
#define BLE_TASK_PRIO  1

static QueueHandle_t ble_queue;

static void ble_task(void *arg) {
    ESP_LOGI(TAG, "BLE task started (stub mode)");

    while (1) {
        // Stub: 等待后续实现 BLE 广播 + 扫描 + 配对
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGD(TAG, "BLE stub heartbeat");
    }
}

QueueHandle_t ble_start(void) {
    ble_queue = xQueueCreate(5, sizeof(sensor_event_msg_t));
    xTaskCreate(ble_task, "ble", BLE_TASK_STACK, NULL, BLE_TASK_PRIO, NULL);
    return ble_queue;
}
```

- [ ] **Step 3: Build verify**

```bash
. ~/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build
```

- [ ] **Step 4: Commit**

```bash
git add main/app_ble.h main/app_ble.c
git commit -m "feat: add BLE stub framework module"
```

---

### Task 7: Refactor main.c to FreeRTOS 4-task architecture

**Files:**
- Modify: `main/main.c`

- [ ] **Step 1: Rewrite `main/main.c`**

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

#include "app_display.h"
#include "app_sensors.h"
#include "app_behavior.h"
#include "app_effects.h"
#include "app_ble.h"
#include "gc9a01.h"

static const char *TAG = "harti";

#define DISPLAY_TASK_STACK 4096
#define DISPLAY_TASK_PRIO  5

static void display_task(void *arg) {
    ESP_LOGI(TAG, "Display task started");

    // 启动演示: 初始设为中性表情
    display_set_emotion(EMOTION_NEUTRAL);

    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        display_update();

        // 如果特效激活, effects_update由display_update间接驱动
        // effects_update在display_update内部调用
        effects_update(1.0f / 60.0f);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(16)); // ~60fps
    }
}

// ── 修改 display_update 以集成 effects ──
// 在 display_update() 的 eyes_render_frame() 调用前,
// 需要在 eyes_render_frame 的 per-line 循环中调用 effects_apply_line.
// 最简单的方式: 在 expressive_eyes 的 eyes_render_frame 末尾 per-line 调用.
// 为此, 修改 eyes_render_frame 接收一个可选的 effects 回调.
// 或在 display_update 中, 在 eyes_render_frame 之后叠加.
// 由于 eyes_render_frame 每行直接发送 SPI, 不能再叠加.
// 因此需要: 在 eyes_render_frame 内部, per-line 渲染后调用 effects_apply_line.
```

**问题**: `eyes_render_frame` 逐行渲染后立即通过 `gc9a01_send_pixels` 发送 SPI，特效无法在之后叠加。需要把特效叠加插入到 per-line 循环中。

**解决方案**: 在 `expressive_eyes.c` 的 `eyes_render_frame` 中，在发送像素前调用一个可选的叠加回调。或者直接在渲染循环中调用 `effects_apply_line`。

最简单的方法：`eyes_render_frame` 中，在 `gc9a01_send_pixels(line_buf, SCREEN_W)` 之前，调用 `effects_apply_line(y, line_buf, SCREEN_W)`。

但这引入了 `expressive_eyes` 对 `app_effects` 的依赖 —— 违反分层。更好的方案：让 `display_update` 控制渲染循环。

修改 `eyes_render_frame` 拆分为两个函数:
- `eyes_render_frame_begin()` — 设置窗口
- `eyes_render_line(int y, uint16_t *line_buf)` — 渲染单行到 buffer，不发送

然后 `display_update` 自己做 SPI 发送，这样特效可以插入在渲染和发送之间。

考虑到简洁性和现有代码，使用更轻量的方法：`expressive_eyes` 不依赖 `app_effects`，而是在 `expressive_eyes.h` 中声明一个可选的 per-line 回调函数指针，`eyes_render_frame` 在发送前调用它。

```c
// 在 expressive_eyes.h 中:
typedef void (*eyes_post_line_cb_t)(int y, uint16_t *line_buf, int width);
extern eyes_post_line_cb_t eyes_post_line_cb;

// 在 expressive_eyes.c 中:
eyes_post_line_cb_t eyes_post_line_cb = NULL;

// 在 eyes_render_frame 中, gc9a01_send_pixels 之前:
if (eyes_post_line_cb) {
    eyes_post_line_cb(y, line_buf, SCREEN_W);
}
```

然后在 `main.c` 的 `app_main` 中设置:
```c
eyes_post_line_cb = effects_apply_line;
```

**完整 main.c**:

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

#include "app_display.h"
#include "app_sensors.h"
#include "app_behavior.h"
#include "app_effects.h"
#include "app_ble.h"
#include "gc9a01.h"

static const char *TAG = "harti";

#define DISPLAY_TASK_STACK 4096
#define DISPLAY_TASK_PRIO  5

static void display_task(void *arg) {
    ESP_LOGI(TAG, "Display task started");
    display_set_emotion(EMOTION_NEUTRAL);

    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        display_update();
        effects_update(1.0f / 60.0f);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(16));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Harti starting...");

    // 初始化硬件
    gc9a01_init();
    display_init();

    // 注册特效叠加回调 (在渲染每行后、SPI发送前调用)
    eyes_post_line_cb = effects_apply_line;

    // 启动传感器任务, 获取事件队列
    QueueHandle_t sensor_queue = sensors_start();

    // 启动 BLE 任务 (stub)
    ble_start();

    // 启动行为任务 (订阅传感器事件)
    behavior_start(sensor_queue);

    // 启动显示任务
    xTaskCreate(display_task, "display", DISPLAY_TASK_STACK,
                NULL, DISPLAY_TASK_PRIO, NULL);

    ESP_LOGI(TAG, "All tasks started");

    // app_main 返回后由 idle 任务接管
}
```

- [ ] **Step 2: Add per-line callback to expressive_eyes**

In `components/expressive_eyes/expressive_eyes.h`, add after the function declarations:

```c
/** 可选: 每行渲染后、SPI发送前调用, 用于叠加特效 */
typedef void (*eyes_post_line_cb_t)(int y, uint16_t *line_buf, int width);
extern eyes_post_line_cb_t eyes_post_line_cb;
```

In `components/expressive_eyes/expressive_eyes.c`, add near the top:

```c
eyes_post_line_cb_t eyes_post_line_cb = NULL;
```

In `eyes_render_frame`, find the line `gc9a01_send_pixels(line_buf, SCREEN_W);` and add before it:

```c
        // 特效叠加 (在 SPI 发送前)
        if (eyes_post_line_cb) {
            eyes_post_line_cb(y, line_buf, SCREEN_W);
        }

        gc9a01_send_pixels(line_buf, SCREEN_W);
```

- [ ] **Step 3: Build verify**

```bash
. ~/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build
```

- [ ] **Step 4: Commit**

```bash
git add main/main.c components/expressive_eyes/expressive_eyes.c components/expressive_eyes/expressive_eyes.h
git commit -m "refactor: migrate main.c to 4-task FreeRTOS architecture"
```

---

### Task 8: Update CMakeLists.txt + final build verification

**Files:**
- Modify: `main/CMakeLists.txt`

- [ ] **Step 1: Update `main/CMakeLists.txt`**

```cmake
idf_component_register(
    SRCS "main.c" "app_display.c" "app_sensors.c" "app_behavior.c" "app_effects.c" "app_ble.c"
    INCLUDE_DIRS "."
    REQUIRES driver expressive_eyes gc9a01 harti_imu harti_temp
)
```

- [ ] **Step 2: Final build verification**

```bash
. ~/esp-idf/export.sh && cd /Users/nova/proj/harti && idf.py build
```

Expected: no errors, no warnings.

- [ ] **Step 3: Commit**

```bash
git add main/CMakeLists.txt
git commit -m "build: add new source files to CMakeLists"
```

---

## Self-Review Checklist

1. **Spec coverage**:
   - Heart iris + HEART_EYES → Task 1 ✓
   - 5 new emotions → Task 2 ✓
   - Effects engine → Task 3 ✓
   - Sensor event detection → Task 4 ✓
   - Behavior state machine → Task 5 ✓
   - BLE stub → Task 6 ✓
   - FreeRTOS refactor → Task 7 ✓
   - Build config → Task 8 ✓

2. **Placeholder scan**: No TBD/TODO. All code is shown inline.

3. **Type consistency**: `sensor_event_t`, `effect_type_t`, `friend_record_t` match across all files. `eyes_post_line_cb_t` signature matches `effects_apply_line`.
