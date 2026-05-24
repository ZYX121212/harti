#include "sprite_vector.h"
#include "face_palette.h"
#include "face_common.h"
#include <math.h>
#include <string.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

/* ── Classification enums ──────────────────────────────────── */

typedef enum {
    V_SMILE, V_SURPRISE, V_SAD, V_ANGRY, V_SLEEPY,
    V_LAUGH, V_KISS, V_NEUTRAL, V_TALK, V_CONFUSED
} mouth_type_t;

typedef enum {
    V_EYE_NORMAL, V_EYE_SLEEPY
} eye_type_t;

/* ── Mouth classifier ──────────────────────────────────────── */

static mouth_type_t classify_mouth(const mouth_params_t *mp, const eye_params_t *ep) {
    float o = mp->openness;
    float cdy = (mp->left_corner.dy + mp->right_corner.dy) * 0.5f;
    float cupid = mp->cupid_depth;

    /* V_SLEEPY first: sleepy eyes always get sleepy mouth (was unreachable) */
    if (ep->top_lid_mid.dy > 0.22f) return V_SLEEPY;
    if (cupid > 0.62f && o > 0.06f && ep->pupil_scale < 0.12f) return V_KISS;
    if (o > 0.18f && cupid > 0.55f && cdy < -0.03f) return V_LAUGH;
    if (o > 0.22f) return V_SURPRISE;
    if (o > 0.12f) return V_TALK;
    /* Expanded smile: corner lift OR horizontal stretch + deep cupid (HAPPY) */
    if (cdy < -0.04f || (fabsf(mp->left_corner.dx) > 0.15f && cupid > 0.55f)) return V_SMILE;
    if (cdy > 0.07f && o < 0.05f && cupid > 0.35f) return V_ANGRY;
    if (cdy > 0.06f) return V_SAD;
    /* Asymmetric horizontal corner positions (CONFUSED) */
    if (fabsf(mp->left_corner.dx - mp->right_corner.dx) > 0.2f && o > 0.04f) return V_CONFUSED;
    return V_NEUTRAL;
}

static eye_type_t classify_eye(const eye_params_t *ep) {
    return (ep->top_lid_mid.dy > 0.22f) ? V_EYE_SLEEPY : V_EYE_NORMAL;
}

/* ── draw_face: solid black ────────────────────────────────── */

static void draw_face(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    (void)st;
    for (int x = 0; x < SCREEN_W; x++) buf[x] = sp->pal[PAL_BG];
}

/* ── draw_eye_vector: white outline ring + white pupil + limbal ring + dual catchlight ── */

static void draw_eye_vector(int y, const eye_params_t *ep, float eye_r,
                            int eye_cx, int eye_cy, eye_type_t etype,
                            const uint16_t *pal, uint16_t *buf) {
    float fy = y - eye_cy;
    float pad = 2.5f;
    if (fy < -eye_r - pad || fy > eye_r + pad) return;

    int x_start = eye_cx - (int)eye_r - (int)pad;
    int x_end   = eye_cx + (int)eye_r + (int)pad;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    float eye_r_sq = eye_r * eye_r;

    // ── Sleepy eye: white outline ring + horizontal line ──
    if (etype == V_EYE_SLEEPY) {
        float line_y = eye_cy + eye_r * 0.2f;
        float half_h = 2.5f;
        for (int x = x_start; x <= x_end; x++) {
            float fx = x - eye_cx;
            float r_sq = fx * fx + fy * fy;
            if (r_sq < eye_r_sq) {
                float r = sqrtf(r_sq);
                if (eye_r - r < 2.0f) {
                    buf[x] = pal[PAL_SCLERA];
                }
            }
            if (fabsf(y - line_y) < half_h) {
                float fx2 = x - eye_cx;
                if (fx2 * fx2 < eye_r_sq) {
                    buf[x] = pal[PAL_SCLERA];
                }
            }
        }
        return;
    }

    // ── Normal eye: white outline + limbal ring + dual catchlight ──

    // Pupil (white filled, moves with gaze)
    float pupil_dx = ep->iris_center.dx * eye_r * 0.55f;
    float pupil_dy = ep->iris_center.dy * eye_r * 0.55f;
    float pupil_r = eye_r * 0.32f * (0.4f + ep->pupil_scale * 0.8f);
    if (pupil_r < 3.0f) pupil_r = 3.0f;
    float pupil_r_sq = pupil_r * pupil_r;

    // Limbal ring: dark band around pupil edge
    float limbal_r = pupil_r + 3.0f;
    float limbal_r_sq = limbal_r * limbal_r;

    // Dual catchlights
    float shine = ep->shine_intensity;
    float cl1_r = pupil_r * 0.28f * sqrtf(shine);
    float cl1_off_x = -pupil_r * 0.25f;
    float cl1_off_y = -pupil_r * 0.3f;
    float cl1_r_sq = cl1_r * cl1_r;

    float cl2_r = pupil_r * 0.12f * sqrtf(shine);
    float cl2_off_x = pupil_r * 0.2f;
    float cl2_off_y = -pupil_r * 0.35f;
    float cl2_r_sq = cl2_r * cl2_r;

    for (int x = x_start; x <= x_end; x++) {
        float fx = x - eye_cx;
        float r_sq = fx * fx + fy * fy;
        if (r_sq >= eye_r_sq) continue;

        float r = sqrtf(r_sq);

        // 1. White outline ring (~2.5px thick)
        if (eye_r - r < 2.5f) {
            buf[x] = pal[PAL_SCLERA];
            continue;
        }

        // 2. Interior is black (background) — skipped

        float p_dx = fx - pupil_dx;
        float p_dy = fy - pupil_dy;
        float p_dist_sq = p_dx * p_dx + p_dy * p_dy;

        // 3. Limbal ring (dark band around pupil edge)
        if (p_dist_sq < limbal_r_sq && p_dist_sq >= pupil_r_sq && ep->iris_detail > 0.1f) {
            buf[x] = blend_colors(pal[PAL_SCLERA], pal[PAL_PUPIL], ep->iris_detail * 0.6f);
            continue;
        }

        // 4. White pupil
        if (p_dist_sq < pupil_r_sq) {
            // Dual catchlights
            float c1_dx = fx - (pupil_dx + cl1_off_x);
            float c1_dy = fy - (pupil_dy + cl1_off_y);
            float c2_dx = fx - (pupil_dx + cl2_off_x);
            float c2_dy = fy - (pupil_dy + cl2_off_y);

            bool in_cl1 = (shine > 0.1f && c1_dx * c1_dx + c1_dy * c1_dy < cl1_r_sq);
            bool in_cl2 = (shine > 0.1f && c2_dx * c2_dx + c2_dy * c2_dy < cl2_r_sq);

            if (in_cl1 || in_cl2) {
                buf[x] = pal[PAL_PUPIL];   // black catchlight
            } else {
                buf[x] = pal[PAL_SCLERA];  // white pupil
            }
        }
    }

    // ── Post-pixel-loop: Eyelashes on top lid ──
    if (ep->eyelash > 0.2f) {
        static const float lash_x[3] = { -0.35f, 0.0f, 0.35f };
        for (int i = 0; i < 3; i++) {
            int lx = eye_cx + (int)(lash_x[i] * eye_r);
            if (lx < 0 || lx >= SCREEN_W) continue;
            float lash_y = eye_cy - eye_r * 0.78f;
            if (fabsf(y - lash_y) < 2.0f * ep->eyelash) {
                for (int dx = -2; dx <= 2; dx++) {
                    int px = lx + dx;
                    if (px >= 0 && px < SCREEN_W) buf[px] = pal[PAL_BROW];
                }
            }
        }
    }
}

/* ── draw_eye_left / draw_eye_right ────────────────────────── */

static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[0].position.dy * 15.0f);
    eye_type_t et = classify_eye(&st->eye[0]);
    draw_eye_vector(y, &st->eye[0], sp->eye_radius, eye_cx, eye_cy, et, sp->pal, buf);
}

static void draw_eye_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[1].position.dy * 15.0f);
    eye_type_t et = classify_eye(&st->eye[1]);
    draw_eye_vector(y, &st->eye[1], sp->eye_radius, eye_cx, eye_cy, et, sp->pal, buf);
}

/* ── Mouth rendering helpers ───────────────────────────────── */

static bool in_arc_ring(int x, int y, int cx, int cy,
                        int outer_r, int inner_r, float start_deg, float end_deg,
                        const uint16_t *pal, uint16_t *buf) {
    int dx = x - cx, dy = y - cy;
    int d2 = dx * dx + dy * dy;
    if (d2 > outer_r * outer_r || d2 < inner_r * inner_r) return false;
    float ang = atan2f((float)dy, (float)dx);
    if (ang < 0) ang += 2.0f * 3.14159265f;
    float s = start_deg * 3.14159265f / 180.0f;
    float e = end_deg * 3.14159265f / 180.0f;
    if (e < s) { float t = s; s = e; e = t; }
    return ang >= s && ang <= e;
}

static void draw_ellipse_scan(int y, int cx, int cy, int rx, int ry, uint16_t color, uint16_t *buf) {
    if (rx <= 0 || ry <= 0) return;
    if (y < cy - ry || y > cy + ry) return;
    float norm_y = (float)(y - cy) / (float)ry;
    float xspan = rx * sqrtf(1.0f - norm_y * norm_y);
    int x0 = cx - (int)xspan;
    int x1 = cx + (int)xspan;
    if (x0 < 0) x0 = 0;
    if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
    for (int x = x0; x <= x1; x++) buf[x] = color;
}

/* ── draw_mouth ────────────────────────────────────────────── */

static void draw_mouth(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const mouth_params_t *mp = &st->mouth;
    const eye_params_t *ep = &st->eye[0];
    mouth_type_t mt = classify_mouth(mp, ep);
    const uint16_t *pal = sp->pal;
    int my = CENTER_Y + (int)sp->mouth_y_center;

    switch (mt) {

    case V_SMILE: {
        // Arc ring smile: 200°-340°
        int x0 = CENTER_X - 24; if (x0 < 0) x0 = 0;
        int x1 = CENTER_X + 24; if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
        int y0 = my + 5 - 24; if (y0 < 0) y0 = 0;
        int y1 = my + 5 + 24; if (y1 >= SCREEN_H) y1 = SCREEN_H - 1;
        if (y < y0 || y > y1) return;
        for (int x = x0; x <= x1; x++) {
            if (in_arc_ring(x, y, CENTER_X, my + 5, 18, 13, 200.0f, 340.0f, pal, buf)) {
                buf[x] = pal[PAL_MOUTH];
            }
        }
        return;
    }

    case V_SURPRISE:
        draw_ellipse_scan(y, CENTER_X, my, 8, 12, pal[PAL_MOUTH], buf);
        return;

    case V_TALK: {
        int th = (int)(mp->openness * 35.0f);
        if (th < 3) th = 3;
        draw_ellipse_scan(y, CENTER_X, my, 10, th, pal[PAL_MOUTH], buf);
        return;
    }

    case V_SAD: {
        int x0 = CENTER_X - 24; if (x0 < 0) x0 = 0;
        int x1 = CENTER_X + 24; if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
        int y0 = my + 18 - 24; if (y0 < 0) y0 = 0;
        int y1 = my + 18 + 24; if (y1 >= SCREEN_H) y1 = SCREEN_H - 1;
        if (y < y0 || y > y1) return;
        for (int x = x0; x <= x1; x++) {
            if (in_arc_ring(x, y, CENTER_X, my + 16, 18, 13, 20.0f, 160.0f, pal, buf)) {
                buf[x] = pal[PAL_MOUTH];
            }
        }
        return;
    }

    case V_ANGRY: {
        // Flat arc ring 25°-155° + horizontal line
        int x0 = CENTER_X - 22; if (x0 < 0) x0 = 0;
        int x1 = CENTER_X + 22; if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
        int y0 = my + 3 - 22; if (y0 < 0) y0 = 0;
        int y1 = my + 3 + 22; if (y1 >= SCREEN_H) y1 = SCREEN_H - 1;
        if (y < y0 || y > y1) return;
        for (int x = x0; x <= x1; x++) {
            if (in_arc_ring(x, y, CENTER_X, my + 4, 16, 11, 25.0f, 155.0f, pal, buf)) {
                buf[x] = pal[PAL_MOUTH];
            }
        }
        // horizontal line across
        if (y == my + 1 || y == my + 2 || y == my + 3) {
            int lx0 = CENTER_X - 14, lx1 = CENTER_X + 14;
            if (lx0 < 0) lx0 = 0;
            if (lx1 >= SCREEN_W) lx1 = SCREEN_W - 1;
            for (int x = lx0; x <= lx1; x++) buf[x] = pal[PAL_MOUTH];
        }
        return;
    }

    case V_SLEEPY:
        if (y >= my + 5 - 1 && y <= my + 11 + 1) {
            int lx0 = CENTER_X - 12, lx1 = CENTER_X + 12;
            if (lx0 < 0) lx0 = 0;
            if (lx1 >= SCREEN_W) lx1 = SCREEN_W - 1;
            for (int x = lx0; x <= lx1; x++) buf[x] = pal[PAL_MOUTH];
        }
        return;

    case V_LAUGH: {
        // Large ellipse + BG cutout ellipse on top
        for (int pass = 0; pass < 2; pass++) {
            int cx = CENTER_X, cy = my + (pass == 0 ? 4 : 0);
            int rx = pass == 0 ? 18 : 14, ry = pass == 0 ? 14 : 8;
            if (y < cy - ry || y > cy + ry) continue;
            float norm_y = (float)(y - cy) / (float)ry;
            float xspan = rx * sqrtf(1.0f - norm_y * norm_y);
            int x0 = cx - (int)xspan, x1 = cx + (int)xspan;
            if (x0 < 0) x0 = 0;
            if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
            uint16_t clr = (pass == 0) ? pal[PAL_MOUTH] : pal[PAL_BG];
            for (int x = x0; x <= x1; x++) buf[x] = clr;
        }
        return;
    }

    case V_KISS:
        draw_ellipse_scan(y, CENTER_X - 6, my + 4, 5, 8, pal[PAL_MOUTH], buf);
        draw_ellipse_scan(y, CENTER_X + 6, my + 4, 5, 8, pal[PAL_MOUTH], buf);
        return;

    case V_CONFUSED: {
        /* Asymmetric squiggly mouth — diagonal tilt + 1.5-cycle wave */
        float left_dy = mp->left_corner.dy * 25.0f;
        float right_dy = mp->right_corner.dy * 25.0f;
        float half_w = 16.0f + mp->openness * 10.0f;
        int x0 = CENTER_X - (int)half_w; if (x0 < 0) x0 = 0;
        int x1 = CENTER_X + (int)half_w; if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
        float thick = 2.0f + mp->openness * 6.0f;
        for (int x = x0; x <= x1; x++) {
            float t = (float)(x - x0) / (float)(x1 - x0);
            float base_y = my + left_dy + (right_dy - left_dy) * t;
            float squiggle = sinf(t * 3.0f * 3.14159f) * (3.0f + mp->openness * 10.0f);
            float mouth_y = base_y + squiggle;
            if (fabsf(y - mouth_y) < thick) {
                buf[x] = pal[PAL_MOUTH];
            }
        }
        return;
    }

    case V_NEUTRAL:
    default:
        draw_ellipse_scan(y, CENTER_X, my + 6, 10, 4, pal[PAL_MOUTH], buf);
        return;
    }
}

/* ── draw_brow: quadratic bezier using all brow_params_t fields ── */

static void draw_brow_vector(int y, const brow_params_t *bp, int eye_cx, int eye_cy,
                             const sprite_set_t *sp, const uint16_t *pal, uint16_t *buf) {
    float brow_y_px = eye_cy + sp->brow_y_offset;
    float dy = y - brow_y_px;
    float half_thick = bp->thickness * 2.0f;
    if (half_thick < 1.5f) half_thick = 1.5f;
    if (dy < -half_thick - 3 || dy > half_thick + 3) return;

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
        float curve_y = (1 - t) * (1 - t) * inner_y + 2 * (1 - t) * t * arch_y + t * t * tail_y;
        if (fabsf(y - curve_y) < half_thick) {
            buf[x] = pal[PAL_BROW];
        }
    }
}

static void draw_brow_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[0].position.dy * 15.0f);
    draw_brow_vector(y, &st->brow[0], eye_cx, eye_cy, sp, sp->pal, buf);
}

static void draw_brow_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[1].position.dy * 15.0f);
    draw_brow_vector(y, &st->brow[1], eye_cx, eye_cy, sp, sp->pal, buf);
}

/* ── draw_blush: Bayer-dithered radial gradient ──────────── */

static void draw_blush(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    float level = st->decor.blush;
    if (level <= 0.01f) return;
    const uint16_t *pal = sp->pal;
    int bcy = CENTER_Y + (int)sp->blush_y_offset;

    for (int side = 0; side < 2; side++) {
        int bcx = (side == 0) ? CENTER_X - 60 : CENTER_X + 60;
        float outer_r = 15.0f * level;
        float inner_r = 5.0f * level;
        if (outer_r < 2.0f) continue;

        float dy_sq = (y - bcy) * (y - bcy);
        if (dy_sq > outer_r * outer_r) continue;

        float span = sqrtf(outer_r * outer_r - dy_sq);
        int x0 = bcx - (int)span; if (x0 < 0) x0 = 0;
        int x1 = bcx + (int)span; if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;

        for (int x = x0; x <= x1; x++) {
            float d = sqrtf((x - bcx) * (x - bcx) + dy_sq);
            if (d > outer_r) continue;
            if (d < inner_r) {
                buf[x] = pal[PAL_BLUSH];
                continue;
            }
            float opacity = 1.0f - (d - inner_r) / (outer_r - inner_r);
            if (opacity >= 0.4f && bayer_accept(x, y, opacity)) {
                buf[x] = pal[PAL_BLUSH];
            }
        }
    }
}

/* ── draw_decor_overlay: white outline tears/stars/sweat/sparkle ── */

static void draw_decor_overlay(int y, const face_state_t *st,
                                const sprite_set_t *sp, uint16_t *buf) {
    const decor_params_t *dp = &st->decor;
    const uint16_t *pal = sp->pal;

    // ── Tears: white droplet outlines below eyes ──
    if (dp->tears > 0.01f) {
        int tear_cy = CENTER_Y + 30;
        float dy = y - tear_cy;
        if (dy > 0 && dy < 28 * dp->tears) {
            int positions[2] = {CENTER_X - 35, CENTER_X + 35};
            for (int i = 0; i < 2; i++) {
                float r = 5.0f - dy * 0.05f;
                if (r < 1.0f) continue;
                float r_sq = r * r;
                int sx = positions[i], sy = tear_cy + 5;
                for (int x = sx - (int)r - 2; x <= sx + (int)r + 2; x++) {
                    if (x < 0 || x >= SCREEN_W) continue;
                    float d_sq = dist_sq(x, y, sx, sy);
                    if (d_sq < r_sq && fabsf(sqrtf(d_sq) - r) < 1.3f) {
                        buf[x] = pal[PAL_TEAR];
                    }
                }
            }
        }
    }

    // ── Stars: white cross+diagonal shapes ──
    if (dp->stars > 0.01f) {
        static const int star_pos[4][2] = {
            {CENTER_X - 20, CENTER_Y - 15}, {CENTER_X + 20, CENTER_Y - 15},
            {CENTER_X - 35, CENTER_Y + 5},   {CENTER_X + 35, CENTER_Y + 5},
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

    // ── Sweat drop: single white outline ──
    if (dp->sweat > 0.01f) {
        int sx = CENTER_X + 55, sy = CENTER_Y - 45;
        float r = 5.0f * dp->sweat;
        for (int x = sx - (int)r - 2; x <= sx + (int)r + 2; x++) {
            if (x < 0 || x >= SCREEN_W) continue;
            float d_sq = dist_sq(x, y, sx, sy);
            float r_sq = r * r;
            if (d_sq < r_sq && fabsf(sqrtf(d_sq) - r) < 1.3f) {
                buf[x] = pal[PAL_TEAR];
            }
        }
    }

    // ── Sparkle: small white dots near eyes ──
    if (dp->sparkle > 0.2f) {
        static const int sparkle_pos[6][2] = {
            {CENTER_X - 45, CENTER_Y - 40}, {CENTER_X + 45, CENTER_Y - 40},
            {CENTER_X - 50, CENTER_Y + 35}, {CENTER_X + 50, CENTER_Y + 35},
            {CENTER_X - 25, CENTER_Y - 50}, {CENTER_X + 25, CENTER_Y - 50},
        };
        for (int i = 0; i < 6; i++) {
            int sx = sparkle_pos[i][0], sy = sparkle_pos[i][1];
            if (y < sy - 3 || y > sy + 3) continue;
            for (int x = sx - 2; x <= sx + 2; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                if (dist_sq(x, y, sx, sy) < 3.5f) {
                    buf[x] = pal[PAL_SHINE];
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

/* ── Sprite definition ────────────────────────────────────── */

const sprite_set_t SPRITE_VECTOR = {
    .name = "vector",
    .eye_radius = 40.0f,
    .eye_half_spacing = 50.0f,
    .mouth_y_center = 40.0f,
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
    .pal = PALETTE_VECTOR,
};
