#include "sprite_cat.h"
#include "face_palette.h"
#include <math.h>
#include <stdlib.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120
#define BG_GRADIENT_MAX 171

/* ── Utility: dist squared ──────────────────────────────────── */
static inline float dist_sq(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

/* ── Utility: blend two RGB565 colors ────────────────────────── */
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

/* ── Background gradient ────────────────────────────────────── */
static uint16_t bg_lut[BG_GRADIENT_MAX];
static const uint16_t *active_pal = NULL;

static void build_bg_lut(const uint16_t *pal) {
    for (int d = 0; d < BG_GRADIENT_MAX; d++) {
        float t = d / 160.0f;
        if (t > 1.0f) t = 1.0f;
        bg_lut[d] = blend_colors(pal[PAL_BG], pal[PAL_BG_EDGE], t);
    }
}

static void draw_face(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    if (active_pal != sp->pal) {
        build_bg_lut(sp->pal);
        active_pal = sp->pal;
    }
    int dy = y - CENTER_Y;
    int dy_sq = dy * dy;
    for (int x = 0; x < SCREEN_W; x++) {
        int dx = x - CENTER_X;
        int d_sq = dx * dx + dy_sq;
        int d = (int)sqrtf((float)d_sq);
        if (d >= BG_GRADIENT_MAX) d = BG_GRADIENT_MAX - 1;
        buf[x] = bg_lut[d];
    }
}

/* ── Cat eye: vertical slit pupil ──────────────────────────── */
static void draw_eye_impl(int y, const eye_params_t *ep, int eye_cx, int eye_cy,
                          const uint16_t *pal, uint16_t *buf) {
    const float eye_r = 32.0f;
    const float iris_r = 26.0f;
    const float pupil_base_r = 10.0f;

    float fy = y - eye_cy;
    if (fy < -eye_r - 2 || fy > eye_r + 2) return;

    int x_start = eye_cx - (int)eye_r - 2;
    int x_end   = eye_cx + (int)eye_r + 2;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    float iris_cx = ep->iris_center.dx * 7.0f;
    float iris_cy = ep->iris_center.dy * 7.0f;
    float iris_r_sq = iris_r * iris_r;
    // pupil_scale: 0 = thin slit, 1 = round
    float slit_width = pupil_base_r * (0.15f + ep->pupil_scale * 0.85f);

    float lid_open = 1.0f - (ep->top_lid_mid.dy * 1.4f);
    if (lid_open < 0.05f) lid_open = 0.05f;
    if (lid_open > 1.0f) lid_open = 1.0f;

    float base_top = -eye_r * (1.0f - lid_open);
    float base_bot =  eye_r * (1.0f - lid_open);

    for (int x = x_start; x <= x_end; x++) {
        float fx = x - eye_cx;
        float r_sq = fx * fx + fy * fy;
        if (r_sq >= eye_r * eye_r) continue;

        float arc = 1.0f - (fx * fx) / (eye_r * eye_r);
        float inner_adj = ep->inner_corner.dy * 5.0f;
        float outer_adj = ep->outer_corner.dy * 5.0f;
        float corner_adj = inner_adj + (outer_adj - inner_adj) * ((fx + eye_r) / (2.0f * eye_r));
        float top_lid = base_top + (5.0f * lid_open * arc) + corner_adj * 0.5f;
        float bot_lid = base_bot - (3.0f * lid_open * arc) - corner_adj * 0.3f;
        if (fy < top_lid || fy > bot_lid) continue;

        float iris_d_sq = dist_sq(fx, fy, iris_cx, iris_cy);
        // Slit pupil: vertically elongated diamond
        float dx_abs = fabsf(fx - iris_cx);
        float dy_abs = fabsf(fy - iris_cy);
        float slit_dist = dx_abs / slit_width + dy_abs / (slit_width * 3.5f);

        if (iris_d_sq < iris_r_sq) {
            if (slit_dist < 0.9f) {
                buf[x] = pal[PAL_PUPIL];
            } else if (slit_dist < 1.0f) {
                buf[x] = blend_colors(pal[PAL_PUPIL], pal[PAL_IRIS], (slit_dist - 0.9f) * 10.0f);
            } else {
                float grad_t = sqrtf(iris_d_sq) / iris_r;
                uint16_t dark = blend_colors(pal[PAL_IRIS], pal[PAL_PUPIL], 0.5f);
                buf[x] = blend_colors(pal[PAL_IRIS], dark, grad_t * grad_t);
            }
        }
    }
}

static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X - (int)sp->eye_half_spacing;
    draw_eye_impl(y, &st->eye[0], cx, CENTER_Y, sp->pal, buf);
}
static void draw_eye_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X + (int)sp->eye_half_spacing;
    draw_eye_impl(y, &st->eye[1], cx, CENTER_Y, sp->pal, buf);
}

/* ── Cat brows: thin arcs (reuse bezier with cat palette) ─── */
static void draw_brow_impl(int y, const brow_params_t *bp, int eye_cx, int eye_cy,
                           const uint16_t *pal, float brow_y_offset, uint16_t *buf) {
    float brow_y_px = eye_cy + brow_y_offset;
    float dy = y - brow_y_px;
    float base_width = bp->thickness * 3.0f;
    float inner_mult = 1.0f - bp->taper * 0.3f;
    float tail_mult  = 1.0f - bp->taper * 0.6f;

    float inner_x = eye_cx + bp->inner.dx * 25.0f;
    float inner_y = brow_y_px + bp->inner.dy * 15.0f;
    float arch_y  = brow_y_px + bp->arch.dy * 20.0f;
    float tail_x  = eye_cx + bp->tail.dx * 30.0f;
    float tail_y  = brow_y_px + bp->tail.dy * 15.0f;

    int x_start = (int)inner_x - 4;
    int x_end   = (int)tail_x + 4;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    for (int x = x_start; x <= x_end; x++) {
        float t = (float)(x - inner_x) / (tail_x - inner_x + 0.001f);
        if (t < 0.0f || t > 1.0f) continue;
        float curve_y = (1-t)*(1-t)*inner_y + 2*(1-t)*t*arch_y + t*t*tail_y;
        float dist = fabsf(y - curve_y);
        float width_mult = inner_mult + (tail_mult - inner_mult) * t;
        float half_thick = base_width * width_mult;
        if (dist < half_thick) {
            float alpha = (half_thick - dist) / half_thick * 0.75f;
            buf[x] = blend_colors(buf[x], pal[PAL_BROW], alpha);
        }
    }
}

static void draw_brow_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X - (int)sp->eye_half_spacing;
    draw_brow_impl(y, &st->brow[0], cx, CENTER_Y, sp->pal, sp->brow_y_offset, buf);
}
static void draw_brow_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X + (int)sp->eye_half_spacing;
    draw_brow_impl(y, &st->brow[1], cx, CENTER_Y, sp->pal, sp->brow_y_offset, buf);
}

/* ── Cat mouth: ω shape ───────────────────────────────────── */
static void draw_mouth(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const mouth_params_t *mp = &st->mouth;
    int mouth_cy = CENTER_Y + (int)sp->mouth_y_center;
    const uint16_t *pal = sp->pal;
    float half_width = 22.0f;

    int x_start = CENTER_X - (int)half_width - 3;
    int x_end   = CENTER_X + (int)half_width + 3;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    float lcx = CENTER_X + mp->left_corner.dx * half_width;
    float rcx = CENTER_X + mp->right_corner.dx * half_width;
    float omega_rise = 7.0f; // center upward bump

    for (int x = x_start; x <= x_end; x++) {
        if (x < lcx - 2 || x > rcx + 2) continue;
        float t = (x - lcx) / (rcx - lcx + 0.001f);
        if (t < 0.0f || t > 1.0f) continue;

        // Omega shape: three-point curve (corner → omega dip → center rise)
        float corner_y = mouth_cy + mp->left_corner.dy * 8.0f * (1-t) + mp->right_corner.dy * 8.0f * t;
        float omega_dip = mouth_cy + 3.0f;
        float omega_top = mouth_cy - omega_rise;
        float upper_y;
        if (t < 0.5f) {
            float tt = t * 2.0f;
            upper_y = (1-tt)*(1-tt)*corner_y + 2*(1-tt)*tt*omega_dip + tt*tt*omega_top;
        } else {
            float tt = (t - 0.5f) * 2.0f;
            upper_y = (1-tt)*(1-tt)*omega_top + 2*(1-tt)*tt*omega_dip + tt*tt*corner_y;
        }
        float lower_y = mouth_cy + mp->openness * 8.0f;

        if (y >= upper_y - 1.0f && y <= lower_y + 1.0f) {
            if (y > upper_y + 1.0f && y < lower_y - 1.0f && mp->openness > 0.05f) {
                if (mp->openness > 0.2f) {
                    buf[x] = blend_colors(buf[x], pal[PAL_TONGUE], 0.8f);
                } else {
                    buf[x] = blend_colors(buf[x], pal[PAL_PUPIL], 0.6f);
                }
            } else {
                buf[x] = blend_colors(buf[x], pal[PAL_MOUTH], 0.7f);
            }
        }
    }
}

/* ── Cat blush ─────────────────────────────────────────────── */
static void draw_blush(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    float level = st->decor.blush;
    if (level <= 0) return;
    const uint16_t *pal = sp->pal;
    int blush_cy = CENTER_Y + (int)sp->blush_y_offset;
    float blush_r = 20.0f;
    float r_sq = blush_r * blush_r;
    float dy = y - blush_cy;
    if (fabsf(dy) > blush_r) return;

    int left_cx = CENTER_X - 58, right_cx = CENTER_X + 58;
    int x_start = left_cx - (int)blush_r - 1;
    int x_end = right_cx + (int)blush_r + 1;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    for (int x = x_start; x <= x_end; x++) {
        float dl = dist_sq(x, y, left_cx, blush_cy);
        float dr = dist_sq(x, y, right_cx, blush_cy);
        if (dl < r_sq || dr < r_sq) {
            float d = (dl < dr) ? dl : dr;
            float t = (1.0f - d / r_sq) * level * 0.6f;
            buf[x] = blend_colors(buf[x], pal[PAL_BLUSH], t);
        }
    }
}

/* ── Cat whiskers + decor overlay ──────────────────────────── */
static void draw_decor_overlay(int y, const face_state_t *st,
                                const sprite_set_t *sp, uint16_t *buf) {
    const decor_params_t *dp = &st->decor;
    const uint16_t *pal = sp->pal;

    // Cat ears (triangular above eyes)
    float ear_size = 28.0f;
    int left_cx = CENTER_X - (int)sp->eye_half_spacing - 5;
    int left_base_y = CENTER_Y + (int)sp->brow_y_offset - 12;
    int right_cx = CENTER_X + (int)sp->eye_half_spacing + 5;

    int ear_x_start = left_cx - (int)ear_size - 2;
    int ear_x_end = right_cx + (int)ear_size + 2;
    if (ear_x_start < 0) ear_x_start = 0;
    if (ear_x_end >= SCREEN_W) ear_x_end = SCREEN_W - 1;

    for (int x = ear_x_start; x <= ear_x_end; x++) {
        int ear_cx = (x < CENTER_X) ? left_cx : right_cx;
        float fx = (float)(x - ear_cx);
        float fy = (float)(y - left_base_y);
        float tip_y = -ear_size * 1.3f;
        float half_w = ear_size * (1.0f - fy / tip_y);
        if (fy < tip_y || fy > 0) continue;
        if (fabsf(fx) < half_w * 0.95f) {
            float edge = half_w - fabsf(fx);
            float alpha = (edge < 3.0f) ? edge / 3.0f * 0.8f : 0.8f;
            buf[x] = blend_colors(buf[x], pal[PAL_SKIN], alpha);
        }
        if (fabsf(fx) < half_w * 0.5f && fy > tip_y * 0.5f) {
            buf[x] = blend_colors(buf[x], pal[PAL_BLUSH], 0.5f);
        }
    }

    // Whiskers (always visible, subtly)
    int whisker_cy = CENTER_Y + 25;
    if (y >= whisker_cy - 8 && y <= whisker_cy + 8) {
        for (int side = 0; side < 2; side++) {
            int base_x = side ? CENTER_X + 35 : CENTER_X - 35;
            for (int w = 0; w < 3; w++) {
                float wy = (float)(whisker_cy - 3 + w * 3);
                if (fabsf(y - wy) > 1.0f) continue;
                int wx_start = side ? base_x : base_x - 22;
                int wx_end   = side ? base_x + 22 : base_x;
                float angle = (w - 1) * 0.25f;
                // Draw curved whisker lines
                for (int x = wx_start; x <= wx_end; x++) {
                    if (x < 0 || x >= SCREEN_W) continue;
                    float wy_curve = wy + sinf((float)(x - base_x) * 0.15f) * 3.0f;
                    if (fabsf(y - wy_curve) < 1.5f && abs(x - base_x) > 4) {
                        buf[x] = blend_colors(buf[x], pal[PAL_BROW], 0.4f);
                    }
                }
            }
        }
    }

    // Tears
    if (dp->tears > 0) {
        // Simplified tears below eyes
    }

    // Stars
    if (dp->stars > 0) {
        static const int pos[3][2] = {{CENTER_X, CENTER_Y - 80}, {CENTER_X - 50, CENTER_Y - 60}, {CENTER_X + 50, CENTER_Y - 60}};
        for (int i = 0; i < 3; i++) {
            float d_sq = dist_sq(CENTER_X, y, pos[i][0], pos[i][1]);
            if (d_sq < 16.0f) {
                int sx = pos[i][0], x_s = sx - 4, x_e = sx + 4;
                if (x_s < 0) x_s = 0;
                if (x_e >= SCREEN_W) x_e = SCREEN_W - 1;
                for (int x = x_s; x <= x_e; x++) {
                    float sd = dist_sq(x, y, sx, pos[i][1]);
                    if (sd < 12.0f)
                        buf[x] = blend_colors(buf[x], pal[PAL_STAR], dp->stars * 0.5f);
                }
            }
        }
    }

    // Sparkle
    if (dp->sparkle > 0) {
        for (int i = 0; i < 4; i++) {
            int sx = CENTER_X + (i % 2 ? 40 : -40);
            int sy = CENTER_Y - 35 + (i / 2) * 20;
            if (y < sy - 3 || y > sy + 3) continue;
            for (int x = sx - 3; x <= sx + 3; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                float sd = dist_sq(x, y, sx, sy);
                if (sd < 6.0f)
                    buf[x] = blend_colors(buf[x], pal[PAL_SHINE], dp->sparkle * 0.3f);
            }
        }
    }
}

/* ── Sprite definition ──────────────────────────────────────── */
const sprite_set_t SPRITE_CAT = {
    .name = "cat",
    .eye_radius = 32.0f,
    .eye_half_spacing = 24.0f,
    .mouth_y_center = 48.0f,
    .brow_y_offset = -34.0f,
    .blush_y_offset = 32.0f,
    .draw_face = draw_face,
    .draw_blush = draw_blush,
    .draw_mouth = draw_mouth,
    .draw_eye_left = draw_eye_left,
    .draw_eye_right = draw_eye_right,
    .draw_brow_left = draw_brow_left,
    .draw_brow_right = draw_brow_right,
    .draw_decor_overlay = draw_decor_overlay,
    .pal = PALETTE_CAT,
};
