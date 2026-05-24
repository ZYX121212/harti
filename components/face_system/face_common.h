#ifndef FACE_COMMON_H
#define FACE_COMMON_H

#include <stdint.h>
#include <math.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ════════════════════════════════════════════════════════════════
 *  Basic math / color utilities
 * ══════════════════════════════════════════════════════════════ */

static inline float dist_sq(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

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

/* ── fmodf fallback (newlib nano may lack it) ──────────────── */

static inline float _fmodf(float x, float y) {
    return x - (float)((int)(x / y)) * y;
}

/* ════════════════════════════════════════════════════════════════
 *  Curve evaluation primitives
 * ══════════════════════════════════════════════════════════════ */

/** 二阶贝塞尔（弧线）: p0→p1→p2, t ∈ [0,1]. */
static inline float quad_bezier(float p0, float p1, float p2, float t) {
    float s = 1.0f - t;
    return s * s * p0 + 2.0f * s * t * p1 + t * t * p2;
}

/** 三阶贝塞尔（完美 S 型）: p0→p1→p2→p3, t ∈ [0,1]. */
static inline float cubic_bezier(float p0, float p1, float p2, float p3, float t) {
    float s = 1.0f - t;
    return s * s * s * p0 + 3.0f * s * s * t * p1 +
           3.0f * s * t * t * p2 + t * t * t * p3;
}

/** Catmull-Rom 平滑曲线（无棱角）: 在 p1→p2 之间插值, t ∈ [0,1].
 *  p0,p1,p2,p3 为连续 4 个控制点，曲线精确通过 p1(t=0) 和 p2(t=1). */
static inline float cr_spline(float p0, float p1, float p2, float p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2.0f * p1) +
                   (-p0 + p2) * t +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

/* ════════════════════════════════════════════════════════════════
 *  Scanline curve drawing （逐行渲染）
 * ══════════════════════════════════════════════════════════════ */

/**
 * 二阶贝塞尔 scanline 绘制。
 * 在当前 y 行上，遍历曲线 x 范围，对每个 x 求 t 再算曲线 y，
 * 若 |y - curve_y| < half_thick 则画像素。
 */
static inline void draw_quad_bezier_scan(int y,
                                          float x0, float y0,
                                          float x1, float y1,
                                          float x2, float y2,
                                          float half_thick,
                                          uint16_t color, uint16_t *buf, int screen_w) {
    float x_min = x0 < x2 ? x0 : x2;
    float x_max = x0 > x2 ? x0 : x2;
    int xs = (int)x_min - (int)half_thick - 2; if (xs < 0) xs = 0;
    int xe = (int)x_max + (int)half_thick + 2; if (xe >= screen_w) xe = screen_w - 1;

    for (int x = xs; x <= xe; x++) {
        // Map x → parameter t via linear interpolation + Newton refinement
        float t = (x - x0) / (x2 - x0 + 0.001f);
        if (t < -0.1f || t > 1.1f) continue;
        // 1-step Newton: improve t so x(t) ≈ actual x
        float qx = quad_bezier(x0, x1, x2, t);
        float dxdt = 2.0f * (1.0f - t) * (x1 - x0) + 2.0f * t * (x2 - x1);
        if (fabsf(dxdt) > 0.5f) t += ((float)x - qx) / dxdt;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float cy = quad_bezier(y0, y1, y2, t);
        if (fabsf((float)y - cy) < half_thick) {
            buf[x] = color;
        }
    }
}

/**
 * 三阶贝塞尔 scanline 绘制。
 * 在当前 y 行上画出 S 型曲线或任意三阶贝塞尔。
 */
static inline void draw_cubic_bezier_scan(int y,
                                           float x0, float y0,
                                           float x1, float y1,
                                           float x2, float y2,
                                           float x3, float y3,
                                           float half_thick,
                                           uint16_t color, uint16_t *buf, int screen_w) {
    float x_min = x0 < x3 ? x0 : x3;
    float x_max = x0 > x3 ? x0 : x3;
    // 控制点也可能超出端点范围
    if (x1 < x_min) x_min = x1;
    if (x2 < x_min) x_min = x2;
    if (x1 > x_max) x_max = x1;
    if (x2 > x_max) x_max = x2;
    int xs = (int)x_min - (int)half_thick - 3; if (xs < 0) xs = 0;
    int xe = (int)x_max + (int)half_thick + 3; if (xe >= screen_w) xe = screen_w - 1;

    for (int x = xs; x <= xe; x++) {
        float t = (x - x0) / (x3 - x0 + 0.001f);
        if (t < -0.1f || t > 1.1f) continue;
        // 1-step Newton
        float qx = cubic_bezier(x0, x1, x2, x3, t);
        float dxdt = 3.0f * (1.0f - t) * (1.0f - t) * (x1 - x0)
                   + 6.0f * (1.0f - t) * t * (x2 - x1)
                   + 3.0f * t * t * (x3 - x2);
        if (fabsf(dxdt) > 0.5f) t += ((float)x - qx) / dxdt;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float cy = cubic_bezier(y0, y1, y2, y3, t);
        if (fabsf((float)y - cy) < half_thick) {
            buf[x] = color;
        }
    }
}

/**
 * Catmull-Rom 样条 scanline 绘制。
 * 在 p1→p2 段上绘制平滑曲线。
 */
static inline void draw_cr_scan(int y,
                                 float x0, float y0,
                                 float x1, float y1,
                                 float x2, float y2,
                                 float x3, float y3,
                                 float half_thick,
                                 uint16_t color, uint16_t *buf, int screen_w) {
    float x_min = x1 < x2 ? x1 : x2;
    float x_max = x1 > x2 ? x1 : x2;
    int xs = (int)x_min - (int)half_thick - 3;
    if (xs < 0) xs = 0;
    int xe = (int)x_max + (int)half_thick + 3;
    if (xe >= screen_w) xe = screen_w - 1;

    for (int x = xs; x <= xe; x++) {
        float t = (x - x1) / (x2 - x1 + 0.001f);
        if (t < -0.1f || t > 1.1f) continue;
        // 1-step Newton
        float qx = cr_spline(x0, x1, x2, x3, t);
        (void)cr_spline(y0, y1, y2, y3, t);  // keep y for consistency
        // Derivative of CR at t
        float dt = 0.001f;
        float qx2 = cr_spline(x0, x1, x2, x3, t + dt);
        float dxdt = (qx2 - qx) / dt;
        if (fabsf(dxdt) > 0.5f) t += ((float)x - qx) / dxdt;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float cy = cr_spline(y0, y1, y2, y3, t);
        if (fabsf((float)y - cy) < half_thick) {
            buf[x] = color;
        }
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Ring / outline / pattern helpers
 * ══════════════════════════════════════════════════════════════ */

/** 点在环形内？ d2 = 到圆心距离平方, [inner_r², outer_r²]. */
static inline bool in_ring_sq(float d2, float inner_r, float outer_r) {
    return d2 >= (inner_r * inner_r) && d2 <= (outer_r * outer_r);
}

/** 椭圆环形检测。 nx=(x-cx)/rx, ny=(y-cy)/ry.
 *  e = nx²+ny² 在 [inner², outer²] 内。 */
static inline bool in_ellipse_ring(float nx, float ny, float inner, float outer) {
    float e = nx * nx + ny * ny;
    return e >= inner * inner && e <= outer * outer;
}

/** 角度区间检测（0°=右，逆时针）。start_deg/end_deg 可任意顺序。 */
static inline bool in_arc_angle(float dx, float dy, float start_deg, float end_deg) {
    float ang = atan2f(dy, dx);
    if (ang < 0.0f) ang += 2.0f * 3.14159265f;
    float s = start_deg * 3.14159265f / 180.0f;
    float e = end_deg * 3.14159265f / 180.0f;
    if (e < s) { float tmp = s; s = e; e = tmp; }
    return ang >= s && ang <= e;
}

/** 虚线模式：沿曲线距离 dist 处是否落在 dash 段内。 */
static inline bool dash_visible(float dist, float dash_px, float gap_px) {
    float period = dash_px + gap_px;
    float phase = _fmodf(dist, period);
    return phase < dash_px;
}

/** 波浪偏移：沿曲线距离 dist 处的正弦位移。 */
static inline float wave_offset(float dist, float amplitude, float freq, float phase) {
    return amplitude * sinf(dist * freq * 2.0f * 3.14159265f + phase);
}

/* ════════════════════════════════════════════════════════════════
 *  Circle / ring / arc — scanline drawing
 * ══════════════════════════════════════════════════════════════ */

/** 填充圆（scanline）。 */
static inline void fill_circle_scan(int y, int cx, int cy, float r,
                                    uint16_t color, uint16_t *buf, int screen_w) {
    float dy = (float)(y - cy);
    if (fabsf(dy) > r) return;
    float xspan = sqrtf(r * r - dy * dy);
    int x0 = cx - (int)xspan; if (x0 < 0) x0 = 0;
    int x1 = cx + (int)xspan; if (x1 >= screen_w) x1 = screen_w - 1;
    for (int x = x0; x <= x1; x++) buf[x] = color;
}

/** 圆环（轮廓圆）scanline 绘制。 */
static inline void draw_ring_scan(int y, int cx, int cy, float inner_r, float outer_r,
                                   uint16_t color, uint16_t *buf, int screen_w) {
    float dy = (float)(y - cy);
    if (fabsf(dy) > outer_r) return;
    float outer_span = sqrtf(outer_r * outer_r - dy * dy);
    float inner_span = (fabsf(dy) < inner_r) ? sqrtf(inner_r * inner_r - dy * dy) : 0.0f;
    int xl = cx - (int)outer_span; if (xl < 0) xl = 0;
    int xr = cx + (int)outer_span; if (xr >= screen_w) xr = screen_w - 1;
    int xil = cx - (int)inner_span;
    int xir = cx + (int)inner_span;
    for (int x = xl; x < xil; x++) if (x >= 0) buf[x] = color;
    for (int x = xir + 1; x <= xr; x++) if (x < screen_w) buf[x] = color;
}

/** 圆弧环（指定角度范围内）scanline 绘制。 */
static inline void draw_arc_ring_scan(int y, int cx, int cy, float inner_r, float outer_r,
                                       float start_deg, float end_deg,
                                       uint16_t color, uint16_t *buf, int screen_w) {
    float dy = (float)(y - cy);
    if (fabsf(dy) > outer_r) return;
    float outer_span = sqrtf(outer_r * outer_r - dy * dy);
    float inner_span = (fabsf(dy) < inner_r) ? sqrtf(inner_r * inner_r - dy * dy) : 0.0f;
    int xl = cx - (int)outer_span; if (xl < 0) xl = 0;
    int xr = cx + (int)outer_span; if (xr >= screen_w) xr = screen_w - 1;
    int xil = cx - (int)inner_span;
    int xir = cx + (int)inner_span;
    for (int x = xl; x < xil; x++)
        if (x >= 0 && in_arc_angle((float)(x - cx), dy, start_deg, end_deg)) buf[x] = color;
    for (int x = xir + 1; x <= xr; x++)
        if (x < screen_w && in_arc_angle((float)(x - cx), dy, start_deg, end_deg)) buf[x] = color;
}

/** 虚线圆环 scanline 绘制。 */
static inline void draw_dashed_ring_scan(int y, int cx, int cy, float r, float thick,
                                          float dash_len, float gap_len,
                                          uint16_t color, uint16_t *buf, int screen_w) {
    float dy = (float)(y - cy);
    float max_r = r + thick * 0.5f;
    if (fabsf(dy) > max_r) return;
    float outer_span = sqrtf(max_r * max_r - dy * dy);
    float inner_r = r - thick * 0.5f;
    float inner_span = (fabsf(dy) < inner_r) ? sqrtf(inner_r * inner_r - dy * dy) : 0.0f;
    int xl = cx - (int)outer_span; if (xl < 0) xl = 0;
    int xr = cx + (int)outer_span; if (xr >= screen_w) xr = screen_w - 1;
    int xil = cx - (int)inner_span;
    int xir = cx + (int)inner_span;
    // 用弧长做虚线判定
    float arc_step = (r > 0.0f) ? 1.0f / sqrtf(r * r - dy * dy + 0.001f) : 0.0f;
    float arc_base = atan2f(dy, (float)(xl - cx + 0.001f)) * r;
    for (int x = xl; x < xil; x++) {
        if (x < 0) continue;
        if (dash_visible(arc_base + (float)(x - xl) * arc_step, dash_len, gap_len))
            buf[x] = color;
    }
    for (int x = xir + 1; x <= xr; x++) {
        if (x >= screen_w) continue;
        if (dash_visible(arc_base + (float)(x - xl) * arc_step, dash_len, gap_len))
            buf[x] = color;
    }
}

/** 波浪圆环：半径按正弦调制。 */
static inline void draw_wavy_ring_scan(int y, int cx, int cy, float base_r, float thick,
                                        float amplitude, int waves,
                                        uint16_t color, uint16_t *buf, int screen_w) {
    float dy = (float)(y - cy);
    float max_r = base_r + amplitude + thick;
    if (fabsf(dy) > max_r) return;
    float outer_span = sqrtf(max_r * max_r - dy * dy);
    int xl = cx - (int)outer_span; if (xl < 0) xl = 0;
    int xr = cx + (int)outer_span; if (xr >= screen_w) xr = screen_w - 1;
    for (int x = xl; x <= xr; x++) {
        float ang = atan2f(dy, (float)(x - cx));
        float mod_r = base_r + sinf(ang * (float)waves) * amplitude;
        float d2 = dist_sq(x, y, cx, cy);
        if (in_ring_sq(d2, mod_r - thick * 0.5f, mod_r + thick * 0.5f)) buf[x] = color;
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Hand-drawn jitter + dithering helpers
 * ══════════════════════════════════════════════════════════════ */

/** Deterministic pseudo-random jitter for hand-drawn feel.
 *  Same (x,y) always returns same offset — no flicker across frames.
 *  Hash-based, no state, no division.
 *  Returns value in [-amplitude, +amplitude]. */
static inline float hand_jitter(int x, int y, float amplitude) {
    uint32_t h = (uint32_t)(x * 374761393 + y * 668265263 + 1274126177);
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    return ((float)(h & 0xffff) / 65535.0f - 0.5f) * 2.0f * amplitude;
}

/** Bayer 4x4 ordered dithering threshold.
 *  Returns true if pixel (x,y) should be drawn at the given opacity level [0,1].
 *  Uses 4x4 Bayer matrix for 17-level dither. No floating point in threshold. */
static inline bool bayer_accept(int x, int y, float level) {
    static const uint8_t bayer[4][4] = {
        { 0, 8, 2,10},
        {12, 4,14, 6},
        { 3,11, 1, 9},
        {15, 7,13, 5}
    };
    int threshold = (int)((1.0f - level) * 16.0f);
    return bayer[y & 3][x & 3] >= threshold;
}

#ifdef __cplusplus
}
#endif

#endif
