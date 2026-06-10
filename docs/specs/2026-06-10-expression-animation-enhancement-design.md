# Expression Animation Enhancement — Design Spec
Date: 2026-06-10

## Overview

Three parallel enhancements to the harti face animation system, adding squash-and-stretch entry impacts (A), per-expression micro-life animations and tear particles (B), and a multi-step keyframe sequence engine with BLE control (C). All changes are additive — no existing interfaces break.

---

## A. Entry Impact — Squash-and-Stretch

### Goal
Every expression transition triggers a brief, expression-specific squash-and-stretch deformation that lands on the face body (`squash_x` / `stretch_y`) and optionally on brow and pupil parameters.

### Implementation: `face_micro.c`

Add an `impact_state_t` struct and a single active instance `g_impact`:

```c
typedef struct {
    bool     active;
    uint32_t start_ms;
    float    sq_peak;      // squash_x peak (positive = wider)
    float    st_peak;      // stretch_y peak (positive = taller)
    uint32_t duration_ms;
    float    pupil_peak;   // pupil_scale additive delta at peak (0 = no pupil effect)
    float    brow_peak;    // brow arch.dy additive delta at peak (0 = no brow effect)
} impact_state_t;

static impact_state_t g_impact;
```

**Trigger**: extend `micro_animator_set_expression(id)` to load the table entry and set `g_impact.active = true`.

**Apply**: in `micro_animator_apply()`, after existing sections, evaluate impact:

```
t = elapsed / duration_ms   (clamped 0..1)
envelope = sin(t * π)       // bell curve: 0 → peak → 0

squash_x  += sq_peak  * envelope
stretch_y += st_peak  * envelope
pupil     += pupil_peak * envelope    (both eyes)
brow arch += brow_peak  * envelope   (both brows)
```

**Impact table** (per expression):

| Expression   | sq_peak | st_peak | duration_ms | pupil_peak | brow_peak | Character         |
|--------------|---------|---------|-------------|------------|-----------|-------------------|
| NEUTRAL      | 0       | 0       | 0           | 0          | 0         | none              |
| HAPPY        | +0.15   | −0.10   | 200         | +0.05      | −0.03     | light bounce      |
| SAD          | 0       | −0.08   | 350         | −0.05      | 0         | slow deflate      |
| SURPRISED    | +0.35   | −0.20   | 180         | +0.12      | −0.06     | strong hit        |
| SLEEPY       | 0       | −0.06   | 400         | −0.08      | 0         | slow collapse     |
| ANGRY        | +0.22   | −0.14   | 140         | −0.04      | +0.04     | hard compress     |
| BORED        | 0       | 0       | 0           | 0          | 0         | none              |
| EXCITED      | +0.30   | −0.18   | 320         | +0.10      | −0.05     | 2-stage bounce (see below) |
| CONFUSED     | +0.12   | 0       | 180         | 0          | +0.03     | sideways squash   |
| CONTENT      | 0       | +0.04   | 300         | 0          | −0.02     | gentle settle     |
| COLD         | +0.18   | −0.10   | 200         | −0.06      | 0         | shiver spike      |
| WARM         | +0.10   | +0.08   | 220         | +0.04      | −0.04     | soft expand       |
| HEART_EYES   | +0.18   | +0.12   | 200         | 0          | −0.04     | heart swell       |
| THINKING     | −0.05   | +0.06   | 200         | 0          | 0         | subtle tilt       |
| DIZZY        | +0.25   | −0.15   | 280         | 0          | 0         | spin land         |

EXCITED uses a two-peak envelope. Add two extra fields to `impact_state_t`:

```c
float    sq2_peak;     // second squash peak (0 = single-peak)
float    st2_peak;
uint32_t peak2_ms;     // ms offset of second peak from start
```

EXCITED config: sq_peak=+0.30 at 120ms, sq2_peak=−0.15 at 240ms, total duration=320ms.
Envelope: use `sin((elapsed/peak_ms)*π)` for each peak independently, sum the results.

### No new public API
Impact fires automatically inside `micro_animator_set_expression()`. No caller changes needed.

---

## B. Micro-Life Enhancements

### B1. Per-Expression Micro-Animations (10 missing expressions)

Extend `face_micro.c`'s expression-linked section (currently has COLD/BORED/SLEEPY). Add a data-driven table instead of hard-coded switch cases:

```c
typedef struct {
    float freq_hz;        // oscillation frequency
    float amplitude;      // parameter delta amplitude
    uint8_t target;       // which param: EYE_LID / BROW_ARCH / PUPIL / FACE_SQ / MOUTH / BLUSH
    uint8_t eye_mask;     // bit0=left, bit1=right, 0=both
    bool    gated;        // true = only fires when sin(gate_freq) > 0.4 (intermittent)
    float   gate_freq_hz; // gate oscillation frequency (used when gated=true)
} expr_micro_cfg_t;
```

Each expression gets an array of up to 3 `expr_micro_cfg_t` entries. Applied via:

```
delta = sin(2π * freq_hz * total_s) * amplitude
if gated: only apply when sin(2π * gate_freq_hz * total_s) > 0.4
```

**Per-expression micro table**:

| Expression | Effect 1 | Effect 2 | Effect 3 |
|------------|----------|----------|----------|
| HAPPY      | bot_lid 0.5Hz ±0.015 | brow arch 1.1Hz ±0.012 (gated) | — |
| SURPRISED  | iris fast-dart 0.3Hz ±0.018 (gated, gate=0.07Hz) | — | — |
| ANGRY      | top_lid jitter 10Hz ±0.008 (gated, gate=2Hz) | brow arch 2Hz ±0.012 | — |
| EXCITED    | face squash 4Hz ±0.020 | shine 1.8Hz ±0.06 | — |
| CONFUSED   | brow[0] arch 1Hz ±0.022 (left only, anti-phase) | brow[1] arch 1Hz ±0.022 (right, opposite phase) | — |
| CONTENT    | top_lid 0.12Hz ±0.035 (deep slow blink overlay) | — | — |
| THINKING   | iris dx drift: slow sinusoidal toward +0.08 right-up | — | — |
| WARM       | blush 0.3Hz ±0.04 | — | — |
| HEART_EYES | pupil_scale 1.5Hz ±0.045 (heartbeat) | — | — |
| SAD        | handled by tear particles (B2), no additional micro | — | — |

### B2. Eye Contact Simulation

In `face_micro.c`, add a new periodic state machine `eye_contact`:

- State: IDLE → CONVERGING → HOLDING → RELEASING
- Trigger: every 8–15 seconds (random, reset after each cycle)
- CONVERGING (200ms): iris offset lerps toward (0, 0) additive to gaze
- HOLDING (200–400ms): iris stays at center
- RELEASING (150ms): iris smoothly returns to current gaze target
- Suppressed during SLEEPY and BORED expressions

### B3. Tear Particle System

**New struct in `face_vivid.c`**:

```c
typedef struct {
    float  y_offset;   // current vertical offset from eye bottom (pixels, grows downward)
    float  speed;      // pixels per second
    float  phase;      // spawn phase offset for stagger
    bool   active;
    uint32_t spawn_at; // absolute ms timestamp to spawn
} tear_particle_t;

static tear_particle_t tears[4];  // [0],[1] = left eye, [2],[3] = right eye
```

**Logic in `face_vivid_apply()`**:
- Active when `s->decor.tears > 0.05f`
- Each particle falls at `speed = 18 + randf()*10` px/s
- Despawn when y_offset > 30px, respawn after random 0.4–1.2s delay
- Left/right pairs have 0.2s and 0.6s phase offsets respectively

**New public API in `face_vivid.h`**:

```c
typedef struct { float x; float y; float opacity; } tear_drop_t;
void face_vivid_get_tears(tear_drop_t out[4], const face_state_t *s);
```

**Caller: `sprite_vector.c` `draw_decor_overlay()`**:
Replace the current static tear rendering with a call to `face_vivid_get_tears()` and render each active drop as a small filled teardrop at `(eye_x + drop.x, eye_bottom + drop.y)`.

The original `decor.tears` float continues to control particle spawn rate and overall opacity — no `face_state_t` schema change.

### B4. Prop Spring Entrance

In `face_vivid.c`, extend `prop_snapshot_t`:

```c
typedef struct {
    prop_type_t type;
    float base_angle, base_distance, base_scale;
    bool  spawn_active;
    uint32_t spawn_start_ms;
} prop_snapshot_t;

#define PROP_SPRING_DURATION_MS 400
#define PROP_SPRING_OVERSHOOT   0.30f   // 30% overshoot
```

**Spring envelope** (applied to `p->scale`):

```
t = elapsed / PROP_SPRING_DURATION_MS
spring_scale = 1 + OVERSHOOT * sin(t*π) * (1-t)²
p->scale = base_scale * spring_scale     (during spawn)
```

Triggers when `snapshots[i].type` changes or prop first appears.

---

## C. Keyframe Sequence Engine

### New file: `components/face_system/face_seq.h / face_seq.c`

**Data model**:

```c
#define SEQ_MAX_STEPS 16

typedef struct {
    expression_id_t expr_id;    // 0xFF = use expression at call site (no-op step)
    uint16_t        hold_ms;    // time to hold this expression before advancing
    uint16_t        trans_ms;   // transition duration override (0 = use expr default)
} seq_step_t;

typedef struct {
    const seq_step_t *steps;
    uint8_t           step_count;
    uint8_t           loop_count;   // 0=once, 0xFF=infinite
} face_seq_t;
```

**Public API**:

```c
void face_seq_init(void);
void face_seq_tick(void);                              // call every frame from main loop

// Play a built-in named sequence (fires automatically on expression change too)
void face_seq_play(const face_seq_t *seq);

// Play a dynamically-pushed sequence (from BLE)
void face_seq_play_steps(const seq_step_t *steps, uint8_t count, uint8_t loops);

void face_seq_stop(void);
bool face_seq_is_playing(void);

// Called by face_temperament or app_behavior when expression changes
// If the new expression has a built-in entry sequence, auto-plays it
void face_seq_on_expression_set(expression_id_t id);
```

**Internal state machine**: IDLE → PLAYING(step=N, waiting for hold_ms) → advance step or loop/stop.

### Built-in entry sequences

Each expression can optionally have an entry sequence in `BUILTIN_ENTRY_SEQS[]` (NULL = use direct set, no sequence):

| Expression | Entry Sequence |
|------------|---------------|
| SURPRISED  | NEUTRAL(80ms) → SURPRISED(hold) — brief neutral flash then pop |
| EXCITED    | HAPPY(120ms) → EXCITED(hold) — ramp through happy first |
| DIZZY      | SURPRISED(100ms) → DIZZY(hold) — surprise then settle |
| HEART_EYES | HAPPY(150ms) → HEART_EYES(hold) — happy becomes heart |
| Others     | NULL (direct set) |

### BLE Protocol

**Note**: `app_ble.c` is currently a stub (`ble_start()` logs and returns). The BLE protocol is specified here for completeness; full BLE implementation is a separate task. The sequence engine itself works without BLE — it is testable via direct `face_seq_play_steps()` calls from `app_behavior.c`.

**New Characteristic**: `FACE_SEQ_CHAR` (suggested UUID: `0xBE01`, same service as future face expression characteristic)

**Write payload** (max 82 bytes = 2-byte header + 16 × 5-byte steps):

```
Byte 0:  loop_count   uint8   (0=once, 0xFF=infinite)
Byte 1:  step_count   uint8   (1..16)
Bytes 2+N×5:
  [N*5+2] expr_id     uint8   (0x00..0x0E = expression, 0xFF = stop)
  [N*5+3] hold_ms     uint16  little-endian
  [N*5+5] trans_ms    uint16  little-endian (0 = use expr default)
```

**Stop command**: write `[0x00, 0x00]` (2 bytes) — immediately stops sequence and holds current expression.

**Validation**: step_count > 16 → reject with BLE error. expr_id > EXPRESSION_COUNT and != 0xFF → reject.

### Integration into existing call path

The single integration point is `face_api.h`'s `face_set_expression()` inline, which already fans out to `animator_set_expression` and `micro_animator_set_expression`. Extend it:

```c
// face_api.h  face_set_expression() — after:
static inline void face_set_expression(expression_id_t id) {
    face_seq_on_expression_set(id);       // NEW: plays entry seq OR calls animator directly
    micro_animator_set_expression(id);    // unchanged
    face_temperament_notify_expression_change(id); // already exists
}
```

`face_seq_on_expression_set()` calls `animator_set_expression()` internally when no entry sequence is defined, so no call-site changes are needed in `app_behavior.c` or `main.c`.

`face_seq_tick()` is added to `face_api.h`'s `face_animator_tick()`:

```c
static inline void face_animator_tick(void) {
    animator_tick();
    face_seq_tick();   // NEW
}
```

**Interruption rule**: a direct `face_set_expression()` call always stops any running sequence first (including BLE-pushed ones). `face_seq_stop()` is called at the top of `face_seq_on_expression_set()` before checking for an entry sequence.

---

## File Change Summary

| File | Change type | Scope |
|------|-------------|-------|
| `components/face_system/face_micro.c` | Extend | impact_state_t + 10 expr micros + eye contact |
| `components/face_system/face_micro.h` | Extend | no new public API (all internal) |
| `components/face_system/face_vivid.c` | Extend | tear particles + prop spring entrance |
| `components/face_system/face_vivid.h` | Extend | add `face_vivid_get_tears()` |
| `components/face_system/sprites/sprite_vector.c` | Small edit | `draw_decor_overlay()` uses tear particles |
| `components/face_system/face_seq.c` | **New** | full sequence engine |
| `components/face_system/face_seq.h` | **New** | public API |
| `main/app_ble.c` | Extend | new FACE_SEQ_CHAR characteristic |
| `components/face_system/face_api.h` | Small edit | `face_set_expression()` + `face_animator_tick()` include seq calls |
| `main/app_ble.c` | Stub note | BLE seq characteristic spec only; full impl deferred |

Estimated new code: ~500 lines total. No changes to `face_model.h`, `face_state_t`, `face_animator`, or BLE expression characteristic — fully backward compatible.

---

## Constraints Respected

- Black-and-white only: no color introduced anywhere
- `face_micro` micro-animations remain stateless overlays (do not couple to expression IDs in the renderer)
- `face_seq` does not bypass `face_animator` — sequences still go through the lerp pipeline
- BLE payload fits in a single ATT write (≤ 82 bytes < typical 182-byte MTU)
