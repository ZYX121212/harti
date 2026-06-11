#include "sprite_classic.h"
#include "face_palette.h"
#include "face_common.h"
#include <math.h>
#include <string.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

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
    float roundness = st->face.roundness;
    float sx = 1.0f + st->face.squash_x * 0.3f;
    float sy = 1.0f + st->face.stretch_y * 0.3f;
    if (sx < 0.5f) sx = 0.5f;
    if (sy < 0.5f) sy = 0.5f;
    for (int x = 0; x < SCREEN_W; x++) {
        int dx = x - CENTER_X;
        float dx_s = (float)dx / sx;
        float dy_s = (float)dy / sy;
        float manhattan = fabsf(dx_s) + fabsf(dy_s);
        float euclidean = sqrtf(dx_s * dx_s + dy_s * dy_s);
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

/* ── draw_eye: render one eye with sclera + layered shine ─ */
static void draw_eye_impl(int y, const eye_params_t *ep, float eye_r,
                          int eye_cx, int eye_cy, const uint16_t *pal, uint16_t *buf) {
    float fy = y - eye_cy;
    if (fy < -eye_r - 2 || fy > eye_r + 2) return;

    int x_start = eye_cx - (int)eye_r - 2;
    int x_end   = eye_cx + (int)eye_r + 2;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    /* kawaii pupil geometry (screen coords) */
    float iris_cx = ep->iris_center.dx * 7.0f;
    float iris_cy = ep->iris_center.dy * 7.0f;
    float pupil_r = eye_r * 0.50f * (0.7f + ep->pupil_scale * 0.6f);
    if (pupil_r < 4.0f) pupil_r = 4.0f;
    float pupil_cx = eye_cx + iris_cx;
    float pupil_cy = eye_cy + iris_cy;

    float lid_open = 1.0f - (ep->top_lid_mid.dy * 1.4f);
    if (lid_open < 0.05f) lid_open = 0.05f;
    if (lid_open > 1.0f) lid_open = 1.0f;

    float base_top = -eye_r * lid_open;
    float base_bot =  eye_r * lid_open;

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

        /* eyelash: subtle dark strokes above the top lid */
        if (ep->eyelash > 0.01f && fy >= top_lid - 2.5f && fy < top_lid + 1.0f) {
            float lash_phase = fabsf(fmodf(fx * 1.9f + 0.7f, 4.0f) - 2.0f);
            if (lash_phase < 0.6f && fy < top_lid + 0.5f) {
                float lash_alpha = (1.0f - lash_phase / 0.6f) * ep->eyelash * 0.45f;
                uint16_t lash_color = blend_colors(pal[PAL_IRIS], pal[PAL_PUPIL], 0.85f);
                buf[x] = blend_colors(buf[x], lash_color, lash_alpha);
            }
        }

        if (fy < top_lid || fy > bot_lid) continue;

        float edge_dist = eye_r - sqrtf(r_sq);
        bool is_edge = (edge_dist < 1.5f);

        if (is_edge) {
            /* edge: anti-alias sclera against background */
            buf[x] = blend_colors(buf[x], pal[PAL_SCLERA], edge_dist / 1.4f);
            continue;
        }

        /* interior: solid white sclera (kawaii pupil drawn on top below) */
        buf[x] = blend_colors(buf[x], pal[PAL_SCLERA], 0.92f);
    }

    /* glossy black pupil + two white sparkles over the sclera.
       Gate by the lid band at the eye's horizontal centre (arc=1 at fx=0) so the
       pupil is clipped to the open eye and shrinks smoothly with a blink/squint
       instead of floating over the background when the lids close. */
    {
        float corner0 = (ep->inner_corner.dy * 5.0f + ep->outer_corner.dy * 5.0f) * 0.5f;
        float center_top = base_top + 5.0f * lid_open + corner0 * 0.5f;
        float center_bot = base_bot - 3.0f * lid_open - corner0 * 0.3f;
        if (fy >= center_top && fy <= center_bot) {
            draw_kawaii_pupil(y, x_start, x_end,
                              pupil_cx, pupil_cy, pupil_r,
                              ep->shine_intensity, pal[PAL_PUPIL], pal[PAL_SCLERA], buf);
        }
    }
}

static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[0].position.dy * 15.0f);
    draw_eye_impl(y, &st->eye[0], sp->eye_radius, eye_cx, eye_cy, sp->pal, buf);
}

static void draw_eye_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[1].position.dy * 15.0f);
    draw_eye_impl(y, &st->eye[1], sp->eye_radius, eye_cx, eye_cy, sp->pal, buf);
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
    int eye_cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[0].position.dy * 15.0f);
    draw_brow_impl(y, &st->brow[0], eye_cx, eye_cy, sp, sp->pal, buf);
}

static void draw_brow_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[1].position.dy * 15.0f);
    draw_brow_impl(y, &st->brow[1], eye_cx, eye_cy, sp, sp->pal, buf);
}

/* ── draw_mouth: simple mouth shape ──────────────────────── */
static void draw_mouth(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const mouth_params_t *mp = &st->mouth;
    int mouth_cy = CENTER_Y + (int)sp->mouth_y_center;

    float half_width = 30.0f;
    int x_start = CENTER_X - (int)half_width - 3;
    int x_end   = CENTER_X + (int)half_width + 3;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    float lcx = CENTER_X - half_width + mp->left_corner.dx * 14.0f;
    float rcx = CENTER_X + half_width + mp->right_corner.dx * 14.0f;
    float lcy = mouth_cy + mp->left_corner.dy * 12.0f;
    float rcy = mouth_cy + mp->right_corner.dy * 12.0f;
    float uly = mouth_cy + mp->upper_lip_mid.dy * 15.0f;
    float lly = mouth_cy + mp->lower_lip_mid.dy * 15.0f;

    float openness_offset = mp->openness * 12.0f;

    for (int x = x_start; x <= x_end; x++) {
        float t = (x - lcx) / (rcx - lcx + 0.001f);
        if (t < 0.0f || t > 1.0f) continue;

        float corner_y = (1-t) * lcy + t * rcy;
        float smile_lift = sinf(t * 3.14159f) * 3.0f;
        float upper_y = (1-t)*(1-t)*corner_y + 2*(1-t)*t*(uly - smile_lift) + t*t*corner_y;
        /* cupid's bow: dip at center of upper lip */
        if (mp->cupid_depth > 0.01f) {
            float center_dist = 1.0f - fabsf(t - 0.5f) * 2.0f;
            float dip = center_dist * center_dist * mp->cupid_depth * 4.0f;
            upper_y += dip;
        }
        float lower_y = (1-t)*(1-t)*corner_y + 2*(1-t)*t*(lly + openness_offset) + t*t*corner_y;

        if (y >= upper_y - 2.5f && y <= lower_y + 2.5f) {
            if (y > upper_y + 1.5f && y < lower_y - 1.5f && mp->openness > 0.05f) {
                /* tooth_show: teeth in upper portion of open mouth */
                if (mp->tooth_show > 0.01f) {
                    float cavity_h = (lower_y - 1.5f) - (upper_y + 1.5f);
                    float rel_y = (y - (upper_y + 1.5f)) / cavity_h;
                    float tooth_zone = mp->tooth_show * 0.5f;
                    if (rel_y < tooth_zone) {
                        float tooth_t = rel_y / (tooth_zone + 0.001f);
                        float tooth_alpha = (1.0f - tooth_t) * 0.85f;
                        buf[x] = blend_colors(buf[x], sp->pal[PAL_SCLERA], tooth_alpha);
                    } else if (mp->openness > 0.2f) {
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
                    /* original tongue / dark fill */
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
    int left_cx  = CENTER_X - 64;
    int right_cx = CENTER_X + 64;
    float blush_r = 26.0f;
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
            if (t >= 0.4f && bayer_accept(x, y, t)) buf[x] = pal[PAL_BLUSH];
        }

        bool left_dash = (x >= left_cx - 12 && x <= left_cx + 10 && fabsf((float)y - (blush_cy + (x - left_cx) * 0.18f)) < 1.2f);
        bool right_dash = (x >= right_cx - 10 && x <= right_cx + 12 && fabsf((float)y - (blush_cy - (x - right_cx) * 0.18f)) < 1.2f);
        if (left_dash || right_dash) {
            if (bayer_accept(x, y, level * 0.75f)) buf[x] = pal[PAL_BLUSH];
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

/* ── draw_props ─────────────────────────────────────────────── */

static void draw_props(int y, const face_state_t *st,
                       const sprite_set_t *sp, uint16_t *buf) {
    for (int i = 0; i < st->decor.prop_count; i++) {
        const prop_instance_t *p = &st->decor.props[i];
        if (p->opacity <= 0.01f) continue;

        float r = 100.0f * p->distance;
        float px = CENTER_X + r * cosf(p->angle);
        float py = CENTER_Y - r * sinf(p->angle);
        float sz = 10.0f + p->scale * 12.0f;

        uint16_t raw_color;
        switch (p->type) {
        case PROP_HEART:      raw_color = sp->pal[PAL_BLUSH]; break;
        case PROP_TEACUP:     raw_color = sp->pal[PAL_SKIN];  break;
        case PROP_HAND:       raw_color = sp->pal[PAL_SKIN];  break;
        case PROP_STAR_SMALL: raw_color = sp->pal[PAL_STAR];  break;
        case PROP_SWEAT_DROP: raw_color = sp->pal[PAL_TEAR];  break;
        case PROP_FINGER_HEART: raw_color = sp->pal[PAL_SCLERA]; break;
        default: continue;
        }

        uint16_t color = blend_colors(sp->pal[PAL_BG], raw_color, p->opacity);

        switch (p->type) {
        case PROP_HEART:      draw_heart_scan(y, px, py, sz, color, buf, SCREEN_W); break;
        case PROP_TEACUP:     draw_teacup_scan(y, px, py, sz, color, buf, SCREEN_W); break;
        case PROP_HAND:       draw_hand_scan(y, px, py, sz, color, buf, SCREEN_W); break;
        case PROP_STAR_SMALL: draw_star_scan(y, px, py, sz, color, buf, SCREEN_W); break;
        case PROP_SWEAT_DROP: draw_sweat_scan(y, px, py, sz, color, buf, SCREEN_W); break;
        case PROP_FINGER_HEART: draw_finger_heart_scan(y, px, py, sz, color, buf, SCREEN_W); break;
        default: break;
        }
    }
}

/* ── Sprite definition ────────────────────────────────────── */

const sprite_set_t SPRITE_CLASSIC = {
    .name = "classic",
    .eye_radius = 35.0f,
    .eye_half_spacing = 38.0f,
    .mouth_y_center = 47.0f,
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
    .draw_props = draw_props,
    .pal = PALETTE_WHITE,
};
