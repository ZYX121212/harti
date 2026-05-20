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
    // Lazy-build the background gradient LUT when palette changes
    if (active_pal != sp->pal) {
        build_bg_lut(sp->pal);
        active_pal = sp->pal;
    }
    int dy = y - CENTER_Y;
    int dy_sq = dy * dy;
    float roundness = st->face.roundness;
    for (int x = 0; x < SCREEN_W; x++) {
        int dx = x - CENTER_X;
        float manhattan = fabsf((float)dx) + fabsf((float)dy);
        float euclidean = sqrtf((float)(dx * dx + dy_sq));
        // r=0 → diamond (manhattan), r=0.5 → circle (euclidean), r=1 → circle
        float mixed;
        if (roundness <= 0.5f) {
            mixed = manhattan + (euclidean - manhattan) * (roundness * 2.0f);
        } else {
            mixed = euclidean;
        }
        if (mixed < 0.0f) mixed = 0.0f;
        int d = (int)mixed;
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

    float iris_cx = ep->iris_center.dx * 7.0f;
    float iris_cy = ep->iris_center.dy * 7.0f;
    float iris_r_sq = iris_r * iris_r;
    float pupil_r = pupil_base_r * ep->pupil_scale;
    float pupil_r_sq = pupil_r * pupil_r;

    float lid_open = 1.0f - (ep->top_lid_mid.dy * 1.4f);
    if (lid_open < 0.05f) lid_open = 0.05f;
    if (lid_open > 1.0f) lid_open = 1.0f;

    float base_top = -eye_r * lid_open;
    float base_bot =  eye_r * lid_open;

    float sh_cx = iris_cx - 6.0f, sh_cy = iris_cy - 7.0f;
    float sh_r = 5.5f, sh_r_sq = sh_r * sh_r;

    uint16_t iris_dark = blend_colors(pal[PAL_IRIS], pal[PAL_PUPIL], 0.65f);

    for (int x = x_start; x <= x_end; x++) {
        float fx = x - eye_cx;
        float r_sq = fx * fx + fy * fy;
        if (r_sq >= eye_r * eye_r) continue;

        float arc = 1.0f - (fx * fx) / (eye_r * eye_r);
        // Inner/outer corner adjustments
        float inner_adj = ep->inner_corner.dy * 5.0f;
        float outer_adj = ep->outer_corner.dy * 5.0f;
        float corner_adj = inner_adj + (outer_adj - inner_adj) * ((fx + eye_r) / (2.0f * eye_r));
        float top_lid = base_top + (5.0f * lid_open * arc) + corner_adj * 0.5f;
        float bot_lid = base_bot - (3.0f * lid_open * arc) - corner_adj * 0.3f;
        if (fy < top_lid || fy > bot_lid) continue;

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
    float base_width = bp->thickness * 4.0f;
    if (dy < -base_width - 2 || dy > base_width + 2) return;

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

        float inner_mult = 1.0f - bp->taper * 0.3f;
        float tail_mult = 1.0f - bp->taper * 0.6f;
        float width_mult = inner_mult + (tail_mult - inner_mult) * t;
        float half_thick = base_width * width_mult;

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
    int x_start = CENTER_X - (int)half_width - 3;
    int x_end   = CENTER_X + (int)half_width + 3;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    float lcx = CENTER_X + mp->left_corner.dx * half_width;
    float rcx = CENTER_X + mp->right_corner.dx * half_width;
    float lcy = mouth_cy + mp->left_corner.dy * 12.0f;
    float rcy = mouth_cy + mp->right_corner.dy * 12.0f;
    float uly = mouth_cy + mp->upper_lip_mid.dy * 15.0f;
    float lly = mouth_cy + mp->lower_lip_mid.dy * 15.0f;

    float openness_offset = mp->openness * 12.0f;

    for (int x = x_start; x <= x_end; x++) {
        float t = (x - lcx) / (rcx - lcx + 0.001f);
        if (t < 0.0f || t > 1.0f) continue;

        float corner_y = (1-t) * lcy + t * rcy;
        float upper_y = (1-t)*(1-t)*corner_y + 2*(1-t)*t*uly + t*t*corner_y;
        float lower_y = (1-t)*(1-t)*corner_y + 2*(1-t)*t*(lly + openness_offset) + t*t*corner_y;

        if (y >= upper_y - 1.5f && y <= lower_y + 1.5f) {
            if (y > upper_y + 1.5f && y < lower_y - 1.5f && mp->openness > 0.05f) {
                // Tongue in open mouth
                if (mp->openness > 0.2f) {
                    float tongue_cx = CENTER_X;
                    float tongue_cy = (upper_y + lower_y) * 0.5f + openness_offset * 0.3f;
                    float tongue_rx = half_width * 0.35f;
                    float tongue_ry = openness_offset * 0.3f;
                    float fx_t = x - tongue_cx;
                    float fy_t = y - tongue_cy;
                    float tongue_dist = (fx_t * fx_t) / (tongue_rx * tongue_rx)
                                      + (fy_t * fy_t) / (tongue_ry * tongue_ry);
                    if (tongue_dist < 1.0f) {
                        buf[x] = blend_colors(buf[x], sp->pal[PAL_TONGUE], 0.85f);
                    } else {
                        buf[x] = blend_colors(buf[x], sp->pal[PAL_PUPIL], 0.7f);
                    }
                } else {
                    buf[x] = blend_colors(buf[x], sp->pal[PAL_PUPIL], 0.7f);
                }
            } else {
                float alpha = 0.7f;
                buf[x] = blend_colors(buf[x], sp->pal[PAL_MOUTH], alpha);
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
    .pal = PALETTE_BLACK,
};
