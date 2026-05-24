# Scanline Geometry Primitives — Design Spec

**Date:** 2026-05-24

**Goal:** Extend `face_common.h` with 4 new filled-shape scanline primitives
(filled ellipse, triangle, regular N-gon, rounded rectangle), filling the
most critical gaps in the drawing vocabulary while staying within the
implicit-shape / point-in-shape-test approach.

## Current Baseline

- Only one filled-shape primitive: `fill_circle_scan(y, cx, cy, r, color, buf, w)`
- Ring/outline primitives exist for circle, arc, dashed, wavy
- All primitives are `static inline` in `face_common.h`, zero heap allocation
- Scanline model: function receives row `y`, writes directly to `buf[240]`
- ESP32-S3 @ 240MHz, single-precision FPU, 20fps target

## Design

### 1. `fill_ellipse_scan` — Filled Ellipse

Simplest extension. Circle is a special case (rx=ry).

```
Algorithm:
  For row y: if |y-cy| > ry → skip
  Compute x-half-span = rx * sqrt(1 - ((y-cy)/ry)²)
  Fill [cx - span, cx + span]

Per-pixel cost: 0 (only row-level math, then memset-style fill)
Per-row cost:  1 sqrtf + 2 divisions + 2 multiplications
```

```c
static inline void fill_ellipse_scan(int y,
    int cx, int cy, float rx, float ry,
    uint16_t color, uint16_t *buf, int screen_w);
```

### 2. `fill_triangle_scan` — Filled Triangle (Arbitrary 3 Vertices)

Edge-function method. For each row, find the leftmost and rightmost
intersection with the 3 edges, then fill between them.

```
Algorithm:
  Given 3 vertices (x0,y0), (x1,y1), (x2,y2)
  For row y:
    Compute intersection x-coordinates of y with each edge
    Keep min_x and max_x among edges that span this y
    Fill [min_x, max_x]

  Optimization: pre-sort vertices by y for fast edge-span detection.
  Only 2 edges can be active on any given row.

Per-row cost: 2 edge intersection computations + 1 fill-span
```

```c
static inline void fill_triangle_scan(int y,
    float x0, float y0, float x1, float y1, float x2, float y2,
    uint16_t color, uint16_t *buf, int screen_w);
```

### 3. `fill_ngon_scan` — Filled Regular N-gon

Precompute N vertices from (cx, cy, radius, n_sides, rotation), then
delegate to the same edge-function span logic as triangle.

```
Algorithm:
  Precompute V[0..n-1] vertices via cos/sin at call time (stack array, n≤8)
  For row y:
    Find x-intersections with all edges that span this y
    Min x → left bound, max x → right bound
    Fill [left, right]

  Convexity guarantee: regular N-gons are always convex, so
  each row has exactly 0 or 1 fill-span (no gaps).
```

```c
static inline void fill_ngon_scan(int y,
    int cx, int cy, float radius, int n_sides, float rotation_deg,
    uint16_t color, uint16_t *buf, int screen_w);
```

`n_sides` clamped to [3, 8]. Stack array of 8×2 floats = 64 bytes.

### 4. `fill_rounded_rect_scan` — Filled Rounded Rectangle

Decompose row into up to 3 segments: left flat region, full-width middle,
right flat region. Corner arcs handled as quarter-circle distance tests.

```
Algorithm:
  Given (left, top, right, bottom, corner_r)
  For row y:
    If y outside [top, bottom] → skip
    If y is in flat region (y ∈ [top+cr, bottom-cr]): fill full [left, right]
    If y is in corner region (y near top or bottom):
      Left edge: xs = left + cr - sqrt(cr² - (y - corner_y)²)
      Right edge: xe = right - cr + sqrt(cr² - (y - corner_y)²)
      Fill [xs, xe]

  Only top/bottom corner rows call sqrtf (at most 2*cr*2 ≈ 80 rows).
  Flat middle rows are just one memset-style fill.
```

```c
static inline void fill_rounded_rect_scan(int y,
    float left, float top, float right, float bottom, float corner_r,
    uint16_t color, uint16_t *buf, int screen_w);
```

## API Summary

| Function | Complexity/row | New deps |
|----------|---------------|----------|
| `fill_circle_scan` | 1 sqrtf | (existing) |
| `fill_ellipse_scan` | 1 sqrtf | none |
| `fill_triangle_scan` | 2 edge-x + O(1) | none |
| `fill_ngon_scan` | N edge-x (N≤8) | `fill_triangle_scan` edge logic |
| `fill_rounded_rect_scan` | 0–2 sqrtf (corners only) | none |

## Non-Goals

- No ring/outline variants for these shapes (only fill). Outline variants can
  be added later if needed.
- No anti-aliased edges — consistent with existing primitives (hard edges).
  Soft edges are achieved via Bayer dithering at the caller level.
- No concave polygon support — `fill_triangle_scan` works for any 3 points;
  `fill_ngon_scan` handles convex regular N-gons.
- No changes to any sprite code — purely additive to `face_common.h`.
- No new third-party dependencies, no heap allocation.

## Performance Budget

All 4 functions share the same pattern as existing primitives:
- Row-level math (sqrtf, division) → O(1)
- Row fill span → O(span_width) pixels, same as memset
- No per-pixel floating point (except rounded_rect corner rows, amortized)

At 20fps (50ms/frame), these add negligible overhead. The hotspot remains
the chibi sprite's `expf()` limbal ring computation, not these fills.

## Files Changed

| File | Change |
|------|--------|
| `components/face_system/face_common.h` | Add 4 `static inline` functions (~120 lines total) |

## Future (Phase 2): Expression-Specific Shapes

These will be implemented later as dedicated `static inline` functions
using the same implicit-shape approach:
- `fill_heart_scan` — heart curve: `(x²+y²-1)³ - x²y³ ≤ 0`
- `fill_star_scan` — 5-point star: outer_r + inner_r alternating, 10 vertices + edge fill
- `fill_teardrop_scan` — parametric droplet shape
