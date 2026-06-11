#include "sprite_chibi.h"
#include "face_palette.h"
#include "face_common.h"
#include <math.h>
#include <string.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

/* ── Pass 1: draw_face — warm white fill + soft grey-brown circle outline ── */

static void draw_face(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    (void)st;
    const uint16_t *pal = sp->pal;
    float dy = y - CENTER_Y;
    float face_r = 118.0f;
    float outline_thick = 6.0f;

    if (fabsf(dy) > face_r) {
        for (int x = 0; x < SCREEN_W; x++) buf[x] = RGB565(0, 0, 0);
        return;
    }

    float span = sqrtf(face_r * face_r - dy * dy);
    int x0 = CENTER_X - (int)span; if (x0 < 0) x0 = 0;
    int x1 = CENTER_X + (int)span; if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;

    for (int x = 0; x < SCREEN_W; x++) {
        if (x >= x0 && x <= x1) {
            float fx = x - CENTER_X;
            float d = sqrtf(fx * fx + dy * dy);
            if (d > face_r - outline_thick) {
                buf[x] = pal[PAL_BG_EDGE];
            } else {
                buf[x] = pal[PAL_BG];
            }
        } else {
            buf[x] = RGB565(0, 0, 0);
        }
    }
}

/* ── Pass 2: draw_blush — Bayer-dithered radial gradient ── */

static void draw_blush(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    float level = st->decor.blush;
    if (level <= 0.01f) return;
    const uint16_t *pal = sp->pal;
    int bcy = CENTER_Y + (int)sp->blush_y_offset;

    for (int side = 0; side < 2; side++) {
        int bcx = (side == 0) ? CENTER_X - 55 : CENTER_X + 55;
        float outer_r = 18.0f * level;
        float inner_r = 6.0f * level;
        if (outer_r < 1.0f) continue;

        float dy_f = y - bcy;
        if (fabsf(dy_f) > outer_r) continue;

        float span = sqrtf(outer_r * outer_r - dy_f * dy_f);
        int x0 = bcx - (int)span; if (x0 < 0) x0 = 0;
        int x1 = bcx + (int)span; if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;

        for (int x = x0; x <= x1; x++) {
            float dx_f = x - bcx;
            float d = sqrtf(dx_f * dx_f + dy_f * dy_f);
            if (d > outer_r) continue;
            if (d < inner_r) {
                buf[x] = pal[PAL_BLUSH];
                continue;
            }
            float opacity = 1.0f - (d - inner_r) / (outer_r - inner_r);
            if (opacity < 0.4f) continue;
            if (bayer_accept(x, y, opacity)) {
                buf[x] = pal[PAL_BLUSH];
            }
        }
    }
}

/* ── Pass 3: draw_mouth — CR spline + jitter omega-shaped small mouth ── */

static void draw_mouth(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const mouth_params_t *mp = &st->mouth;
    const uint16_t *pal = sp->pal;
    int my = CENTER_Y + (int)sp->mouth_y_center;

    float openness = mp->openness;
    float cupid_d = mp->cupid_depth;
    float corner_dy = (mp->left_corner.dy + mp->right_corner.dy) * 0.5f;

    float corner_lx = CENTER_X - 18.0f - openness * 6.0f;
    float corner_rx = CENTER_X + 18.0f + openness * 6.0f;
    float corner_y  = my + corner_dy * 10.0f;

    float cupid_y = my - cupid_d * 12.0f - openness * 8.0f;
    float ul_mid_y = my - openness * 10.0f;
    (void)ul_mid_y;

    float ll_y = my + openness * 14.0f;
    float half_thick = 2.5f;

    float jx0 = hand_jitter((int)corner_lx, y, half_thick * 0.6f);
    float jx1 = hand_jitter((int)corner_rx, y, half_thick * 0.6f);

    /* Upper lip CR spline: p0=ghost, p1=left corner, p2=cupid peak, p3=right corner */
    float x0 = corner_lx + jx0 - 5.0f;
    float y0 = corner_y + hand_jitter((int)corner_lx + 1, y, half_thick * 0.5f);
    float x1 = corner_lx + jx0;
    float y1 = corner_y;
    float x2 = CENTER_X;
    float y2 = cupid_y + hand_jitter(CENTER_X, y, half_thick * 0.5f);
    float x3 = corner_rx + jx1;
    float y3 = corner_y;

    float x_min = x0 < x3 ? x0 : x3;
    if (x1 < x_min) x_min = x1;
    if (x2 < x_min) x_min = x2;
    float x_max = x0 > x3 ? x0 : x3;
    if (x1 > x_max) x_max = x1;
    if (x2 > x_max) x_max = x2;

    int xs = (int)x_min - (int)half_thick - 3; if (xs < 0) xs = 0;
    int xe = (int)x_max + (int)half_thick + 3; if (xe >= SCREEN_W) xe = SCREEN_W - 1;

    for (int x = xs; x <= xe; x++) {
        float t = (x - x1) / (x3 - x1 + 0.001f);
        if (t < -0.1f || t > 1.1f) continue;
        float qx = cr_spline(x0, x1, x2, x3, t);
        float dt = 0.001f;
        float qx2 = cr_spline(x0, x1, x2, x3, t + dt);
        float dxdt = (qx2 - qx) / dt;
        if (fabsf(dxdt) > 0.5f) t += ((float)x - qx) / dxdt;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float cy = cr_spline(y0, y1, y2, y3, t);
        if (fabsf((float)y - cy) < half_thick) {
            buf[x] = pal[PAL_MOUTH];
        }
    }

    /* Lower lip CR spline (smaller arc) */
    if (openness > 0.02f) {
        float llx0 = corner_lx - 2.0f;
        float lly0 = corner_y;
        float llx1 = corner_lx + (corner_rx - corner_lx) * 0.3f;
        float lly1 = ll_y + jx0 * 0.3f;
        float llx2 = corner_rx - (corner_rx - corner_lx) * 0.3f;
        float lly2 = ll_y + jx1 * 0.3f;
        float llx3 = corner_rx + 2.0f;
        float lly3 = corner_y;

        for (int x = xs; x <= xe; x++) {
            float t = (x - llx1) / (llx3 - llx1 + 0.001f);
            if (t < -0.1f || t > 1.1f) continue;
            float qx_r = cr_spline(llx0, llx1, llx2, llx3, t);
            float dt = 0.001f;
            float qx2_r = cr_spline(llx0, llx1, llx2, llx3, t + dt);
            float dxdt_r = (qx2_r - qx_r) / dt;
            if (fabsf(dxdt_r) > 0.5f) t += ((float)x - qx_r) / dxdt_r;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            float cy = cr_spline(lly0, lly1, lly2, lly3, t);
            if (fabsf((float)y - cy) < half_thick * 0.8f) {
                buf[x] = pal[PAL_MOUTH];
            }
        }
    }
}

/* ── Eye rendering: 8 layers, chibi-proportioned ── */

static void draw_eye_chibi(int y, const eye_params_t *ep, float eye_r,
                            int eye_cx, int eye_cy, const uint16_t *pal, uint16_t *buf) {
    float fy = y - eye_cy;
    float pad = 4.0f;
    if (fy < -eye_r - pad || fy > eye_r + pad) return;

    int x_start = eye_cx - (int)eye_r - (int)pad;
    int x_end   = eye_cx + (int)eye_r + (int)pad;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    float eye_r_sq = eye_r * eye_r;

    /* Eyelid geometry */
    float lid_open = 1.0f - ep->top_lid_mid.dy * 0.5f;
    float base_top = -eye_r * 0.85f;
    float top_lid_y = base_top + (18.0f * lid_open);
    float top_lid_curve = eye_r * 0.12f * lid_open;
    float bot_lid_y = eye_r * 0.55f + ep->bot_lid_mid.dy * 8.0f;

    /* Pupil center (driven by gaze) */
    float iris_off_x = ep->iris_center.dx * eye_r * 0.45f;
    float iris_off_y = ep->iris_center.dy * eye_r * 0.35f;

    /* Kawaii: large black pupil replaces the old iris (≈ eye_r * 0.52) */
    float pupil_scale = 0.92f + ep->pupil_scale * 0.3f;
    float pupil_r = eye_r * 0.52f * pupil_scale;
    if (pupil_r < 6.0f) pupil_r = 6.0f;
    float pupil_r_sq = pupil_r * pupil_r;

    /* Dual white sparkles inside the pupil (kawaii standard proportions) */
    float shine = ep->shine_intensity;
    float sh = shine > 0.0f ? sqrtf(shine) : 0.0f;
    float cl1_off_x = -pupil_r * 0.32f;
    float cl1_off_y = -pupil_r * 0.34f;
    float cl1_r = pupil_r * 0.30f * sh;          /* upper-left, larger */
    float cl1_r_sq = cl1_r * cl1_r;
    float cl2_off_x = pupil_r * 0.26f;
    float cl2_off_y = pupil_r * 0.24f;
    float cl2_r = pupil_r * 0.16f * sh;          /* lower-right, smaller */
    float cl2_r_sq = cl2_r * cl2_r;

    for (int x = x_start; x <= x_end; x++) {
        float fx = x - eye_cx;
        float r_sq = fx * fx + fy * fy;
        if (r_sq >= eye_r_sq) continue;

        /* Eyelid clipping */
        float t_norm = fx / eye_r;
        float top_lid_at_x = top_lid_y + top_lid_curve * (t_norm * t_norm - 0.5f);
        if (fy < top_lid_at_x) continue;
        if (fy > bot_lid_y) continue;

        /* Kawaii pupil: large black disc with dual white sparkles.
           Drawn inside the lid-clipped loop, so it can never float
           outside the eye during blinks/squints. */
        float p_dx = fx - iris_off_x;
        float p_dy = fy - iris_off_y;
        float p_dist_sq = p_dx * p_dx + p_dy * p_dy;

        if (p_dist_sq < pupil_r_sq) {
            bool sparkle = false;
            if (shine > 0.1f) {
                float s1dx = p_dx - cl1_off_x;
                float s1dy = p_dy - cl1_off_y;
                float s2dx = p_dx - cl2_off_x;
                float s2dy = p_dy - cl2_off_y;
                if (s1dx * s1dx + s1dy * s1dy < cl1_r_sq) sparkle = true;
                else if (s2dx * s2dx + s2dy * s2dy < cl2_r_sq) sparkle = true;
            }
            buf[x] = sparkle ? pal[PAL_SHINE] : pal[PAL_PUPIL];
            continue;
        }

        /* Sclera white */
        buf[x] = pal[PAL_SCLERA];
    }

    /* ── Post-pixel-loop: Eyelashes (Layer 6) ── */
    if (ep->eyelash > 0.15f) {
        static const float lash_x[4] = { -0.45f, -0.15f, 0.15f, 0.45f };
        for (int i = 0; i < 4; i++) {
            int lx = eye_cx + (int)(lash_x[i] * eye_r);
            if (lx < 0 || lx >= SCREEN_W) continue;
            float lid_y_t = top_lid_y + top_lid_curve * (lash_x[i] * lash_x[i] - 0.5f);
            float lash_y = eye_cy + lid_y_t;
            float dy_lash = y - lash_y;
            if (dy_lash > -1.0f && dy_lash < 3.0f * ep->eyelash) {
                float lash_thick = 1.2f * ep->eyelash;
                for (int dx = -2; dx <= 2; dx++) {
                    int px = lx + dx;
                    if (px >= 0 && px < SCREEN_W && fabsf((float)dx) < lash_thick) {
                        buf[px] = pal[PAL_BROW];
                    }
                }
            }
        }
    }

    /* ── Post-pixel-loop: Double eyelid crease (Layer 7) ── */
    {
        float crease_y = eye_cy + top_lid_y - 3.0f;
        if (fabsf(y - crease_y) < 0.8f) {
            int cx0 = eye_cx - (int)(eye_r * 0.65f);
            int cx1 = eye_cx + (int)(eye_r * 0.65f);
            if (cx0 < 0) cx0 = 0;
            if (cx1 >= SCREEN_W) cx1 = SCREEN_W - 1;
            for (int cx = cx0; cx <= cx1; cx++) {
                float ct = (float)(cx - eye_cx) / eye_r;
                float c_curve = crease_y + top_lid_curve * 0.6f * (ct * ct - 0.5f);
                if (fabsf(y - c_curve) < 0.8f) {
                    buf[cx] = pal[PAL_LID];
                }
            }
        }
    }

    /* ── Post-pixel-loop: Lower lid shadow (Layer 8) ── */
    {
        float shadow_y = eye_cy + bot_lid_y + 1.0f;
        if (fabsf(y - shadow_y) < 1.5f) {
            int sx0 = eye_cx - (int)(eye_r * 0.7f);
            int sx1 = eye_cx + (int)(eye_r * 0.7f);
            if (sx0 < 0) sx0 = 0;
            if (sx1 >= SCREEN_W) sx1 = SCREEN_W - 1;
            for (int sx = sx0; sx <= sx1; sx++) {
                float st = (float)(sx - eye_cx) / eye_r;
                float s_opacity = 0.3f * (1.0f - st * st);
                if (s_opacity > 0.05f && bayer_accept(sx, y, s_opacity)) {
                    buf[sx] = blend_colors(buf[sx], pal[PAL_LID], 0.5f);
                }
            }
        }
    }
}

/* ── eye wrappers ── */

static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[0].position.dy * 15.0f);
    draw_eye_chibi(y, &st->eye[0], sp->eye_radius, eye_cx, eye_cy, sp->pal, buf);
}

static void draw_eye_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[1].position.dy * 15.0f);
    draw_eye_chibi(y, &st->eye[1], sp->eye_radius, eye_cx, eye_cy, sp->pal, buf);
}

/* ── draw_brow_chibi: CR spline with jitter + taper ── */

static void draw_brow_chibi(int y, const brow_params_t *bp, int eye_cx, int eye_cy,
                             const sprite_set_t *sp, const uint16_t *pal, uint16_t *buf) {
    float brow_y_px = eye_cy + sp->brow_y_offset;
    float dy = y - brow_y_px;
    float half_thick = bp->thickness * 2.5f;
    if (half_thick < 2.0f) half_thick = 2.0f;
    if (dy < -half_thick - 4 || dy > half_thick + 4) return;

    float inner_x = eye_cx + bp->inner.dx * 30.0f;
    float inner_y = brow_y_px + bp->inner.dy * 18.0f;
    float arch_x  = eye_cx + bp->arch.dx * 15.0f;
    float arch_y  = brow_y_px + bp->arch.dy * 24.0f;
    float tail_x  = eye_cx + bp->tail.dx * 38.0f;
    float tail_y  = brow_y_px + bp->tail.dy * 16.0f;

    /* CR spline: p0=ghost left, p1=inner, p2=arch, p3=tail */
    float p0x = inner_x - 8.0f;
    float p0y = inner_y;
    float p1x = inner_x;
    float p1y = inner_y;
    float p2x = arch_x;
    float p2y = arch_y;
    float p3x = tail_x;
    float p3y = tail_y;

    float x_min = p1x < p3x ? p1x : p3x;
    float x_max = p1x > p3x ? p1x : p3x;
    if (p2x < x_min) x_min = p2x;
    if (p2x > x_max) x_max = p2x;

    int xs = (int)x_min - (int)half_thick - 4; if (xs < 0) xs = 0;
    int xe = (int)x_max + (int)half_thick + 4; if (xe >= SCREEN_W) xe = SCREEN_W - 1;

    for (int x = xs; x <= xe; x++) {
        float t = (x - p1x) / (p3x - p1x + 0.001f);
        if (t < -0.1f || t > 1.1f) continue;

        float qx = cr_spline(p0x, p1x, p2x, p3x, t);
        float dt = 0.001f;
        float qx2 = cr_spline(p0x, p1x, p2x, p3x, t + dt);
        float dxdt = (qx2 - qx) / dt;
        if (fabsf(dxdt) > 0.5f) t += ((float)x - qx) / dxdt;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        float cy = cr_spline(p0y, p1y, p2y, p3y, t);
        cy += hand_jitter(x, y, half_thick * 0.7f);

        float taper_thick = half_thick * (1.0f - t * 0.5f);
        if (fabsf((float)y - cy) < taper_thick) {
            buf[x] = pal[PAL_BROW];
        }
    }
}

static void draw_brow_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[0].position.dy * 15.0f);
    draw_brow_chibi(y, &st->brow[0], eye_cx, eye_cy, sp, sp->pal, buf);
}

static void draw_brow_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[1].position.dy * 15.0f);
    draw_brow_chibi(y, &st->brow[1], eye_cx, eye_cy, sp, sp->pal, buf);
}

/* ── Pass 8: draw_decor_overlay — sparkle dots + gold stars ── */

static void draw_decor_overlay(int y, const face_state_t *st,
                                const sprite_set_t *sp, uint16_t *buf) {
    const decor_params_t *dp = &st->decor;
    const uint16_t *pal = sp->pal;

    /* Sparkle: gold dots near eyes */
    if (dp->sparkle > 0.2f) {
        static const int sparkle_pos[6][2] = {
            {CENTER_X - 58, CENTER_Y - 42}, {CENTER_X + 58, CENTER_Y - 42},
            {CENTER_X - 55, CENTER_Y + 40}, {CENTER_X + 55, CENTER_Y + 40},
            {CENTER_X - 20, CENTER_Y - 55}, {CENTER_X + 20, CENTER_Y - 55},
        };
        for (int i = 0; i < 6; i++) {
            int sx = sparkle_pos[i][0], sy = sparkle_pos[i][1];
            if (y < sy - 3 || y > sy + 3) continue;
            for (int x = sx - 2; x <= sx + 2; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                if (dist_sq(x, y, sx, sy) < 3.5f) {
                    buf[x] = pal[PAL_STAR];
                }
            }
        }
    }

    /* Stars: gold cross+diagonal shapes */
    if (dp->stars > 0.01f) {
        static const int star_pos[4][2] = {
            {CENTER_X - 25, CENTER_Y - 20}, {CENTER_X + 25, CENTER_Y - 20},
            {CENTER_X - 40, CENTER_Y + 10}, {CENTER_X + 40, CENTER_Y + 10},
        };
        for (int i = 0; i < 4; i++) {
            int sx = star_pos[i][0], sy = star_pos[i][1];
            if (y < sy - 5 || y > sy + 5) continue;
            for (int x = sx - 5; x <= sx + 5; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                if (dist_sq(x, y, sx, sy) < 25.0f) {
                    float adx = fabsf((float)(x - sx));
                    float ady = fabsf((float)(y - sy));
                    bool on_cross = (adx < 1.2f && ady < 4.5f) || (ady < 1.2f && adx < 4.5f);
                    bool on_diag = fabsf(adx - ady) < 1.2f && adx < 3.0f && ady < 3.0f;
                    if (on_cross || on_diag) buf[x] = pal[PAL_STAR];
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

const sprite_set_t SPRITE_CHIBI = {
    .name = "chibi",
    .eye_radius = 45.0f,
    .eye_half_spacing = 32.0f,
    .mouth_y_center = 45.0f,
    .brow_y_offset = -48.0f,
    .blush_y_offset = 58.0f,
    .draw_face = draw_face,
    .draw_blush = draw_blush,
    .draw_mouth = draw_mouth,
    .draw_eye_left = draw_eye_left,
    .draw_eye_right = draw_eye_right,
    .draw_brow_left = draw_brow_left,
    .draw_brow_right = draw_brow_right,
    .draw_decor_overlay = draw_decor_overlay,
    .draw_props = draw_props,
    .pal = PALETTE_CHIBI,
};
