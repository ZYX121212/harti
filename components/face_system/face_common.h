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

/** 填充椭圆（scanline）。当 rx=ry 时等价于 fill_circle_scan。 */
static inline void fill_ellipse_scan(int y,
    int cx, int cy, float rx, float ry,
    uint16_t color, uint16_t *buf, int screen_w) {
    float dy = (float)(y - cy);
    if (fabsf(dy) > ry) return;
    float xspan = rx * sqrtf(1.0f - (dy * dy) / (ry * ry));
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
 *  Filled shapes — scanline drawing
 * ══════════════════════════════════════════════════════════════ */

/** 填充任意三角形（scanline）。
 *  用 edge-function 方法：对每行求与 3 条边的交点，填充 [x_min, x_max]。
 *  顶点顺序任意，自动处理顺时针/逆时针。 */
static inline void fill_triangle_scan(int y,
    float x0, float y0, float x1, float y1, float x2, float y2,
    uint16_t color, uint16_t *buf, int screen_w) {
    /* Sort vertices by y: v0 top, v1 middle, v2 bottom */
    if (y0 > y1) { float tx = x0; x0 = x1; x1 = tx; float ty = y0; y0 = y1; y1 = ty; }
    if (y1 > y2) { float tx = x1; x1 = x2; x2 = tx; float ty = y1; y1 = y2; y2 = ty; }
    if (y0 > y1) { float tx = x0; x0 = x1; x1 = tx; float ty = y0; y0 = y1; y1 = ty; }

    float fy = (float)y;
    if (fy < y0 || fy > y2) return;

    /* Compute x-intersections of row y with edges */
    float xs[4];
    int nx = 0;

    /* Edge v0→v1 */
    if (y1 > y0) {
        float t = (fy - y0) / (y1 - y0);
        if (t >= 0.0f && t <= 1.0f) xs[nx++] = x0 + t * (x1 - x0);
    }
    /* Edge v0→v2 */
    if (y2 > y0) {
        float t = (fy - y0) / (y2 - y0);
        if (t >= 0.0f && t <= 1.0f) xs[nx++] = x0 + t * (x2 - x0);
    }
    /* Edge v1→v2 */
    if (y2 > y1) {
        float t = (fy - y1) / (y2 - y1);
        if (t >= 0.0f && t <= 1.0f) xs[nx++] = x1 + t * (x2 - x1);
    }

    if (nx < 2) return;

    /* Find min and max x among intersections */
    float x_min = xs[0], x_max = xs[0];
    for (int i = 1; i < nx; i++) {
        if (xs[i] < x_min) x_min = xs[i];
        if (xs[i] > x_max) x_max = xs[i];
    }

    /* Fill span */
    int xl = (int)x_min; if (xl < 0) xl = 0;
    int xr = (int)x_max; if (xr >= screen_w) xr = screen_w - 1;
    for (int x = xl; x <= xr; x++) buf[x] = color;
}

/** 填充正 N 边形（scanline）。
 *  n_sides 限制在 [3, 8]，rotation_deg 控制旋转角度（0° = 顶点朝上）。 */
static inline void fill_ngon_scan(int y,
    int cx, int cy, float radius, int n_sides, float rotation_deg,
    uint16_t color, uint16_t *buf, int screen_w) {
    if (n_sides < 3) n_sides = 3;
    if (n_sides > 8) n_sides = 8;

    float dy = (float)(y - cy);
    if (fabsf(dy) > radius) return;

    /* Precompute vertices (stack array, 8×2 floats) */
    float vx[8], vy[8];
    float angle_step = 2.0f * 3.14159265f / (float)n_sides;
    float base_angle = rotation_deg * 3.14159265f / 180.0f;
    for (int i = 0; i < n_sides; i++) {
        float a = base_angle + angle_step * (float)i;
        vx[i] = (float)cx + cosf(a) * radius;
        vy[i] = (float)cy + sinf(a) * radius;
    }

    float fy = (float)y;

    /* Find x-intersections of row y with all edges */
    float x_min = (float)screen_w, x_max = -1.0f;
    int found = 0;

    for (int i = 0; i < n_sides; i++) {
        int j = (i + 1) % n_sides;
        float ey0 = vy[i], ey1 = vy[j];
        if ((fy < ey0 && fy < ey1) || (fy > ey0 && fy > ey1)) continue;
        if (ey1 == ey0) continue;
        float t = (fy - ey0) / (ey1 - ey0);
        float ix = vx[i] + t * (vx[j] - vx[i]);
        if (ix < x_min) x_min = ix;
        if (ix > x_max) x_max = ix;
        found++;
    }

    if (found < 2) return;

    int xl = (int)x_min; if (xl < 0) xl = 0;
    int xr = (int)x_max; if (xr >= screen_w) xr = screen_w - 1;
    for (int x = xl; x <= xr; x++) buf[x] = color;
}

/** 填充圆角矩形（scanline）。
 *  corner_r 为四角倒角半径，left < right, top < bottom。 */
static inline void fill_rounded_rect_scan(int y,
    float left, float top, float right, float bottom, float corner_r,
    uint16_t color, uint16_t *buf, int screen_w) {
    float fy = (float)y;
    if (fy < top || fy > bottom) return;

    /* Clamp corner radius to fit */
    float max_cr = (right - left) * 0.5f;
    if (corner_r > max_cr) corner_r = max_cr;
    float max_cr_y = (bottom - top) * 0.5f;
    if (corner_r > max_cr_y) corner_r = max_cr_y;

    float cr = corner_r;
    float cr_sq = cr * cr;

    /* Determine x bounds for this row */
    float xs, xe;

    if (fy >= top + cr && fy <= bottom - cr) {
        /* Flat middle region: full width */
        xs = left;
        xe = right;
    } else {
        /* Corner region: inset by circle chord */
        float corner_y;
        if (fy < top + cr) corner_y = top + cr;
        else               corner_y = bottom - cr;

        float dy_c = fy - corner_y;
        float chord = sqrtf(cr_sq - dy_c * dy_c);
        xs = left + cr - chord;
        xe = right - cr + chord;
    }

    int xl = (int)xs; if (xl < 0) xl = 0;
    int xr = (int)xe; if (xr >= screen_w) xr = screen_w - 1;
    for (int x = xl; x <= xr; x++) buf[x] = color;
}

/** 通用凹多边形 even-odd 填充（scanline）。
 *  verts = [x0,y0, x1,y1, ...], n_verts ≤ 12（stack 分配）。
 *  对每行求所有边交点 → 排序 → 交替填充。支持自相交多边形。 */
static inline void fill_polygon_evenodd_scan(int y,
    const float *verts, int n_verts,
    uint16_t color, uint16_t *buf, int screen_w) {
    if (n_verts < 3 || n_verts > 12) return;

    float fy = (float)y;
    float xs[12];
    int nx = 0;

    for (int i = 0; i < n_verts; i++) {
        int j = (i + 1) % n_verts;
        float ey0 = verts[i * 2 + 1];
        float ey1 = verts[j * 2 + 1];
        if ((fy < ey0 && fy < ey1) || (fy > ey0 && fy > ey1)) continue;
        if (ey1 == ey0) continue;
        float t = (fy - ey0) / (ey1 - ey0);
        xs[nx++] = verts[i * 2] + t * (verts[j * 2] - verts[i * 2]);
    }

    if (nx < 2) return;

    /* Insertion sort intersections (small array, typically 2-6) */
    for (int i = 1; i < nx; i++) {
        float key = xs[i];
        int k = i - 1;
        while (k >= 0 && xs[k] > key) { xs[k + 1] = xs[k]; k--; }
        xs[k + 1] = key;
    }

    /* Fill alternate spans (even-odd rule) */
    for (int i = 0; i < nx - 1; i += 2) {
        int xl = (int)xs[i];     if (xl < 0) xl = 0;
        int xr = (int)xs[i + 1]; if (xr >= screen_w) xr = screen_w - 1;
        for (int x = xl; x <= xr; x++) buf[x] = color;
    }
}

/** 填充心形（scanline）。组合 2 圆 + 1 三角形。 */
static inline void fill_heart_scan(int y,
    int cx, int cy, float size,
    uint16_t color, uint16_t *buf, int screen_w) {
    float r = size * 0.5f;
    float lobe_off = r * 0.55f;
    /* Left lobe */
    fill_circle_scan(y, (int)(cx - lobe_off), (int)(cy - r * 0.3f), r, color, buf, screen_w);
    /* Right lobe */
    fill_circle_scan(y, (int)(cx + lobe_off), (int)(cy - r * 0.3f), r, color, buf, screen_w);
    /* Bottom triangle */
    fill_triangle_scan(y,
        cx - r * 1.05f, cy + r * 0.15f,
        cx + r * 1.05f, cy + r * 0.15f,
        (float)cx,        cy - r * 1.1f,
        color, buf, screen_w);
}

/** 填充星形（scanline）。n_points 角星，outer_r/inner_r 交替半径。
 *  生成 2*n_points 个顶点追踪星形轮廓，然后用 even-odd 填充。 */
static inline void fill_star_scan(int y,
    int cx, int cy, float outer_r, float inner_r, int n_points, float rotation_deg,
    uint16_t color, uint16_t *buf, int screen_w) {
    if (n_points < 3) n_points = 3;
    if (n_points > 6) n_points = 6;
    int nv = n_points * 2;

    float verts[12]; /* max 6*2*2 = 24 floats → clamp n_points≤6 → 12 floats */
    float angle_step = 3.14159265f / (float)n_points;
    float base = rotation_deg * 3.14159265f / 180.0f - 3.14159265f * 0.5f;

    for (int i = 0; i < nv; i++) {
        float a = base + angle_step * (float)i;
        float r = (i & 1) ? inner_r : outer_r;
        verts[i * 2]     = (float)cx + cosf(a) * r;
        verts[i * 2 + 1] = (float)cy + sinf(a) * r;
    }

    fill_polygon_evenodd_scan(y, verts, nv, color, buf, screen_w);
}

/** 填充泪滴/水滴形（scanline）。上半圆 + 下半三角收敛到尖点。 */
static inline void fill_teardrop_scan(int y,
    int cx, int cy, float size,
    uint16_t color, uint16_t *buf, int screen_w) {
    float r = size * 0.45f;
    /* Upper circle */
    fill_circle_scan(y, cx, (int)(cy - r * 0.25f), r, color, buf, screen_w);
    /* Lower triangle → point */
    fill_triangle_scan(y,
        (float)cx - r,      cy,
        (float)cx + r,      cy,
        (float)cx,          cy + size * 1.05f,
        color, buf, screen_w);
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

/** Teacup with steam: trapezoid cup body + elliptical rim + handle arc + 3 wavy steam lines. */
static inline void draw_teacup_steam_scan(int y, float cx, float cy, float size,
					   uint16_t color, uint16_t *buf, int screen_w) {
	float rx = size * 0.48f, ry = size * 0.28f;
	float cup_top = cy - ry;
	float cup_bot = cy + ry * 1.4f;
	float cup_rx_top = rx;
	float cup_rx_bot = rx * 0.78f;

	/* Cup body: filled trapezoid scanline */
	if (y >= (int)cup_top && y <= (int)cup_bot) {
		float t = (float)(y - cup_top) / (cup_bot - cup_top);
		float span = cup_rx_top + (cup_rx_bot - cup_rx_top) * t;
		int xl = (int)(cx - span); if (xl < 0) xl = 0;
		int xr = (int)(cx + span); if (xr >= screen_w) xr = screen_w - 1;
		if (y > (int)cup_top + 2) {
			for (int x = xl; x <= xr; x++) buf[x] = color;
		}
	}

	/* Rim ellipse */
	float nydy = (float)(y - cup_top) / (ry * 0.28f);
	if (fabsf(nydy) < 1.0f) {
		float span = rx * sqrtf(1.0f - nydy * nydy);
		int xl = (int)(cx - span); if (xl < 0) xl = 0;
		int xr = (int)(cx + span); if (xr >= screen_w) xr = screen_w - 1;
		for (int x = xl; x <= xr; x++) buf[x] = color;
	}

	/* Handle on right side */
	float h0 = cup_top + ry * 0.6f, h1 = cup_bot - ry * 0.2f;
	if (y >= (int)h0 && y <= (int)h1) {
		float ht = (float)(y - h0) / (h1 - h0);
		float hx = cx + cup_rx_top + sinf(ht * 3.14159265f) * size * 0.12f;
		for (int dx = -1; dx <= 1; dx++) {
			int px = (int)(hx) + dx;
			if (px >= 0 && px < screen_w) buf[px] = color;
		}
	}

	/* 3 wavy steam lines */
	float steam_y0 = cup_top - size * 0.1f;
	for (int s = 0; s < 3; s++) {
		float sx0 = cx - size * 0.12f + s * size * 0.12f;
		float freq = 0.08f + s * 0.02f;
		for (int dy = 0; dy < (int)(size * 0.8f); dy++) {
			int sy = (int)(steam_y0 - dy);
			if (y != sy) continue;
			float sway = sinf(dy * freq) * size * 0.06f;
			int px = (int)(sx0 + sway);
			if (px >= 0 && px < screen_w) buf[px] = color;
			if (px + 1 < screen_w) buf[px + 1] = color;
		}
	}
}

/** Double music note: two note heads (ellipses) + stems + flags. */
static inline void draw_music_note_scan(int y, float cx, float cy, float size,
					 uint16_t color, uint16_t *buf, int screen_w) {
	/* Primary note head */
	float n1_cx = cx - size * 0.2f, n1_cy = cy + size * 0.2f;
	float n1_rx = size * 0.32f, n1_ry = size * 0.2f;
	float ndy1 = (float)(y - n1_cy) / n1_ry;
	if (fabsf(ndy1) < 1.0f) {
		float span = n1_rx * sqrtf(1.0f - ndy1 * ndy1);
		int xl = (int)(n1_cx - span); if (xl < 0) xl = 0;
		int xr = (int)(n1_cx + span); if (xr >= screen_w) xr = screen_w - 1;
		for (int x = xl; x <= xr; x++) buf[x] = color;
	}

	/* Primary stem */
	int stem_x = (int)(n1_cx + n1_rx);
	int stem_top = (int)(n1_cy - size * 0.9f);
	int stem_bot = (int)(n1_cy);
	if (y >= stem_top && y <= stem_bot) {
		int px = stem_x;
		if (px >= 0 && px < screen_w) buf[px] = color;
		if (px + 1 < screen_w) buf[px + 1] = color;
	}

	/* Primary flag */
	if (y >= stem_top && y <= stem_top + (int)(size * 0.5f)) {
		float ft = (float)(y - stem_top) / (size * 0.5f);
		float fy_curve = stem_top + (1.0f - ft) * (1.0f - ft) * size * 0.1f;
		if (fabsf(y - fy_curve - stem_top) < 2.0f || fabsf(y - (stem_top + ft * size * 0.5f)) < 1.5f) {
			for (int dx = 0; dx <= (int)(ft * size * 0.4f); dx++) {
				int px = stem_x + dx;
				if (px >= 0 && px < screen_w) buf[px] = color;
			}
		}
	}

	/* Secondary note head (smaller, offset right-down) */
	float n2_cx = cx + size * 0.45f, n2_cy = cy + size * 0.1f;
	float n2_rx = size * 0.22f, n2_ry = size * 0.14f;
	float ndy2 = (float)(y - n2_cy) / n2_ry;
	if (fabsf(ndy2) < 1.0f) {
		float span = n2_rx * sqrtf(1.0f - ndy2 * ndy2);
		int xl = (int)(n2_cx - span); if (xl < 0) xl = 0;
		int xr = (int)(n2_cx + span); if (xr >= screen_w) xr = screen_w - 1;
		for (int x = xl; x <= xr; x++) buf[x] = color;
	}

	/* Secondary stem */
	int s2_x = (int)(n2_cx + n2_rx);
	int s2_top = (int)(n2_cy - size * 0.7f);
	if (y >= s2_top && y <= (int)n2_cy) {
		if (s2_x >= 0 && s2_x < screen_w) buf[s2_x] = color;
		if (s2_x + 1 < screen_w) buf[s2_x + 1] = color;
	}

	/* Secondary flag */
	if (y >= s2_top && y <= s2_top + (int)(size * 0.35f)) {
		float ft = (float)(y - s2_top) / (size * 0.35f);
		for (int dx = 0; dx <= (int)(ft * size * 0.35f); dx++) {
			int px = s2_x + dx;
			if (px >= 0 && px < screen_w) buf[px] = color;
		}
	}
}

/** Sunglasses: two rounded rectangle lenses + bridge + temples. */
static inline void draw_sunglasses_scan(int y, float cx, float cy, float size,
					 uint16_t color, uint16_t *buf, int screen_w) {
	float lens_w = size * 0.5f, lens_h = size * 0.35f;
	float gap = size * 0.08f;
	float lens_cy = cy;

	/* Left lens */
	float lx = cx - lens_w - gap, ly = lens_cy - lens_h * 0.5f;
	if (y >= (int)ly && y <= (int)(ly + lens_h)) {
		int xl = (int)lx, xr = (int)(lx + lens_w);
		if (xl < 0) xl = 0;
		if (xr >= screen_w) xr = screen_w - 1;
		for (int x = xl; x <= xr; x++) buf[x] = color;
	}

	/* Right lens */
	float rx = cx + gap, ry2 = lens_cy - lens_h * 0.5f;
	if (y >= (int)ry2 && y <= (int)(ry2 + lens_h)) {
		int xl = (int)rx, xr = (int)(rx + lens_w);
		if (xl < 0) xl = 0;
		if (xr >= screen_w) xr = screen_w - 1;
		for (int x = xl; x <= xr; x++) buf[x] = color;
	}

	/* Bridge horizontal line */
	int bridge_y = (int)(lens_cy);
	if (y == bridge_y || y == bridge_y - 1 || y == bridge_y + 1) {
		int bl = (int)(lx + lens_w), br = (int)rx;
		if (bl < 0) bl = 0;
		if (br >= screen_w) br = screen_w - 1;
		for (int x = bl; x <= br; x++) buf[x] = color;
	}

	/* Temples */
	int temple_y = (int)(lens_cy - lens_h * 0.2f);
	if (y == temple_y || y == temple_y - 1) {
		/* Left temple */
		int tl = (int)(lx - size * 0.22f), tr2 = (int)lx;
		if (tl < 0) tl = 0;
		for (int x = tl; x <= tr2; x++) buf[x] = color;
		/* Right temple */
		int rl = (int)(rx + lens_w), rr = (int)(rx + lens_w + size * 0.22f);
		if (rr >= screen_w) rr = screen_w - 1;
		for (int x = rl; x <= rr; x++) buf[x] = color;
	}
}


/** Finger heart: vector outline of a finger-heart gesture.
 *  Draws a clean heart shape (parametric curve) plus two thumb lines at the bottom,
 *  all as thin white strokes with no background fill. */
static inline void draw_finger_heart_scan(int y, float cx, float cy, float size,
                                          uint16_t color, uint16_t *buf, int screen_w) {
    float thick = 2.5f;
    float scale = size / 16.0f;

    /* ── Heart outline via parametric equation ───────────────── */
    int y0 = (int)(cy - size * 0.95f);
    int y1 = (int)(cy + size * 0.65f);

    if (y >= y0 && y <= y1) {
        float prev_px = 0.0f, prev_py = 0.0f;
        bool has_prev = false;

        for (int i = 0; i <= 360; i++) {
            float t = (float)i * 0.0174533f;  /* i * π/180 */
            float st = sinf(t);
            float hx = 16.0f * st * st * st;
            float hy = -(13.0f * cosf(t) - 5.0f * cosf(2.0f * t)
                       - 2.0f * cosf(3.0f * t) - cosf(4.0f * t));

            float px = cx + hx * scale;
            float py = cy + hy * scale * 0.78f;

            if (has_prev) {
                float ymin = prev_py < py ? prev_py : py;
                float ymax = prev_py > py ? prev_py : py;
                if ((float)y >= ymin - thick && (float)y <= ymax + thick) {
                    float denom = py - prev_py;
                    float t_seg = (fabsf(denom) > 0.001f) ? ((float)y - prev_py) / denom : 0.5f;
                    if (t_seg >= -0.1f && t_seg <= 1.1f) {
                        float ix = prev_px + (px - prev_px) * t_seg;
                        int xi = (int)(ix + 0.5f);
                        for (int d = -1; d <= 1; d++) {
                            int xd = xi + d;
                            if (xd >= 0 && xd < screen_w) buf[xd] = color;
                        }
                    }
                }
            }
            prev_px = px;
            prev_py = py;
            has_prev = true;
        }
    }

    /* ── Thumb lines below the heart ─────────────────────────── */
    float thumb_top = cy + size * 0.38f;
    float thumb_bot = cy + size * 0.62f;
    if (y >= (int)thumb_top && y <= (int)thumb_bot) {
        float t = ((float)y - thumb_top) / (thumb_bot - thumb_top);
        float lx = cx + t * (-size * 0.18f);
        float rx = cx + t * (size * 0.18f);
        for (int d = -1; d <= 1; d++) {
            int xl = (int)(lx + 0.5f) + d;
            int xr = (int)(rx + 0.5f) + d;
            if (xl >= 0 && xl < screen_w) buf[xl] = color;
            if (xr >= 0 && xr < screen_w) buf[xr] = color;
        }
    }
}

/* 萌系亮眼瞳孔：大实心瞳 + 双高光星光。逐扫描线在某只眼的 x 范围内调用。
   (pcx,pcy)=瞳孔中心(屏幕px)，pr=瞳孔半径，shine=0..1 星光强度。
   pupil_col=瞳孔填充色，sparkle_col=星光色（常规风格传 黑瞳/白星，nova 反相）。*/
static inline void draw_kawaii_pupil(int y, int x_start, int x_end,
                                     float pcx, float pcy, float pr, float shine,
                                     uint16_t pupil_col, uint16_t sparkle_col,
                                     uint16_t *buf) {
    if (pr < 1.0f) return;
    float pr_sq = pr * pr;
    float cl1_r  = pr * 0.30f;
    float cl1x   = pcx - pr * 0.32f, cl1y = pcy - pr * 0.34f;
    float cl1_sq = cl1_r * cl1_r;
    float cl2_r  = pr * 0.14f;
    float cl2x   = pcx + pr * 0.26f, cl2y = pcy + pr * 0.24f;
    float cl2_sq = cl2_r * cl2_r;
    for (int x = x_start; x <= x_end; x++) {
        float dx = (float)x - pcx, dy = (float)y - pcy;
        if (dx * dx + dy * dy >= pr_sq) continue;
        float s1x = (float)x - cl1x, s1y = (float)y - cl1y;
        float s2x = (float)x - cl2x, s2y = (float)y - cl2y;
        bool sp1 = (shine > 0.1f && s1x * s1x + s1y * s1y < cl1_sq);
        bool sp2 = (shine > 0.1f && s2x * s2x + s2y * s2y < cl2_sq);
        buf[x] = (sp1 || sp2) ? sparkle_col : pupil_col;
    }
}

#ifdef __cplusplus
}
#endif

#endif
