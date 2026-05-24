#include "sprite_lineart.h"
#include "face_palette.h"
#include "face_common.h"
#include <math.h>
#include <string.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

/* ── draw_face: solid black background ───────────────────── */
static void draw_face(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    (void)st;
    for (int x = 0; x < SCREEN_W; x++) buf[x] = sp->pal[PAL_BG];
}

/* ── draw_eye_lineart: white outline + filled pupil + black catchlight ── */
static void draw_eye_lineart(int y, const eye_params_t *ep, float eye_r,
                             int eye_cx, int eye_cy, const uint16_t *pal, uint16_t *buf) {
    float fy = y - eye_cy;
    float pad = eye_r * 0.15f;
    if (fy < -eye_r - pad || fy > eye_r + pad) return;

    int x_start = eye_cx - (int)eye_r - (int)pad;
    int x_end   = eye_cx + (int)eye_r + (int)pad;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    float eye_r_sq = eye_r * eye_r;

    // ── pupil: filled white circle offset by iris_center ──
    float pupil_r = eye_r * 0.22f * ep->pupil_scale;
    float pcx = eye_cx + ep->iris_center.dx * 8.0f;
    float pcy = eye_cy + ep->iris_center.dy * 8.0f;
    float pupil_r_sq = pupil_r * pupil_r;

    // ── catchlight: black negative-space dot inside pupil ──
    float cl_r = pupil_r * 0.35f;
    float clx = pcx - pupil_r * 0.3f;
    float cly = pcy - pupil_r * 0.35f;
    float cl_r_sq = cl_r * cl_r;

    // ── lid openness from params ──
    float lid_open = 1.0f - (ep->top_lid_mid.dy * 1.6f);
    if (lid_open < 0.04f) lid_open = 0.04f;
    if (lid_open > 1.05f) lid_open = 1.05f;
    float base_top = -eye_r * lid_open;
    float base_bot =  eye_r * lid_open;

    for (int x = x_start; x <= x_end; x++) {
        float fx = x - eye_cx;
        float r_sq = fx * fx + fy * fy;
        if (r_sq >= eye_r_sq) continue;

        // ── lid curves (quadratic blend) ──
        float arc = 1.0f - (fx * fx) / eye_r_sq;
        float inner_adj = ep->inner_corner.dy * 5.0f;
        float outer_adj = ep->outer_corner.dy * 5.0f;
        float corner_adj = inner_adj + (outer_adj - inner_adj) * ((fx + eye_r) / (2.0f * eye_r));
        float top_lid = base_top + (4.0f * lid_open * arc) + corner_adj * 0.5f;
        float bot_lid = base_bot - (2.5f * lid_open * arc) - corner_adj * 0.3f;

        if (fy < top_lid || fy > bot_lid) continue;

        bool drawn = false;

        // 1. Eye outline (white ring at edge)
        float edge_dist = eye_r - sqrtf(r_sq);
        if (edge_dist < 2.0f) {
            buf[x] = pal[PAL_SCLERA];
            drawn = true;
        }

        // 2. Top/bottom lid lines (thin white lines)
        float dist_top = fabsf(fy - top_lid);
        float dist_bot = fabsf(fy - bot_lid);
        if (!drawn && (dist_top < 1.3f || dist_bot < 1.3f)) {
            buf[x] = pal[PAL_SCLERA];
            drawn = true;
        }

        // 3. Iris ring (white outline, interior stays black)
        float iris_r = eye_r * 0.50f;
        float iris_r_sq = iris_r * iris_r;
        float iris_d = dist_sq(fx, fy, pcx, pcy);
        if (!drawn && iris_d < iris_r_sq) {
            float iris_edge = fabsf(sqrtf(iris_d) - iris_r);
            if (iris_edge < 1.5f) {
                buf[x] = pal[PAL_IRIS];
                drawn = true;
            }
        }

        // 4. Filled white pupil
        if (!drawn && dist_sq(fx, fy, pcx, pcy) < pupil_r_sq) {
            // catchlight: black negative space
            if (dist_sq(fx, fy, clx, cly) < cl_r_sq) {
                buf[x] = pal[PAL_BG];  // black cutout
            } else {
                buf[x] = pal[PAL_PUPIL]; // white filled
            }
            drawn = true;
        }
    }
}

static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[0].position.dy * 15.0f);
    draw_eye_lineart(y, &st->eye[0], sp->eye_radius, eye_cx, eye_cy, sp->pal, buf);
}

static void draw_eye_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[1].position.dy * 15.0f);
    draw_eye_lineart(y, &st->eye[1], sp->eye_radius, eye_cx, eye_cy, sp->pal, buf);
}

/* ── draw_brow: thin white quadratic bezier ──────────────── */
static void draw_brow_lineart(int y, const brow_params_t *bp, int eye_cx, int eye_cy,
                              const sprite_set_t *sp, const uint16_t *pal, uint16_t *buf) {
    float brow_y_px = eye_cy + sp->brow_y_offset;
    float dy = y - brow_y_px;
    float half_thick = 2.0f;
    if (dy < -half_thick - 2 || dy > half_thick + 2) return;

    float inner_x = eye_cx + bp->inner.dx * 25.0f;
    float inner_y = brow_y_px + bp->inner.dy * 15.0f;
    float arch_y  = brow_y_px + bp->arch.dy * 20.0f;
    float tail_x  = eye_cx + bp->tail.dx * 30.0f;
    float tail_y  = brow_y_px + bp->tail.dy * 15.0f;

    int x_start = (int)inner_x - 3;
    int x_end   = (int)tail_x + 3;
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
    draw_brow_lineart(y, &st->brow[0], eye_cx, eye_cy, sp, sp->pal, buf);
}

static void draw_brow_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[1].position.dy * 15.0f);
    draw_brow_lineart(y, &st->brow[1], eye_cx, eye_cy, sp, sp->pal, buf);
}

/* ── draw_mouth: white outlines + black interior ──────────── */
static void draw_mouth(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const mouth_params_t *mp = &st->mouth;
    int mouth_cy = CENTER_Y + (int)sp->mouth_y_center;

    float half_width = 34.0f;
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
    float openness_offset = mp->openness * 14.0f;

    for (int x = x_start; x <= x_end; x++) {
        float t = (x - lcx) / (rcx - lcx + 0.001f);
        if (t < 0.0f || t > 1.0f) continue;

        float corner_y = (1 - t) * lcy + t * rcy;
        float smile_lift = sinf(t * 3.14159f) * 4.0f;

        // cupid's bow indentation
        float cupid = 0;
        if (mp->cupid_depth > 0.01f) {
            float cd = 1.0f - fabsf(t - 0.5f) * 2.0f;
            cupid = cd * cd * mp->cupid_depth * 5.0f;
        }

        float upper_y = (1 - t) * (1 - t) * corner_y
                      + 2 * (1 - t) * t * (uly - smile_lift + cupid)
                      + t * t * corner_y;
        float lower_y = (1 - t) * (1 - t) * corner_y
                      + 2 * (1 - t) * t * (lly + openness_offset)
                      + t * t * corner_y;

        // Upper lip line
        float du = fabsf(y - upper_y);
        if (du < 1.5f) { buf[x] = sp->pal[PAL_MOUTH]; continue; }
        // Lower lip line
        float dl = fabsf(y - lower_y);
        if (dl < 1.5f) { buf[x] = sp->pal[PAL_MOUTH]; continue; }
        // Fill between lips with black
        if (mp->openness > 0.06f && y > upper_y + 1.5f && y < lower_y - 1.5f) {
            buf[x] = sp->pal[PAL_BG];
        }
    }
}

/* ── draw_blush: short curved arcs under each eye ─────────── */
static void draw_blush(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    float level = st->decor.blush;
    if (level <= 0.01f) return;
    const uint16_t *pal = sp->pal;

    int blush_cy = CENTER_Y + (int)sp->blush_y_offset;

    for (int side = 0; side < 2; side++) {
        int bx = (side == 0) ? CENTER_X - 55 : CENTER_X + 55;
        for (int i = 0; i < 3; i++) {
            int cx = bx + (i - 1) * 8;
            int cy = blush_cy + i * 3;
            float rx = 4.5f, ry = 2.8f * level;
            if (y < cy - (int)ry - 1 || y > cy + (int)ry + 1) continue;
            for (int x = cx - (int)rx - 1; x <= cx + (int)rx + 1; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                float ex = (float)(x - cx) / rx;
                float ey = (float)(y - cy) / ry;
                float e = ex * ex + ey * ey;
                // draw as ring: between inner and outer ellipse edge
                if (e < 1.15f && e > 0.45f) {
                    buf[x] = pal[PAL_BLUSH];
                }
            }
        }
    }
}

/* ── draw_decor_overlay: white outline tears/stars/sweat/sparkle */
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
            float radii[2] = {5.0f, 5.5f};
            for (int i = 0; i < 2; i++) {
                float r = radii[i] - dy * 0.05f;
                if (r < 1.0f) continue;
                float r_sq = r * r;
                int sx = positions[i], sy = tear_cy + 5;
                for (int x = sx - (int)r - 2; x <= sx + (int)r + 2; x++) {
                    if (x < 0 || x >= SCREEN_W) continue;
                    float d_sq = dist_sq(x, y, sx, sy);
                    if (d_sq < r_sq && fabsf(sqrtf(d_sq) - r) < 1.2f) {
                        buf[x] = pal[PAL_TEAR];
                    }
                }
            }
        }
    }

    // ── Stars: white star shapes (cross + diagonals) ──
    if (dp->stars > 0.01f) {
        static const int star_pos[4][2] = {
            {CENTER_X - 20, CENTER_Y - 15}, {CENTER_X + 20, CENTER_Y - 15},
            {CENTER_X - 35, CENTER_Y + 5},   {CENTER_X + 35, CENTER_Y + 5},
        };
        for (int i = 0; i < 4; i++) {
            int sx = star_pos[i][0], sy = star_pos[i][1];
            if (y < sy - 6 || y > sy + 6) continue;
            for (int x = sx - 6; x <= sx + 6; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                float d_sq = dist_sq(x, y, sx, sy);
                if (d_sq < 25.0f) {
                    float adx = fabsf((float)(x - sx));
                    float ady = fabsf((float)(y - sy));
                    bool on_cross = (adx < 1.2f && ady < 5.0f) || (ady < 1.2f && adx < 5.0f);
                    bool on_diag = fabsf(adx - ady) < 1.2f && adx < 3.5f && ady < 3.5f;
                    if (on_cross || on_diag) buf[x] = pal[PAL_STAR];
                }
            }
        }
    }

    // ── Sweat drops: white outline ──
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
    if (dp->sparkle > 0.01f) {
        static const int sparkle_pos[6][2] = {
            {CENTER_X - 45, CENTER_Y - 40}, {CENTER_X + 45, CENTER_Y - 40},
            {CENTER_X - 50, CENTER_Y + 35}, {CENTER_X + 50, CENTER_Y + 35},
            {CENTER_X - 25, CENTER_Y - 50}, {CENTER_X + 25, CENTER_Y - 50},
        };
        for (int i = 0; i < 6; i++) {
            int sx = sparkle_pos[i][0], sy = sparkle_pos[i][1];
            if (y < sy - 3 || y > sy + 3) continue;
            if (dp->sparkle < 0.3f) continue;
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

const sprite_set_t SPRITE_LINEART = {
    .name = "lineart",
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
    .pal = PALETTE_LINEART,
};
