# Props System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add comic-sticker props (heart, teacup, hand, star, sweat drop) that float around the face, triggered via `face_prop_show()` API.

**Architecture:** Extend `decor_params_t` with a `props[3]` array. Props animate via a new `prop_animator` (same pattern as `face_micro.c`). Each sprite gets a `draw_props` function pointer, called as a 9th compositing pass after `draw_decor_overlay`. Shared prop-drawing helpers live in `face_common.h`.

**Tech Stack:** C (ESP-IDF), LVGL tick, existing scanline render primitives in `face_common.h`

---

### Task 1: Data model — new types, decor_params_t, sprite_set_t

**Files:**
- Modify: `components/face_system/face_model.h`
- Modify: `components/face_system/face_model.c`

- [ ] **Step 1: Add prop types and prop_instance_t in face_model.h**

Insert after the `mouth_params_t` definition (after line 55):

```c
/* ── Prop (floating decoration sticker) ─────────────────────── */

typedef enum {
    PROP_NONE = 0,
    PROP_HEART,
    PROP_TEACUP,
    PROP_HAND,
    PROP_STAR_SMALL,
    PROP_SWEAT_DROP,
    PROP_COUNT
} prop_type_t;

typedef struct {
    prop_type_t type;
    float angle;       /* radians, 0 = right, π/2 = top */
    float distance;    /* 0.0..1.0, 1.0 = 100 px from face center */
    float scale;       /* 0.0..1.0 */
    float opacity;     /* 0.0..1.0, for fade in/out */
} prop_instance_t;
```

- [ ] **Step 2: Extend decor_params_t (lines 57-63)**

Replace the existing `decor_params_t`:

```c
typedef struct {
    float blush;
    float tears;
    float stars;
    float sweat;
    float sparkle;
    uint8_t prop_count;
    prop_instance_t props[3];
} decor_params_t;
```

- [ ] **Step 3: Add draw_props to sprite_set_t (after draw_decor_overlay)**

```c
    sprite_draw_func_t draw_decor_overlay;
    sprite_draw_func_t draw_props;
```

- [ ] **Step 4: Update DECOR_NEUTRAL in face_model.c (line 26)**

```c
static const decor_params_t DECOR_NEUTRAL = {
    .blush = 0.38f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0.18f,
    .prop_count = 0,
};
```

All expression presets use named-field initializers; omitted `prop_count`/`props` are zero-initialized.

- [ ] **Step 5: Build**

```bash
idf.py build 2>&1 | tail -5
```
Expected: build succeeds.

- [ ] **Step 6: Commit**

```bash
git add components/face_system/face_model.h components/face_system/face_model.c
git commit -m "feat: add prop types and extend decor_params_t for props system"
```

---

### Task 2: Animator — extend decor_params_lerp

**Files:**
- Modify: `components/face_system/face_animator.c`

- [ ] **Step 1: Extend decor_params_lerp to copy-through props (lines 59-66)**

Replace the function body:

```c
static void decor_params_lerp(const decor_params_t *a, const decor_params_t *b,
                              float t, decor_params_t *out) {
    out->blush   = a->blush   + (b->blush   - a->blush)   * t;
    out->tears   = a->tears   + (b->tears   - a->tears)   * t;
    out->stars   = a->stars   + (b->stars   - a->stars)   * t;
    out->sweat   = a->sweat   + (b->sweat   - a->sweat)   * t;
    out->sparkle = a->sparkle + (b->sparkle - a->sparkle) * t;
    out->prop_count = a->prop_count;
    memcpy(out->props, a->props, sizeof(a->props));
}
```

- [ ] **Step 2: Build and commit**

```bash
idf.py build 2>&1 | tail -5
git add components/face_system/face_animator.c
git commit -m "feat: extend decor_params_lerp to forward props unchanged"
```

---

### Task 3: Prop animator — face_prop.h and face_prop.c

**Files:**
- Create: `components/face_system/face_prop.h`
- Create: `components/face_system/face_prop.c`

- [ ] **Step 1: Create face_prop.h**

```c
#ifndef FACE_PROP_H
#define FACE_PROP_H

#include "face_model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void prop_animator_init(void);
void prop_animator_apply(face_state_t *s);

void face_prop_show(prop_type_t type, float angle, float distance, uint32_t duration_ms);
void face_prop_hide(prop_type_t type, uint32_t duration_ms);
void face_prop_clear(uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 2: Create face_prop.c**

```c
#include "face_prop.h"
#include "face_animator.h"
#include "lvgl.h"
#include <string.h>

#define PROP_DEFAULT_DURATION 200
#define PROP_CLEANUP_FRAMES   2

typedef struct {
    float target_opacity;
    float fade_speed;
    uint8_t dead_frames;
} prop_anim_t;

static prop_anim_t anim[3];

void prop_animator_init(void) {
    memset(anim, 0, sizeof(anim));
}

void prop_animator_apply(face_state_t *s) {
    uint32_t dt = 50;

    for (int i = 0; i < (int)s->decor.prop_count; i++) {
        prop_anim_t *a = &anim[i];
        float op = s->decor.props[i].opacity;

        if (op < a->target_opacity) {
            op += a->fade_speed * dt;
            if (op > a->target_opacity) op = a->target_opacity;
        } else if (op > a->target_opacity) {
            op -= a->fade_speed * dt;
            if (op < a->target_opacity) op = a->target_opacity;
        }
        s->decor.props[i].opacity = op;

        if (a->target_opacity <= 0.01f && op <= 0.01f) a->dead_frames++;
        else a->dead_frames = 0;
    }

    /* Garbage collect dead props */
    int w = 0;
    for (int i = 0; i < (int)s->decor.prop_count; i++) {
        if (anim[i].dead_frames >= PROP_CLEANUP_FRAMES) continue;
        if (w != i) {
            s->decor.props[w] = s->decor.props[i];
            anim[w] = anim[i];
        }
        w++;
    }
    s->decor.prop_count = w;
}

static void insert_prop(face_state_t *s, prop_type_t type,
                        float angle, float distance, uint32_t duration_ms) {
    if (s->decor.prop_count >= 3) return;
    int i = s->decor.prop_count;
    s->decor.props[i].type     = type;
    s->decor.props[i].angle    = angle;
    s->decor.props[i].distance = distance;
    s->decor.props[i].scale    = 0.6f;
    s->decor.props[i].opacity  = 0.0f;
    s->decor.prop_count++;

    float dur = (duration_ms > 0) ? (float)duration_ms : PROP_DEFAULT_DURATION;
    anim[i].target_opacity = 1.0f;
    anim[i].fade_speed     = 1.0f / dur;
    anim[i].dead_frames    = 0;
}

void face_prop_show(prop_type_t type, float angle, float distance, uint32_t duration_ms) {
    face_state_t *s = (face_state_t *)animator_get_state();
    float dur = (duration_ms > 0) ? (float)duration_ms : PROP_DEFAULT_DURATION;

    for (int i = 0; i < (int)s->decor.prop_count; i++) {
        if (s->decor.props[i].type == type) {
            anim[i].target_opacity = 1.0f;
            anim[i].fade_speed     = 1.0f / dur;
            s->decor.props[i].angle    = angle;
            s->decor.props[i].distance = distance;
            return;
        }
    }
    insert_prop(s, type, angle, distance, duration_ms);
}

void face_prop_hide(prop_type_t type, uint32_t duration_ms) {
    face_state_t *s = (face_state_t *)animator_get_state();
    float dur = (duration_ms > 0) ? (float)duration_ms : PROP_DEFAULT_DURATION;
    for (int i = 0; i < (int)s->decor.prop_count; i++) {
        if (s->decor.props[i].type == type) {
            anim[i].target_opacity = 0.0f;
            anim[i].fade_speed     = 1.0f / dur;
            return;
        }
    }
}

void face_prop_clear(uint32_t duration_ms) {
    face_state_t *s = (face_state_t *)animator_get_state();
    float dur = (duration_ms > 0) ? (float)duration_ms : PROP_DEFAULT_DURATION;
    for (int i = 0; i < (int)s->decor.prop_count; i++) {
        anim[i].target_opacity = 0.0f;
        anim[i].fade_speed     = 1.0f / dur;
    }
}
```

- [ ] **Step 3: Build and commit**

```bash
idf.py build 2>&1 | tail -5
git add components/face_system/face_prop.h components/face_system/face_prop.c
git commit -m "feat: add prop animator with show/hide/clear API"
```

---

### Task 4: Shared prop drawing helpers in face_common.h

**Files:**
- Modify: `components/face_system/face_common.h`

- [ ] **Step 1: Append 5 prop draw functions before final `#endif`**

Append after the `draw_wavy_ring_scan` function (before `#ifdef __cplusplus` at line 313):

```c
/* ════════════════════════════════════════════════════════════════
 *  Prop drawing helpers
 * ══════════════════════════════════════════════════════════════ */

/** Heart: two lobe circles + triangle bottom (scanline-safe). */
static inline void draw_heart_scan(int y, float cx, float cy, float size,
                                    uint16_t color, uint16_t *buf, int screen_w) {
    float r = size * 0.42f;
    float lobe_y = cy - size * 0.18f;
    float l_cx = cx - r * 0.6f;
    float r_cx = cx + r * 0.6f;
    float ldy = (float)(y - lobe_y);

    float l_span = 0.0f, r_span = 0.0f;
    if (fabsf(ldy) < r) {
        float hw = sqrtf(r * r - ldy * ldy);
        l_span = hw; r_span = hw;
    }

    float tri_t = ((float)(y - (cy - size * 0.05f))) / (size * 0.55f);
    float tri_w = 0.0f;
    if (tri_t > 0.0f && tri_t < 1.0f) tri_w = size * 0.50f * (1.0f - tri_t);

    int x0 = (int)(cx - size); if (x0 < 0) x0 = 0;
    int x1 = (int)(cx + size); if (x1 >= screen_w) x1 = screen_w - 1;
    for (int x = x0; x <= x1; x++) {
        float dx = (float)(x - cx);
        bool in_l = fabsf(dx - (l_cx - cx)) <= l_span && fabsf(ldy) <= r;
        bool in_r = fabsf(dx - (r_cx - cx)) <= r_span && fabsf(ldy) <= r;
        bool in_t = fabsf(dx) <= tri_w && tri_t > 0.0f && tri_t < 1.0f;
        if ((y < lobe_y && (in_l || in_r)) ||
            (y >= lobe_y - r * 0.5f && (in_l || in_r || in_t)))
            buf[x] = color;
    }
}

/** Teacup: ellipse body + handle arc + steam wisps. */
static inline void draw_teacup_scan(int y, float cx, float cy, float size,
                                     uint16_t color, uint16_t *buf, int screen_w) {
    float rx = size * 0.48f, ry = size * 0.28f;
    float cup_cy = cy + size * 0.12f;

    /* Cup body */
    float nydy = (float)(y - cup_cy) / ry;
    if (fabsf(nydy) < 1.0f) {
        float span = rx * sqrtf(1.0f - nydy * nydy);
        int xl = (int)(cx - span); if (xl < 0) xl = 0;
        int xr = (int)(cx + span); if (xr >= screen_w) xr = screen_w - 1;
        for (int x = xl; x <= xr; x++) buf[x] = color;
    }

    /* Handle arc on right */
    float h_t = ((float)(y - (cup_cy - ry * 0.8f))) / (ry * 2.0f);
    if (h_t > 0.0f && h_t < 1.0f) {
        float hx = cx + rx + sinf(h_t * 3.14159265f) * size * 0.1f;
        int px = (int)hx;
        if (px >= 0 && px < screen_w) buf[px] = color;
        if (px + 1 < screen_w) buf[px + 1] = color;
    }

    /* 3 steam dots */
    float steam_base = cup_cy - ry - 2;
    for (int s = 0; s < 3; s++) {
        float sy = steam_base - size * 0.14f * (float)(s + 1);
        if (fabsf((float)(y - sy)) < 2.0f) {
            float sx = cx - size * 0.15f + s * size * 0.15f;
            for (int dx = -1; dx <= 1; dx++) {
                int px = (int)(sx) + dx;
                if (px >= 0 && px < screen_w) buf[px] = color;
            }
        }
    }
}

/** Hand: palm circle + 3 finger nubs. */
static inline void draw_hand_scan(int y, float cx, float cy, float size,
                                   uint16_t color, uint16_t *buf, int screen_w) {
    /* Palm: filled circle */
    fill_circle_scan(y, (int)cx, (int)(cy + size * 0.1f), size * 0.32f,
                     color, buf, screen_w);

    /* 3 short fingers above palm */
    for (int f = 0; f < 3; f++) {
        float fx = cx - size * 0.18f + f * size * 0.18f;
        float fy = cy - size * 0.28f;
        fill_circle_scan(y, (int)fx, (int)fy, size * 0.1f, color, buf, screen_w);
        /* Vertical bar from finger base to palm */
        if ((float)y >= fy && (float)y < cy && fabsf((float)(y - fy)) < size * 0.35f) {
            for (int dx = -1; dx <= 1; dx++) {
                int px = (int)fx + dx;
                if (px >= 0 && px < screen_w) buf[px] = color;
            }
        }
    }
}

/** Star: 5-pointed using angular check. */
static inline void draw_star_scan(int y, float cx, float cy, float size,
                                   uint16_t color, uint16_t *buf, int screen_w) {
    float dy = (float)(y - cy);
    float outer_r = size * 0.5f;
    float inner_r = size * 0.2f;

    int x0 = (int)(cx - outer_r); if (x0 < 0) x0 = 0;
    int x1 = (int)(cx + outer_r); if (x1 >= screen_w) x1 = screen_w - 1;

    for (int x = x0; x <= x1; x++) {
        float dx = (float)(x - cx);
        float d2 = dx * dx + dy * dy;
        if (d2 > outer_r * outer_r) continue;

        float ang = atan2f(dy, dx);
        float sector = _fmodf(ang * 5.0f / (2.0f * 3.14159265f) + 10.0f, 1.0f);
        /* 5-point star: radius oscillates outer→inner→outer per sector */
        float r_limit = (sector < 0.5f)
            ? (outer_r * (1.0f - sector * 2.0f) + inner_r * sector * 2.0f)
            : (inner_r + (outer_r - inner_r) * (sector - 0.5f) * 2.0f);

        if (d2 <= r_limit * r_limit) buf[x] = color;
    }
}

/** Sweat drop: circle top + pointed bottom. */
static inline void draw_sweat_scan(int y, float cx, float cy, float size,
                                    uint16_t color, uint16_t *buf, int screen_w) {
    float dy = (float)(y - cy);
    float r = size * 0.28f;
    float tip_y = cy + size * 0.45f;
    float circle_cy = cy - size * 0.08f;

    /* Upper circle part */
    float cdy = (float)(y - circle_cy);
    float c_span = 0.0f;
    if (fabsf(cdy) < r) c_span = sqrtf(r * r - cdy * cdy);

    /* Lower triangle: from circle bottom to tip */
    float tri_t = ((float)(y - (circle_cy + r))) / (tip_y - (circle_cy + r));
    float tri_w = 0.0f;
    if (tri_t > 0.0f && tri_t < 1.0f) tri_w = r * (1.0f - tri_t);

    float hw = c_span > tri_w ? c_span : tri_w;
    if (hw <= 0.0f) return;

    int xl = (int)(cx - hw); if (xl < 0) xl = 0;
    int xr = (int)(cx + hw); if (xr >= screen_w) xr = screen_w - 1;
    for (int x = xl; x <= xr; x++) buf[x] = color;
}
```

- [ ] **Step 2: Build and commit**

```bash
idf.py build 2>&1 | tail -5
git add components/face_system/face_common.h
git commit -m "feat: add shared prop drawing helpers to face_common.h"
```

---

### Task 5: Renderer + face_api integration

**Files:**
- Modify: `components/face_system/face_renderer.c`
- Modify: `components/face_system/face_api.h`

- [ ] **Step 1: Add 9th compositing pass in face_renderer.c (after line 37)**

Add after `sp->draw_decor_overlay(y, st, sp, line_buf);`:

```c
        if (sp->draw_props)
            sp->draw_props(y, st, sp, line_buf);
```

- [ ] **Step 2: Update face_api.h — add include and call sites**

Add include after `#include "face_micro.h"`:

```c
#include "face_prop.h"
```

In `face_init()`, add after `micro_animator_init();`:

```c
    prop_animator_init();
```

In `face_render_frame()`, add after `micro_animator_apply(&display_state);`:

```c
    prop_animator_apply(&display_state);
```

- [ ] **Step 3: Build and commit**

```bash
idf.py build 2>&1 | tail -5
git add components/face_system/face_renderer.c components/face_system/face_api.h
git commit -m "feat: integrate props into renderer and face_api"
```

---

### Task 6: Default draw_props — all 9 sprites

**Files:**
- Modify: `components/face_system/sprites/sprite_vector.c`
- Modify: `components/face_system/sprites/sprite_lineart.c`
- Modify: `components/face_system/sprites/sprite_classic.c`
- Modify: `components/face_system/sprites/sprite_cat.c`
- Modify: `components/face_system/sprites/sprite_pixel.c`
- Modify: `components/face_system/sprites/sprite_robot.c`
- Modify: `components/face_system/sprites/sprite_nova.c`
- Modify: `components/face_system/sprites/sprite_pig.c`
- Modify: `components/face_system/sprites/sprite_chibi.c`

- [ ] **Step 1: Add identical draw_props function to each sprite .c file**

In each sprite file, add this function before the `SPRITE_XXX` const definition:

```c
/* ── draw_props ─────────────────────────────────────────────── */

static void draw_props(int y, const face_state_t *st,
                       const sprite_set_t *sp, uint16_t *buf) {
    for (int i = 0; i < st->decor.prop_count; i++) {
        const prop_instance_t *p = &st->decor.props[i];
        if (p->opacity <= 0.01f) continue;

        float r = 100.0f * p->distance;
        float px = CENTER_X + r * cosf(p->angle);
        float py = CENTER_Y - r * sinf(p->angle);
        float sz = 10.0f + p->scale * 12.0f;  /* 10..22 px */

        uint16_t raw_color;
        switch (p->type) {
        case PROP_HEART:      raw_color = sp->pal[PAL_BLUSH]; break;
        case PROP_TEACUP:     raw_color = sp->pal[PAL_SKIN];  break;
        case PROP_HAND:       raw_color = sp->pal[PAL_SKIN];  break;
        case PROP_STAR_SMALL: raw_color = sp->pal[PAL_STAR];  break;
        case PROP_SWEAT_DROP: raw_color = sp->pal[PAL_TEAR];  break;
        default: continue;
        }

        uint16_t color = blend_colors(sp->pal[PAL_BG], raw_color, p->opacity);

        switch (p->type) {
        case PROP_HEART:      draw_heart_scan(y, px, py, sz, color, buf, SCREEN_W); break;
        case PROP_TEACUP:     draw_teacup_scan(y, px, py, sz, color, buf, SCREEN_W); break;
        case PROP_HAND:       draw_hand_scan(y, px, py, sz, color, buf, SCREEN_W); break;
        case PROP_STAR_SMALL: draw_star_scan(y, px, py, sz, color, buf, SCREEN_W); break;
        case PROP_SWEAT_DROP: draw_sweat_scan(y, px, py, sz, color, buf, SCREEN_W); break;
        default: break;
        }
    }
}
```

Each sprite file already includes `face_common.h` and `face_palette.h`, so the helpers are available.

- [ ] **Step 2: Wire draw_props into each SPRITE_XXX definition**

In each sprite's `const sprite_set_t SPRITE_XXX = { ... };`, add:

```c
    .draw_props = draw_props,
```

Place it right after `.draw_decor_overlay = draw_decor_overlay,`.

- [ ] **Step 3: Build and commit**

```bash
idf.py build 2>&1 | tail -5
git add components/face_system/sprites/sprite_*.c
git commit -m "feat: add draw_props to all 9 sprites"
```

---

### Task 7: Integration — app_behavior calls face_prop_show

**Files:**
- Modify: `main/app_behavior.c`

- [ ] **Step 1: Add example prop triggers in behavior state machine**

In `app_behavior.c`, add `#include "face_api.h"` if not already present, then wire example triggers:

```c
/* When entering HEART_EYES expression — show floating hearts */
/* When WARM sensor event — show teacup */
/* When SHAKE event — show small stars */
/* When TAP event — show hand wave */
```

For a minimal integration, add in the HAPPY state handler:

```c
face_prop_show(PROP_HEART, 1.2f, 0.65f, 250);
```

And in the COLD state handler:

```c
face_prop_hide(PROP_HEART, 200);
```

- [ ] **Step 2: Build and commit**

```bash
idf.py build 2>&1 | tail -5
git add main/app_behavior.c
git commit -m "feat: wire prop triggers into app_behavior"
```

---

### Verification

After all tasks, verify on hardware (or in simulator):

1. `face_prop_show(PROP_HEART, 1.2, 0.65, 250)` → heart fades in at upper-right of face
2. `face_prop_hide(PROP_HEART, 200)` → heart fades out
3. Call `face_prop_show()` 3x with different types → all three visible
4. Call `face_prop_show()` 4th time → 4th is ignored (max 3)
5. Switch expressions while a prop is visible → prop persists (not cleared)
6. Prop fades to opacity 0 → auto-cleaned after 2 frames
