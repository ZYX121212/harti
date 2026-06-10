# Expression Animation Enhancement (Phase 2 A+B+C) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add entry-impact squash-and-stretch (A), data-driven per-expression micro-animations + tear particles + prop spring (B), and a keyframe sequence engine with BLE stub (C) to the harti face firmware.

**Architecture:** All changes are additive overlays on the existing 5-layer render pipeline (animator → micro → prop → vivid → temperament → renderer). Tasks A and B modify `face_micro.c` and `face_vivid.c` in-place. Task C adds a new `face_seq.c/h` and wires it into `face_api.h`. No `face_model.h` or `face_state_t` schema changes.

**Tech Stack:** ESP32-S3, FreeRTOS, LVGL (tick only), C99, custom scanline renderer. Build: `cmake --build build` from `/Users/nova/proj/harti`. All colors strictly black (`RGB565(0,0,0)`) or white (`RGB565(255,255,255)`) only.

---

## File Map

| File | Change |
|------|--------|
| `components/face_system/face_micro.c` | Task A: impact state + table + envelope; Task B1: expr micro table; Task B2: eye contact SM |
| `components/face_system/face_vivid.c` | Task B3: tear particle physics; Task B4: prop spring entrance |
| `components/face_system/face_vivid.h` | Task B3: add `face_vivid_get_tears()` |
| `components/face_system/sprites/sprite_vector.c` | Task B3: replace static tear rendering |
| `components/face_system/face_seq.h` | Task C1: new public API |
| `components/face_system/face_seq.c` | Task C1: sequence engine + built-in entry seqs |
| `components/face_system/CMakeLists.txt` | Task C1: add face_seq.c to SRCS |
| `components/face_system/face_api.h` | Task C2: wire face_seq into face_set_expression + face_animator_tick |
| `main/app_ble.c` | Task C3: FACE_SEQ_CHAR stub + parse helper |

---

## Task A: Entry Impact Squash-and-Stretch

**Files:** Modify `components/face_system/face_micro.c`

- [ ] **Step A1: Add `impact_def_t` table and `impact_state_t` live instance**

  In `face_micro.c`, after the `next_startle_at` state variable line (around line 107), insert:

  ```c
  /* ── Entry impact ─────────────────────────────────────────── */
  typedef struct {
      float    sq_peak;
      float    st_peak;
      uint32_t duration_ms;
      float    pupil_peak;
      float    brow_peak;
      float    sq2_peak;    /* second squash peak (EXCITED); 0 = single-peak */
      float    st2_peak;
      uint32_t peak2_ms;    /* ms offset of 2nd bell centre from start; 0 = unused */
  } impact_def_t;

  /* Per-expression entry-impact parameters.  Order matches emotion_t enum (EMOTION_COUNT=16). */
  static const impact_def_t IMPACT_DEFS[16] = {
      /* [0]  NEUTRAL     */ {0},
      /* [1]  HAPPY       */ {+0.15f, -0.10f, 200, +0.05f, -0.03f},
      /* [2]  SAD         */ { 0.0f,  -0.08f, 350, -0.05f,  0.0f},
      /* [3]  SURPRISED   */ {+0.35f, -0.20f, 180, +0.12f, -0.06f},
      /* [4]  SLEEPY      */ { 0.0f,  -0.06f, 400, -0.08f,  0.0f},
      /* [5]  ANGRY       */ {+0.22f, -0.14f, 140, -0.04f, +0.04f},
      /* [6]  BORED       */ {0},
      /* [7]  EXCITED     */ {+0.30f, -0.18f, 320, +0.10f, -0.05f, -0.15f, 0.0f, 240},
      /* [8]  CONFUSED    */ {+0.12f,  0.0f,  180,  0.0f,  +0.03f},
      /* [9]  CONTENT     */ { 0.0f,  +0.04f, 300,  0.0f,  -0.02f},
      /* [10] COLD        */ {+0.18f, -0.10f, 200, -0.06f,  0.0f},
      /* [11] WARM        */ {+0.10f, +0.08f, 220, +0.04f, -0.04f},
      /* [12] HEART_EYES  */ {+0.18f, +0.12f, 200,  0.0f,  -0.04f},
      /* [13] THINKING    */ {-0.05f, +0.06f, 200,  0.0f,   0.0f},
      /* [14] DIZZY       */ {+0.25f, -0.15f, 280,  0.0f,   0.0f},
      /* [15] UPSIDE_DOWN */ { 0.0f,  -0.06f, 350,  0.0f,   0.0f},
  };

  typedef struct {
      bool     active;
      uint32_t start_ms;
      float    sq_peak, st_peak;
      uint32_t duration_ms;
      float    pupil_peak, brow_peak;
      float    sq2_peak, st2_peak;
      uint32_t peak2_ms;
  } impact_state_t;

  static impact_state_t g_impact;
  ```

- [ ] **Step A2: Trigger impact in `micro_animator_set_expression()`**

  Replace the existing body of `micro_animator_set_expression()` (currently lines 158–166):

  ```c
  void micro_animator_set_expression(expression_id_t id) {
      if (id == current_expr_id) return;
      current_expr_id = id;
      /* Reset expression-linked state machines */
      sigh_phase      = SIGH_IDLE;
      next_sigh_at    = lv_tick_get() + rand_ms(3000, 8000);
      startle_phase   = STARTLE_IDLE;
      next_startle_at = lv_tick_get() + rand_ms(5000, 12000);
      /* Trigger entry impact */
      if (id < 16 && IMPACT_DEFS[id].duration_ms > 0) {
          const impact_def_t *d = &IMPACT_DEFS[id];
          g_impact.active      = true;
          g_impact.start_ms    = lv_tick_get();
          g_impact.sq_peak     = d->sq_peak;
          g_impact.st_peak     = d->st_peak;
          g_impact.duration_ms = d->duration_ms;
          g_impact.pupil_peak  = d->pupil_peak;
          g_impact.brow_peak   = d->brow_peak;
          g_impact.sq2_peak    = d->sq2_peak;
          g_impact.st2_peak    = d->st2_peak;
          g_impact.peak2_ms    = d->peak2_ms;
      } else {
          g_impact.active = false;
      }
      /* Suppress eye contact during SLEEPY and BORED */
      if (id == EMOTION_SLEEPY || id == EMOTION_BORED) {
          ec_phase    = EC_IDLE;
          ec_next_at  = UINT32_MAX;
          ec_offset_x = ec_offset_y = 0.0f;
      } else {
          ec_phase   = EC_IDLE;
          ec_next_at = lv_tick_get() + rand_ms(8000, 15000);
      }
  }
  ```

  Note: the `ec_*` variables are added in Task B2. For Task A only, add the impact block and leave the ec block out — it can be added cleanly in Task B2 Step B2-3.

  Simpler version for Task A only:

  ```c
  void micro_animator_set_expression(expression_id_t id) {
      if (id == current_expr_id) return;
      current_expr_id = id;
      sigh_phase      = SIGH_IDLE;
      next_sigh_at    = lv_tick_get() + rand_ms(3000, 8000);
      startle_phase   = STARTLE_IDLE;
      next_startle_at = lv_tick_get() + rand_ms(5000, 12000);
      /* Trigger entry impact */
      if (id < 16 && IMPACT_DEFS[id].duration_ms > 0) {
          const impact_def_t *d = &IMPACT_DEFS[id];
          g_impact.active      = true;
          g_impact.start_ms    = lv_tick_get();
          g_impact.sq_peak     = d->sq_peak;
          g_impact.st_peak     = d->st_peak;
          g_impact.duration_ms = d->duration_ms;
          g_impact.pupil_peak  = d->pupil_peak;
          g_impact.brow_peak   = d->brow_peak;
          g_impact.sq2_peak    = d->sq2_peak;
          g_impact.st2_peak    = d->st2_peak;
          g_impact.peak2_ms    = d->peak2_ms;
      } else {
          g_impact.active = false;
      }
  }
  ```

- [ ] **Step A3: Apply impact envelope in `micro_animator_apply()`**

  At the very end of `micro_animator_apply()` — after the `startle_phase` block, just before the closing `}` — insert:

  ```c
      /* ── A. Entry impact: sin-bell squash/stretch ──────────── */
      if (g_impact.active && g_impact.duration_ms > 0) {
          uint32_t elapsed = now - g_impact.start_ms;
          if (elapsed >= g_impact.duration_ms) {
              g_impact.active = false;
          } else {
              float t   = (float)elapsed / (float)g_impact.duration_ms;
              float env = sinf(t * 3.14159265f);

              float sq = g_impact.sq_peak * env;
              float st = g_impact.st_peak * env;

              if (g_impact.peak2_ms > 0) {
                  uint32_t bell2_start = g_impact.peak2_ms / 2;
                  if (elapsed >= bell2_start) {
                      uint32_t bell2_dur = g_impact.duration_ms - bell2_start;
                      float t2   = clampf((float)(elapsed - bell2_start) /
                                          (float)bell2_dur, 0.0f, 1.0f);
                      float env2 = sinf(t2 * 3.14159265f);
                      sq += g_impact.sq2_peak * env2;
                      st += g_impact.st2_peak * env2;
                  }
              }

              s->face.squash_x  += sq;
              s->face.stretch_y += st;
              for (int i = 0; i < 2; i++) {
                  s->eye[i].pupil_scale += g_impact.pupil_peak * env;
                  s->brow[i].arch.dy    += g_impact.brow_peak  * env;
              }
          }
      }
  ```

- [ ] **Step A4: Build and verify**

  ```bash
  cd /Users/nova/proj/harti && cmake --build build 2>&1 | tail -5
  ```

  Expected: `[100%]` success line, zero errors.

- [ ] **Step A5: Commit**

  ```bash
  cd /Users/nova/proj/harti
  git add components/face_system/face_micro.c
  git commit -m "feat(micro): entry impact squash-and-stretch on every expression transition"
  ```

---

## Task B1: Per-Expression Micro Table (10 expressions)

**Files:** Modify `components/face_system/face_micro.c`

- [ ] **Step B1-1: Add `micro_target_t` enum and `expr_micro_cfg_t` struct**

  After the `impact_state_t` block added in Task A, insert:

  ```c
  /* ── Per-expression micro-animation table ─────────────────── */
  typedef enum {
      MICRO_BOT_LID = 0,
      MICRO_TOP_LID,
      MICRO_BROW_ARCH,
      MICRO_PUPIL_SCALE,
      MICRO_FACE_SQ,
      MICRO_SHINE,
      MICRO_BLUSH,
      MICRO_IRIS_DX,
  } micro_target_t;

  typedef struct {
      float   freq_hz;
      float   amplitude;
      uint8_t target;       /* micro_target_t */
      uint8_t eye_mask;     /* 0=both, 1=left only, 2=right only */
      bool    gated;
      float   gate_freq_hz;
      float   phase_offset; /* radians; π = anti-phase */
  } expr_micro_cfg_t;

  #define MAX_EXPR_MICROS 3
  ```

- [ ] **Step B1-2: Add the 16-row micro table**

  Immediately after the struct:

  ```c
  static const expr_micro_cfg_t EXPR_MICRO_TABLE[16][MAX_EXPR_MICROS] = {
      /* [0]  NEUTRAL     */ {{0}},
      /* [1]  HAPPY       */ {
          {0.5f,  0.015f, MICRO_BOT_LID,    0, false, 0.0f,  0.0f},
          {1.1f,  0.012f, MICRO_BROW_ARCH,  0, true,  0.3f,  0.0f},
          {0}
      },
      /* [2]  SAD         */ {{0}},  /* handled by tear particles */
      /* [3]  SURPRISED   */ {
          {0.3f,  0.018f, MICRO_IRIS_DX,    0, true,  0.07f, 0.0f},
          {0}, {0}
      },
      /* [4]  SLEEPY      */ {{0}},
      /* [5]  ANGRY       */ {
          {10.0f, 0.008f, MICRO_TOP_LID,    0, true,  2.0f,  0.0f},
          {2.0f,  0.012f, MICRO_BROW_ARCH,  0, false, 0.0f,  0.0f},
          {0}
      },
      /* [6]  BORED       */ {{0}},
      /* [7]  EXCITED     */ {
          {4.0f,  0.020f, MICRO_FACE_SQ,    0, false, 0.0f,  0.0f},
          {1.8f,  0.060f, MICRO_SHINE,      0, false, 0.0f,  0.0f},
          {0}
      },
      /* [8]  CONFUSED    */ {
          {1.0f,  0.022f, MICRO_BROW_ARCH,  1, false, 0.0f,  0.0f},
          {1.0f,  0.022f, MICRO_BROW_ARCH,  2, false, 0.0f,  3.14159265f},
          {0}
      },
      /* [9]  CONTENT     */ {
          {0.12f, 0.035f, MICRO_TOP_LID,    0, false, 0.0f,  0.0f},
          {0}, {0}
      },
      /* [10] COLD        */ {{0}},  /* handled by shiver */
      /* [11] WARM        */ {
          {0.3f,  0.040f, MICRO_BLUSH,      0, false, 0.0f,  0.0f},
          {0}, {0}
      },
      /* [12] HEART_EYES  */ {
          {1.5f,  0.045f, MICRO_PUPIL_SCALE,0, false, 0.0f,  0.0f},
          {0}, {0}
      },
      /* [13] THINKING    */ {
          {0.1f,  0.080f, MICRO_IRIS_DX,    0, false, 0.0f,  0.0f},
          {0}, {0}
      },
      /* [14] DIZZY       */ {{0}},
      /* [15] UPSIDE_DOWN */ {{0}},
  };
  ```

- [ ] **Step B1-3: Add apply loop in `micro_animator_apply()`**

  Between section `3. Breathing` and section `4. Tilt tracking`, insert a new section:

  ```c
      /* ══════════════════════════════════════════════════════
         3-D. Per-expression micro table
         ══════════════════════════════════════════════════════ */
      if (current_expr_id < 16) {
          float total_s = (float)now / 1000.0f;
          for (int m = 0; m < MAX_EXPR_MICROS; m++) {
              const expr_micro_cfg_t *cfg = &EXPR_MICRO_TABLE[current_expr_id][m];
              if (cfg->freq_hz == 0.0f) break;

              if (cfg->gated) {
                  float gate = sinf(2.0f * 3.14159265f * cfg->gate_freq_hz * total_s);
                  if (gate <= 0.4f) continue;
              }

              float delta = sinf(2.0f * 3.14159265f * cfg->freq_hz * total_s
                                 + cfg->phase_offset) * cfg->amplitude;

              int lo = 0, hi = 1;
              if (cfg->eye_mask == 1) hi = 0;
              else if (cfg->eye_mask == 2) lo = 1;

              switch ((micro_target_t)cfg->target) {
              case MICRO_BOT_LID:
                  for (int i = lo; i <= hi; i++) s->eye[i].bot_lid_mid.dy += delta;
                  break;
              case MICRO_TOP_LID:
                  for (int i = lo; i <= hi; i++) s->eye[i].top_lid_mid.dy += delta;
                  break;
              case MICRO_BROW_ARCH:
                  for (int i = lo; i <= hi; i++) s->brow[i].arch.dy += delta;
                  break;
              case MICRO_PUPIL_SCALE:
                  for (int i = lo; i <= hi; i++) s->eye[i].pupil_scale += delta;
                  break;
              case MICRO_FACE_SQ:
                  s->face.squash_x  += delta;
                  s->face.stretch_y -= delta * 0.5f;
                  break;
              case MICRO_SHINE:
                  for (int i = lo; i <= hi; i++) s->eye[i].shine_intensity += delta;
                  break;
              case MICRO_BLUSH:
                  s->decor.blush += delta;
                  break;
              case MICRO_IRIS_DX:
                  for (int i = lo; i <= hi; i++) s->eye[i].iris_center.dx += delta;
                  break;
              }
          }
      }
  ```

- [ ] **Step B1-4: Build and verify**

  ```bash
  cd /Users/nova/proj/harti && cmake --build build 2>&1 | tail -5
  ```

  Expected: clean build, no errors or new warnings.

- [ ] **Step B1-5: Commit**

  ```bash
  cd /Users/nova/proj/harti
  git add components/face_system/face_micro.c
  git commit -m "feat(micro): data-driven per-expression micro table for 10 expressions"
  ```

---

## Task B2: Eye Contact Simulation

**Files:** Modify `components/face_system/face_micro.c`

- [ ] **Step B2-1: Add eye contact state variables**

  After the `next_startle_at` line (around line 107), insert:

  ```c
  /* Eye contact simulation */
  typedef enum { EC_IDLE, EC_CONVERGING, EC_HOLDING, EC_RELEASING } ec_phase_t;
  static ec_phase_t ec_phase    = EC_IDLE;
  static uint32_t   ec_start    = 0;
  static uint32_t   ec_next_at  = 0;
  static uint32_t   ec_hold_dur = 300;
  static float      ec_from_x   = 0.0f;
  static float      ec_from_y   = 0.0f;
  static float      ec_offset_x = 0.0f;
  static float      ec_offset_y = 0.0f;
  ```

- [ ] **Step B2-2: Initialize in `micro_animator_init()`**

  After the `// Breath` init block, add:

  ```c
      // Eye contact
      ec_phase    = EC_IDLE;
      ec_next_at  = now + rand_ms(8000, 15000);
      ec_offset_x = ec_offset_y = 0.0f;
  ```

- [ ] **Step B2-3: Suppress / reset in `micro_animator_set_expression()`**

  At the end of the function body (before the closing `}`), add:

  ```c
      /* Suppress eye contact during SLEEPY and BORED */
      if (id == EMOTION_SLEEPY || id == EMOTION_BORED) {
          ec_phase    = EC_IDLE;
          ec_next_at  = UINT32_MAX;
          ec_offset_x = ec_offset_y = 0.0f;
      } else {
          ec_phase   = EC_IDLE;
          ec_next_at = lv_tick_get() + rand_ms(8000, 15000);
      }
  ```

- [ ] **Step B2-4: Add eye contact state machine in `micro_animator_apply()`**

  After section `6. Apply to eyes` (the `for (int i = 0; i < 2; i++)` block that adds `drift_x/y` to `position`), insert:

  ```c
      /* ══════════════════════════════════════════════════════
         7. Eye contact simulation
         ══════════════════════════════════════════════════════ */
      if (current_expr_id != EMOTION_SLEEPY && current_expr_id != EMOTION_BORED
          && ec_next_at != UINT32_MAX) {

          if (ec_phase == EC_IDLE && now >= ec_next_at) {
              ec_phase  = EC_CONVERGING;
              ec_start  = now;
              ec_from_x = gaze_x;
              ec_from_y = gaze_y;
          }

          switch (ec_phase) {
          case EC_IDLE:
              break;
          case EC_CONVERGING: {
              float t = clampf((float)(now - ec_start) / 200.0f, 0.0f, 1.0f);
              ec_offset_x = -lerpf(0.0f, ec_from_x, ease_out(t));
              ec_offset_y = -lerpf(0.0f, ec_from_y, ease_out(t));
              if (t >= 1.0f) {
                  ec_phase    = EC_HOLDING;
                  ec_start    = now;
                  ec_hold_dur = rand_ms(200, 400);
              }
              break;
          }
          case EC_HOLDING:
              ec_offset_x = -ec_from_x;
              ec_offset_y = -ec_from_y;
              if (now - ec_start >= ec_hold_dur) {
                  ec_phase = EC_RELEASING;
                  ec_start = now;
              }
              break;
          case EC_RELEASING: {
              float t = clampf((float)(now - ec_start) / 150.0f, 0.0f, 1.0f);
              ec_offset_x = lerpf(-ec_from_x, 0.0f, ease_out(t));
              ec_offset_y = lerpf(-ec_from_y, 0.0f, ease_out(t));
              if (t >= 1.0f) {
                  ec_offset_x = ec_offset_y = 0.0f;
                  ec_phase    = EC_IDLE;
                  ec_next_at  = now + rand_ms(8000, 15000);
              }
              break;
          }
          }

          for (int i = 0; i < 2; i++) {
              s->eye[i].iris_center.dx += ec_offset_x;
              s->eye[i].iris_center.dy += ec_offset_y;
          }
      }
  ```

- [ ] **Step B2-5: Build and verify**

  ```bash
  cd /Users/nova/proj/harti && cmake --build build 2>&1 | tail -5
  ```

  Expected: clean build.

- [ ] **Step B2-6: Commit**

  ```bash
  cd /Users/nova/proj/harti
  git add components/face_system/face_micro.c
  git commit -m "feat(micro): eye contact simulation — periodic iris convergence every 8-15s"
  ```

---

## Task B3: Tear Particle System

**Files:** Modify `face_vivid.h`, `face_vivid.c`, `sprites/sprite_vector.c`

- [ ] **Step B3-1: Add `tear_drop_t` and `face_vivid_get_tears()` to `face_vivid.h`**

  Replace the full contents of `components/face_system/face_vivid.h`:

  ```c
  #pragma once
  #include "face_model.h"

  #ifdef __cplusplus
  extern "C" {
  #endif

  void face_vivid_init(void);
  void face_vivid_apply(face_state_t *s);

  typedef struct {
      float x;        /* lateral offset from eye centre (pixels) */
      float y;        /* vertical offset from eye bottom (pixels, grows downward) */
      float opacity;  /* 0 = inactive */
  } tear_drop_t;

  /* Returns 4 drops: [0][1] = left eye, [2][3] = right eye */
  void face_vivid_get_tears(tear_drop_t out[4], const face_state_t *s);

  #ifdef __cplusplus
  }
  #endif
  ```

- [ ] **Step B3-2: Add tear state variables to `face_vivid.c`**

  After `static bool snapshot_init = false;` (line 21), insert:

  ```c
  /* ── Tear particle state ─────────────────────────────────── */
  typedef struct {
      float    y_offset;    /* pixels below eye bottom */
      float    speed;       /* pixels per second */
      bool     active;
      uint32_t spawn_at;    /* absolute ms to activate */
  } tear_particle_t;

  static tear_particle_t s_tears[4];
  static uint32_t        s_tear_prev_ms = 0;
  ```

  Also add `#include <stdlib.h>` after `#include <string.h>` at the top, and add this private helper after the include block:

  ```c
  static float randf_s(void) { return (float)rand() / (float)RAND_MAX; }
  ```

- [ ] **Step B3-3: Initialize tears in `face_vivid_init()`**

  Replace the existing `face_vivid_init()` body:

  ```c
  void face_vivid_init(void) {
      vivid_t0 = lv_tick_get();
      memset(snapshots, 0, sizeof(snapshots));
      snapshot_init = false;

      /* Tear particles: staggered spawn to avoid synchronised drops */
      s_tears[0] = (tear_particle_t){0.0f, 0.0f, false,   0};   /* left  #0: immediate */
      s_tears[1] = (tear_particle_t){0.0f, 0.0f, false, 200};   /* left  #1: +0.2s     */
      s_tears[2] = (tear_particle_t){0.0f, 0.0f, false,   0};   /* right #0: immediate */
      s_tears[3] = (tear_particle_t){0.0f, 0.0f, false, 600};   /* right #1: +0.6s     */
      s_tear_prev_ms = lv_tick_get();
  }
  ```

- [ ] **Step B3-4: Add tear physics update at end of `face_vivid_apply()`**

  Before the closing `}` of `face_vivid_apply()`, insert:

  ```c
      /* ── Tear particle physics ────────────────────────────── */
      if (s->decor.tears > 0.05f) {
          float dt_s = (float)(now - s_tear_prev_ms) / 1000.0f;
          if (dt_s > 0.1f) dt_s = 0.1f;

          for (int i = 0; i < 4; i++) {
              tear_particle_t *tp = &s_tears[i];
              if (!tp->active) {
                  if (now >= tp->spawn_at) {
                      tp->active   = true;
                      tp->y_offset = 0.0f;
                      tp->speed    = 18.0f + randf_s() * 10.0f;
                  }
              } else {
                  tp->y_offset += tp->speed * dt_s;
                  if (tp->y_offset > 30.0f) {
                      tp->active   = false;
                      tp->spawn_at = now + 400u + (uint32_t)(randf_s() * 800.0f);
                  }
              }
          }
      }
      s_tear_prev_ms = now;
  ```

- [ ] **Step B3-5: Implement `face_vivid_get_tears()` in `face_vivid.c`**

  After the closing `}` of `face_vivid_apply()`, add:

  ```c
  void face_vivid_get_tears(tear_drop_t out[4], const face_state_t *s) {
      for (int i = 0; i < 4; i++) {
          if (s_tears[i].active && s->decor.tears > 0.05f) {
              out[i].x       = 0.0f;
              out[i].y       = s_tears[i].y_offset;
              out[i].opacity = s->decor.tears;
          } else {
              out[i].x = out[i].y = out[i].opacity = 0.0f;
          }
      }
  }
  ```

- [ ] **Step B3-6: Replace static tear rendering in `sprite_vector.c`**

  Add `#include "../face_vivid.h"` in `sprite_vector.c` after `#include "face_common.h"` (line 5).

  In `draw_decor_overlay()`, replace the entire `// ── Tears: white droplet outlines below eyes ──` block (lines 702–722) with:

  ```c
      // ── Tears: physics-based particle drops ──
      if (dp->tears > 0.01f) {
          tear_drop_t drops[4];
          face_vivid_get_tears(drops, st);

          /* Hard-coded eye x positions match vector sprite layout (±35 px from centre).
             Eye bottom = eye_cy(CENTER_Y-5) + eye_radius. */
          static const int EYE_CX[4] = {
              CENTER_X - 35, CENTER_X - 35,
              CENTER_X + 35, CENTER_X + 35,
          };
          int eye_bot = (CENTER_Y - 5) + (int)sp->eye_radius;

          for (int i = 0; i < 4; i++) {
              if (drops[i].opacity < 0.05f) continue;
              int   tx     = EYE_CX[i] + (int)drops[i].x;
              int   drop_y = eye_bot + (int)drops[i].y;
              float fy_d   = (float)(y - drop_y);
              float r      = 3.5f;
              if (fy_d < -(r + 1.0f) || fy_d > (r + 1.0f)) continue;
              for (int x = tx - 5; x <= tx + 5; x++) {
                  if (x < 0 || x >= SCREEN_W) continue;
                  float dx2 = (float)(x - tx);
                  if (dx2 * dx2 + fy_d * fy_d < r * r) {
                      buf[x] = pal[PAL_SCLERA];
                  }
              }
          }
      }
  ```

- [ ] **Step B3-7: Build and verify**

  ```bash
  cd /Users/nova/proj/harti && cmake --build build 2>&1 | tail -5
  ```

  Expected: clean build.

- [ ] **Step B3-8: Commit**

  ```bash
  cd /Users/nova/proj/harti
  git add components/face_system/face_vivid.h \
          components/face_system/face_vivid.c \
          components/face_system/sprites/sprite_vector.c
  git commit -m "feat(vivid): tear particle system — physics-based falling drops below eyes"
  ```

---

## Task B4: Prop Spring Entrance

**Files:** Modify `components/face_system/face_vivid.c`

- [ ] **Step B4-1: Add spring constants and extend `prop_snapshot_t`**

  Replace the existing `prop_snapshot_t` typedef (lines 12–17):

  ```c
  #define PROP_SPRING_DURATION_MS 400u
  #define PROP_SPRING_OVERSHOOT   0.30f

  typedef struct {
      prop_type_t type;
      float       base_angle;
      float       base_distance;
      float       base_scale;
      bool        spawn_active;
      uint32_t    spawn_start_ms;
  } prop_snapshot_t;
  ```

- [ ] **Step B4-2: Trigger spring on new prop or type change**

  In `face_vivid_apply()`, replace the two snapshot-init/type-change `if` blocks (lines 57–70):

  ```c
          /* First-frame or type change: reset snapshot and start spring */
          bool is_new     = (!snapshot_init || i >= 3);
          bool changed    = (!is_new && p->type != snapshots[i].type);
          if (is_new || changed) {
              snapshots[i].type           = p->type;
              snapshots[i].base_angle     = p->angle;
              snapshots[i].base_distance  = p->distance;
              snapshots[i].base_scale     = p->scale;
              snapshots[i].spawn_active   = true;
              snapshots[i].spawn_start_ms = now;
              if (is_new) continue;
          }
  ```

- [ ] **Step B4-3: Apply spring envelope after the oscillation switch block**

  After the `switch (p->type) { ... }` closing `}` (before the for-loop's `}`), insert:

  ```c
          /* Spring entrance: 400ms overshoot scale envelope on appear */
          if (snapshots[i].spawn_active) {
              uint32_t elapsed = now - snapshots[i].spawn_start_ms;
              if (elapsed >= PROP_SPRING_DURATION_MS) {
                  snapshots[i].spawn_active = false;
              } else {
                  float t      = (float)elapsed / (float)PROP_SPRING_DURATION_MS;
                  float omit   = 1.0f - t;
                  float spring = 1.0f + PROP_SPRING_OVERSHOOT
                                 * sinf(t * 3.14159265f) * omit * omit;
                  p->scale = snapshots[i].base_scale * spring;
              }
          }
  ```

- [ ] **Step B4-4: Build and verify**

  ```bash
  cd /Users/nova/proj/harti && cmake --build build 2>&1 | tail -5
  ```

  Expected: clean build.

- [ ] **Step B4-5: Commit**

  ```bash
  cd /Users/nova/proj/harti
  git add components/face_system/face_vivid.c
  git commit -m "feat(vivid): prop spring entrance — 30% overshoot bounce on prop appear"
  ```

---

## Task C1: Keyframe Sequence Engine

**Files:** Create `components/face_system/face_seq.h`, `components/face_system/face_seq.c`; modify `components/face_system/CMakeLists.txt`

- [ ] **Step C1-1: Create `face_seq.h`**

  ```c
  /* components/face_system/face_seq.h */
  #pragma once
  #include "face_model.h"
  #include <stdbool.h>
  #include <stdint.h>

  #ifdef __cplusplus
  extern "C" {
  #endif

  #define SEQ_MAX_STEPS 16

  typedef struct {
      expression_id_t expr_id;   /* 0xFF = play the expression that triggered this seq */
      uint16_t        hold_ms;   /* time to hold this step after transition completes */
      uint16_t        trans_ms;  /* transition duration override (0 = use expr default) */
  } seq_step_t;

  typedef struct {
      const seq_step_t *steps;
      uint8_t           step_count;
      uint8_t           loop_count;  /* 0=once, 0xFF=infinite */
  } face_seq_t;

  /* Lifecycle */
  void face_seq_init(void);
  void face_seq_tick(void);   /* call every frame from face_animator_tick() */

  /* Playback */
  void face_seq_play(const face_seq_t *seq);
  void face_seq_play_steps(const seq_step_t *steps, uint8_t count, uint8_t loops);
  void face_seq_stop(void);
  bool face_seq_is_playing(void);

  /* Called by face_set_expression() — stops any running seq, checks for a
     built-in entry sequence, falls back to animator_set_expression(). */
  void face_seq_on_expression_set(expression_id_t id);

  #ifdef __cplusplus
  }
  #endif
  ```

- [ ] **Step C1-2: Create `face_seq.c`**

  ```c
  /* components/face_system/face_seq.c */
  #include "face_seq.h"
  #include "face_animator.h"
  #include "../../main/harti_config.h"
  #include "lvgl.h"
  #include <string.h>

  /* ── State ──────────────────────────────────────────────────── */

  typedef enum { SEQ_IDLE, SEQ_PLAYING } seq_state_t;

  static seq_state_t      s_state      = SEQ_IDLE;
  static seq_step_t       s_buf[SEQ_MAX_STEPS];
  static uint8_t          s_count      = 0;
  static uint8_t          s_loop_count = 0;
  static uint8_t          s_loop_done  = 0;
  static uint8_t          s_cur        = 0;
  static uint32_t         s_step_start = 0;
  static expression_id_t  s_target     = 0; /* expression that triggered this seq */

  /* ── Built-in entry sequences ───────────────────────────────── */
  /* Each ends with a {0xFF, 0, 0} step that resolves to the target expression. */

  static const seq_step_t SEQ_SURPRISED[] = {
      {EMOTION_NEUTRAL,    80, 0},
      {0xFF,                0, 0},
  };
  static const seq_step_t SEQ_EXCITED[] = {
      {EMOTION_HAPPY,     120, 0},
      {0xFF,                0, 0},
  };
  static const seq_step_t SEQ_DIZZY[] = {
      {EMOTION_SURPRISED, 100, 0},
      {0xFF,                0, 0},
  };
  static const seq_step_t SEQ_HEART_EYES[] = {
      {EMOTION_HAPPY,     150, 0},
      {0xFF,                0, 0},
  };

  typedef struct {
      expression_id_t   id;
      const seq_step_t *steps;
      uint8_t           count;
  } entry_def_t;

  static const entry_def_t ENTRY_DEFS[] = {
      {EMOTION_SURPRISED,  SEQ_SURPRISED,  2},
      {EMOTION_EXCITED,    SEQ_EXCITED,    2},
      {EMOTION_DIZZY,      SEQ_DIZZY,      2},
      {EMOTION_HEART_EYES, SEQ_HEART_EYES, 2},
  };
  #define ENTRY_DEF_COUNT ((int)(sizeof(ENTRY_DEFS) / sizeof(ENTRY_DEFS[0])))

  /* ── Internal helpers ───────────────────────────────────────── */

  static void apply_step(uint8_t idx, uint32_t now);

  static void advance(uint32_t now) {
      s_cur++;
      if (s_cur >= s_count) {
          s_loop_done++;
          if (s_loop_count == 0xFF || s_loop_done < s_loop_count) {
              s_cur = 0;
          } else {
              s_state = SEQ_IDLE;
              return;
          }
      }
      apply_step(s_cur, now);
  }

  static void apply_step(uint8_t idx, uint32_t now) {
      s_step_start = now;
      const seq_step_t *step = &s_buf[idx];

      if (step->expr_id == 0xFF) {
          animator_set_expression(s_target);
      } else {
          animator_set_expression(step->expr_id);
      }

      uint32_t total = (uint32_t)step->trans_ms + (uint32_t)step->hold_ms;
      if (total == 0) {
          advance(now);  /* zero-duration step: advance immediately */
      }
  }

  /* ── Public API ─────────────────────────────────────────────── */

  void face_seq_init(void) {
      s_state = SEQ_IDLE;
      s_count = 0;
  }

  void face_seq_tick(void) {
      if (s_state != SEQ_PLAYING) return;
      uint32_t now     = lv_tick_get();
      uint32_t elapsed = now - s_step_start;
      const seq_step_t *step  = &s_buf[s_cur];
      uint32_t total = (uint32_t)step->trans_ms + (uint32_t)step->hold_ms;
      if (total > 0 && elapsed >= total) {
          advance(now);
      }
  }

  void face_seq_play(const face_seq_t *seq) {
      if (!seq) return;
      face_seq_play_steps(seq->steps, seq->step_count, seq->loop_count);
  }

  void face_seq_play_steps(const seq_step_t *steps, uint8_t count, uint8_t loops) {
      if (!steps || count == 0 || count > SEQ_MAX_STEPS) return;
      memcpy(s_buf, steps, count * sizeof(seq_step_t));
      s_count      = count;
      s_loop_count = loops;
      s_loop_done  = 0;
      s_cur        = 0;
      s_state      = SEQ_PLAYING;
      apply_step(0, lv_tick_get());
  }

  void face_seq_stop(void) {
      s_state = SEQ_IDLE;
  }

  bool face_seq_is_playing(void) {
      return s_state == SEQ_PLAYING;
  }

  void face_seq_on_expression_set(expression_id_t id) {
      face_seq_stop();
      s_target = id;

      for (int i = 0; i < ENTRY_DEF_COUNT; i++) {
          if (ENTRY_DEFS[i].id == id) {
              face_seq_play_steps(ENTRY_DEFS[i].steps, ENTRY_DEFS[i].count, 0);
              return;
          }
      }

      animator_set_expression(id);
  }
  ```

- [ ] **Step C1-3: Add `face_seq.c` to CMakeLists.txt**

  In `components/face_system/CMakeLists.txt`, change the SRCS first line from:

  ```
  SRCS "face_model.c" "face_animator.c" "face_renderer.c" "face_palette.c" "face_micro.c" "face_prop.c" "face_vivid.c" "face_temperament.c"
  ```

  to:

  ```
  SRCS "face_model.c" "face_animator.c" "face_renderer.c" "face_palette.c" "face_micro.c" "face_prop.c" "face_vivid.c" "face_temperament.c" "face_seq.c"
  ```

- [ ] **Step C1-4: Build and verify**

  ```bash
  cd /Users/nova/proj/harti && cmake --build build 2>&1 | tail -5
  ```

  Expected: `face_seq.c` compiles, clean link.

- [ ] **Step C1-5: Commit**

  ```bash
  cd /Users/nova/proj/harti
  git add components/face_system/face_seq.h \
          components/face_system/face_seq.c \
          components/face_system/CMakeLists.txt
  git commit -m "feat(seq): keyframe sequence engine with built-in entry seqs (SURPRISED/EXCITED/DIZZY/HEART_EYES)"
  ```

---

## Task C2: Integrate Sequence Engine into `face_api.h`

**Files:** Modify `components/face_system/face_api.h`

- [ ] **Step C2-1: Add `face_seq.h` include**

  After `#include "face_temperament.h"` (line 10), add:

  ```c
  #include "face_seq.h"
  ```

- [ ] **Step C2-2: Update `face_init()` to call `face_seq_init()`**

  Replace `face_init()`:

  ```c
  static inline void face_init(void) {
      animator_init();
      renderer_init();
      micro_animator_init();
      prop_animator_init();
      face_vivid_init();
      face_temperament_init();
      face_seq_init();
      renderer_set_sprite(sprite_registry_default());
  }
  ```

- [ ] **Step C2-3: Update `face_set_expression()` to route through seq engine**

  Replace `face_set_expression()`:

  ```c
  static inline void face_set_expression(expression_id_t id) {
      face_seq_on_expression_set(id);   /* plays entry seq OR calls animator directly */
      micro_animator_set_expression(id);
  }
  ```

- [ ] **Step C2-4: Update `face_animator_tick()` to tick the seq engine**

  Replace `face_animator_tick()`:

  ```c
  static inline void face_animator_tick(void) {
      animator_tick();
      face_seq_tick();
  }
  ```

- [ ] **Step C2-5: Build and verify**

  ```bash
  cd /Users/nova/proj/harti && cmake --build build 2>&1 | tail -5
  ```

  Expected: clean build. `face_seq_init`, `face_seq_on_expression_set`, `face_seq_tick` must resolve without linker errors.

- [ ] **Step C2-6: Commit**

  ```bash
  cd /Users/nova/proj/harti
  git add components/face_system/face_api.h
  git commit -m "feat(api): wire face_seq into face_set_expression and face_animator_tick"
  ```

---

## Task C3: BLE Stub for FACE_SEQ_CHAR

**Files:** Modify `main/app_ble.c`

- [ ] **Step C3-1: Replace `app_ble.c` with stub + parser**

  ```c
  /* main/app_ble.c */
  #include "app_ble.h"
  #include "face_seq.h"
  #include "../main/harti_config.h"
  #include "esp_log.h"
  #include <stdint.h>
  #include <string.h>

  static const char *TAG = "ble";

  /*
   * FACE_SEQ_CHAR  (UUID 0xBE01, same service as future face expression characteristic)
   * ──────────────────────────────────────────────────────────────────────────────────
   * Write payload (max 82 bytes = 2-byte header + 16 × 5-byte steps):
   *   Byte 0:  loop_count  uint8   (0=once, 0xFF=infinite)
   *   Byte 1:  step_count  uint8   (1..16)
   *   Bytes 2+N*5:
   *     [N*5+2] expr_id   uint8   (0x00..EMOTION_COUNT-1, or 0xFF=target expr)
   *     [N*5+3] hold_ms   uint16  little-endian
   *     [N*5+5] trans_ms  uint16  little-endian (0 = use expr default)
   *
   * Stop command: write [0x00, 0x00]  (step_count = 0)
   * Validation:   step_count > 16 or payload too short → reject (log error)
   *               expr_id >= EMOTION_COUNT and != 0xFF → reject
   */
  void ble_handle_face_seq_write(const uint8_t *data, uint16_t len) {
      if (len < 2) return;
      uint8_t loop_count = data[0];
      uint8_t step_count = data[1];

      if (step_count == 0) {
          face_seq_stop();
          return;
      }
      if (step_count > SEQ_MAX_STEPS || (uint16_t)(2u + (uint16_t)step_count * 5u) > len) {
          ESP_LOGE(TAG, "face_seq: bad payload len=%d step_count=%d", len, step_count);
          return;
      }

      static seq_step_t steps[SEQ_MAX_STEPS];
      for (int i = 0; i < step_count; i++) {
          const uint8_t *p = data + 2 + i * 5;
          uint8_t  expr_id = p[0];
          uint16_t hold_ms  = (uint16_t)((uint16_t)p[1] | ((uint16_t)p[2] << 8));
          uint16_t trans_ms = (uint16_t)((uint16_t)p[3] | ((uint16_t)p[4] << 8));
          if (expr_id != 0xFF && expr_id >= EMOTION_COUNT) {
              ESP_LOGE(TAG, "face_seq: invalid expr_id=%d at step %d", expr_id, i);
              return;
          }
          steps[i].expr_id  = expr_id;
          steps[i].hold_ms  = hold_ms;
          steps[i].trans_ms = trans_ms;
      }
      face_seq_play_steps(steps, step_count, loop_count);
      ESP_LOGI(TAG, "face_seq: playing %d steps loops=%d", step_count, loop_count);
  }

  void ble_start(void) {
      ESP_LOGI(TAG, "BLE not implemented — FACE_SEQ_CHAR stub ready (call ble_handle_face_seq_write when characteristic written)");
  }
  ```

  Note: `app_ble.c` uses `#include "../main/harti_config.h"`. The file is at `main/app_ble.c` so the include should be `#include "harti_config.h"` (same directory):

  ```c
  #include "harti_config.h"
  ```

- [ ] **Step C3-2: Build and verify**

  ```bash
  cd /Users/nova/proj/harti && cmake --build build 2>&1 | tail -5
  ```

  Expected: clean build.

- [ ] **Step C3-3: Commit**

  ```bash
  cd /Users/nova/proj/harti
  git add main/app_ble.c
  git commit -m "feat(ble): FACE_SEQ_CHAR stub — parse + validate helper ready for BLE wiring"
  ```

---

## Self-Review

**Spec coverage (spec doc: `docs/specs/2026-06-10-expression-animation-enhancement-design.md`)**

| Requirement | Task |
|-------------|------|
| A — `impact_state_t` struct with 2-peak fields | A1 |
| A — 16-row impact table (all expressions including DIZZY/UPSIDE_DOWN) | A1 |
| A — trigger in `micro_animator_set_expression()` | A2 |
| A — sin-bell envelope in `micro_animator_apply()` | A3 |
| A — EXCITED two-peak bell (`sq2_peak`, `peak2_ms=240`) | A3 |
| B1 — `expr_micro_cfg_t` data-driven struct | B1-1 |
| B1 — 10-expression micro table (HAPPY through THINKING) | B1-2 |
| B1 — apply loop with gating, phase offset, eye_mask dispatch | B1-3 |
| B2 — eye contact state machine (IDLE→CONVERGING→HOLDING→RELEASING) | B2-1 through B2-4 |
| B2 — 8–15s random interval | B2-4 |
| B2 — suppressed during SLEEPY and BORED | B2-3 |
| B3 — `tear_particle_t` struct + 4 particles (left[0][1], right[2][3]) | B3-2 |
| B3 — physics: 18–28 px/s fall, despawn at 30px, respawn 0.4–1.2s | B3-4 |
| B3 — 0.2s and 0.6s phase stagger | B3-3 |
| B3 — `face_vivid_get_tears()` public API | B3-1, B3-5 |
| B3 — sprite_vector.c uses particle system (replaces static tear) | B3-6 |
| B4 — `prop_snapshot_t` spawn fields | B4-1 |
| B4 — spring 400ms, 30% overshoot | B4-3 |
| B4 — trigger on type change or first appear | B4-2 |
| C — `seq_step_t`, `face_seq_t`, `SEQ_MAX_STEPS=16` | C1-1 |
| C — full state machine IDLE→PLAYING→advance/loop/stop | C1-2 |
| C — `0xFF` step resolves to target expression | C1-2 |
| C — 4 built-in entry seqs (SURPRISED, EXCITED, DIZZY, HEART_EYES) | C1-2 |
| C — CMakeLists.txt updated | C1-3 |
| C — `face_set_expression()` routes through `face_seq_on_expression_set()` | C2-3 |
| C — `face_seq_init()` called in `face_init()` | C2-2 |
| C — `face_seq_tick()` called in `face_animator_tick()` | C2-4 |
| C BLE — payload spec, step_count validation, expr_id validation | C3-1 |
| C BLE — stop command (step_count=0) | C3-1 |

**Type consistency:** `seq_step_t` fields (`expr_id`, `hold_ms`, `trans_ms`) used identically in C1, C2, C3. `tear_drop_t` fields (`x`, `y`, `opacity`) match between B3-1 definition and B3-5/B3-6 usage. `impact_def_t` field order matches initialization in A2.

**No placeholders:** All steps contain complete, copy-pasteable code.
