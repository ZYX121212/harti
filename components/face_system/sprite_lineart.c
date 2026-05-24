#include "sprite_lineart.h"
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

/* ── draw_face: solid black background ───────────────────── */
static void draw_face(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    (void)st;
    for (int x = 0; x < SCREEN_W; x++) {
        buf[x] = sp->pal[PAL_BG];
    }
}

/* ── draw_eye: white outline style ───────────────────────── */
static void draw_eye_lineart(int y, const eye_params_t *ep, float eye_r,
                             int eye_cx, int eye_cy, const uint16_t *pal, uint16_t *buf) {
    float fy = y - eye_cy;
    if (fy < -eye_r - 3 || fy > eye_r + 3) return;

    int x_start = eye_cx - (int)eye_r - 3;
    int x_end   = eye_cx + (int)eye_r + 3;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    float iris_r = eye_r * 0.55f;
    float iris_cx = ep->iris_center.dx * 7.0f;
    float iris_cy = ep->iris_center.dy * 7.0f;
    float iris_r_sq = iris_r * iris_r;

    float pupil_base_r = iris_r * 0.45f;
    float pupil_r = pupil_base_r * ep->pupil_scale;
    float pupil_r_sq = pupil_r * pupil_r;

    float lid_open = 1.0f - (ep->top_lid_mid.dy * 1.4f);
    if (lid_open < 0.05f) lid_open = 0.05f;
    if (lid_open > 1.0f) lid_open = 1.0f;

    float base_top = -eye_r * lid_open;
    float base_bot =  eye_r * lid_open;
    float eye_r_sq = eye_r * eye_r;

    // Shine highlight positions (relative to iris)
    float sh_cx = iris_cx - 7.0f, sh_cy = iris_cy - 8.0f;
    float sh_r = 2.5f, sh_r_sq = sh_r * sh_r;
    float sh2_cx = iris_cx + 4.0f, sh2_cy = iris_cy - 4.0f;
    float sh2_r = 1.8f, sh2_r_sq = sh2_r * sh2_r;

    for (int x = x_start; x <= x_end; x++) {
        float fx = x - eye_cx;
        float r_sq = fx * fx + fy * fy;

        if (r_sq >= eye_r_sq) continue;

        float arc = 1.0f - (fx * fx) / eye_r_sq;
        float inner_adj = ep->inner_corner.dy * 5.0f;
        float outer_adj = ep->outer_corner.dy * 5.0f;
        float corner_adj = inner_adj + (outer_adj - inner_adj) * ((fx + eye_r) / (2.0f * eye_r));
        float top_lid = base_top + (5.0f * lid_open * arc) + corner_adj * 0.5f;
        float bot_lid = base_bot - (3.0f * lid_open * arc) - corner_adj * 0.3f;

        if (fy < top_lid || fy > bot_lid) continue;

        bool drawn = false;

        // 1. Eye circular outline (outer edge)
        float edge_dist = eye_r - sqrtf(r_sq);
        if (edge_dist < 1.8f) {
            buf[x] = pal[PAL_SCLERA];
            drawn = true;
        }

        // 2. Lid lines (top and bottom)
        float dist_top = fabsf(fy - top_lid);
        float dist_bot = fabsf(fy - bot_lid);
        if (!drawn && (dist_top < 1.5f || dist_bot < 1.5f)) {
            buf[x] = pal[PAL_SCLERA];
            drawn = true;
        }

        // 3. Iris circle outline
        float iris_d_sq = dist_sq(fx, fy, iris_cx, iris_cy);
        if (iris_d_sq < iris_r_sq) {
            float iris_edge = fabsf(sqrtf(iris_d_sq) - iris_r);
            if (!drawn && iris_edge < 1.8f) {
                buf[x] = pal[PAL_IRIS];
                drawn = true;
            }
        }

        // 4. Filled pupil
        if (!drawn && iris_d_sq < pupil_r_sq) {
            buf[x] = pal[PAL_PUPIL];
            drawn = true;
        }

        // 5. Shine highlights (small dots within iris)
        if (!drawn && iris_d_sq < iris_r_sq) {
            if (dist_sq(fx, fy, sh_cx, sh_cy) < sh_r_sq) {
                buf[x] = pal[PAL_SHINE];
                drawn = true;
            }
            if (!drawn && dist_sq(fx, fy, sh2_cx, sh2_cy) < sh2_r_sq) {
                buf[x] = pal[PAL_SHINE];
                drawn = true;
            }
        }
    }
}

static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)sp->eye_half_spacing;
    int eye_cy = CENTER_Y;
    draw_eye_lineart(y, &st->eye[0], sp->eye_radius, eye_cx, eye_cy, sp->pal, buf);
}

static void draw_eye_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing;
    int eye_cy = CENTER_Y;
    draw_eye_lineart(y, &st->eye[1], sp->eye_radius, eye_cx, eye_cy, sp->pal, buf);
}

/* ── draw_brow: thin white bezier line ───────────────────── */
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
        float dist = fabsf(y - curve_y);
        if (dist < half_thick) {
            buf[x] = pal[PAL_BROW];
        }
    }
}

static void draw_brow_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)sp->eye_half_spacing;
    draw_brow_lineart(y, &st->brow[0], eye_cx, CENTER_Y, sp, sp->pal, buf);
}

static void draw_brow_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing;
    draw_brow_lineart(y, &st->brow[1], eye_cx, CENTER_Y, sp, sp->pal, buf);
}

/* ── draw_mouth: white outline ───────────────────────────── */
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

        float corner_y = (1 - t) * lcy + t * rcy;
        float smile_lift = sinf(t * 3.14159f) * 3.0f;
        float upper_y = (1 - t) * (1 - t) * corner_y + 2 * (1 - t) * t * (uly - smile_lift) + t * t * corner_y;
        float lower_y = (1 - t) * (1 - t) * corner_y + 2 * (1 - t) * t * (lly + openness_offset) + t * t * corner_y;

        // Draw upper lip line
        float dist_upper = fabsf(y - upper_y);
        if (dist_upper < 1.5f) {
            buf[x] = sp->pal[PAL_MOUTH];
            continue;
        }

        // Draw lower lip line
        float dist_lower = fabsf(y - lower_y);
        if (dist_lower < 1.5f) {
            buf[x] = sp->pal[PAL_MOUTH];
            continue;
        }

        // Fill gap between lips with black (prevents artifacts)
        if (mp->openness > 0.08f && y > upper_y + 1.5f && y < lower_y - 1.5f) {
            buf[x] = sp->pal[PAL_BG];
        }
    }
}

/* ── draw_blush: white hash marks under eyes ─────────────── */
static void draw_blush(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    float level = st->decor.blush;
    if (level <= 0.01f) return;
    const uint16_t *pal = sp->pal;

    int blush_cy = CENTER_Y + (int)sp->blush_y_offset;
    int left_cx  = CENTER_X - 58;
    int right_cx = CENTER_X + 58;

    // Three short diagonal lines under each eye
    for (int side = 0; side < 2; side++) {
        int bx = (side == 0) ? left_cx : right_cx;
        float slope = (side == 0) ? 0.35f : -0.35f;

        for (int h = 0; h < 3; h++) {
            int hx = bx + (h - 1) * 9;
            int hy = blush_cy + (h % 2) * 4;

            // Line: y - hy = slope * (x - hx)
            // For given y: x = hx + (y - hy) / slope
            if (fabsf(slope) < 0.01f) continue;
            float x_on_line = hx + (float)(y - hy) / slope;
            // For given x: check if y matches
            int xi = (int)(x_on_line + 0.5f);
            int dx = xi - hx;
            if (dx >= -4 && dx <= 4 && xi >= 0 && xi < SCREEN_W) {
                buf[xi] = pal[PAL_BLUSH];
            }
        }
    }
}

/* ── draw_decor_overlay: white outline tears/stars/sweat/sparkle */
static void draw_decor_overlay(int y, const face_state_t *st,
                                const sprite_set_t *sp, uint16_t *buf) {
    const decor_params_t *dp = &st->decor;
    const uint16_t *pal = sp->pal;

    // Tears (white droplet outlines below eyes)
    if (dp->tears > 0.01f) {
        int tear_cy = CENTER_Y + 30;
        float dy = y - tear_cy;
        if (dy > 0 && dy < 30 * dp->tears) {
            int lcx = CENTER_X - 35, rcx = CENTER_X + 35;
            float r_left = 5.0f - dy * 0.06f;
            float r_right = 5.5f - dy * 0.06f;
            for (int x = lcx - (int)r_left - 2; x <= lcx + (int)r_left + 2; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                float d_sq = dist_sq(x, y, lcx, tear_cy + 5);
                float r_sq = r_left * r_left;
                if (d_sq < r_sq) {
                    float edge = fabsf(sqrtf(d_sq) - r_left);
                    if (edge < 1.2f) buf[x] = pal[PAL_TEAR];
                }
            }
            for (int x = rcx - (int)r_right - 2; x <= rcx + (int)r_right + 2; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                float d_sq = dist_sq(x, y, rcx, tear_cy + 8);
                float r_sq = r_right * r_right;
                if (d_sq < r_sq) {
                    float edge = fabsf(sqrtf(d_sq) - r_right);
                    if (edge < 1.2f) buf[x] = pal[PAL_TEAR];
                }
            }
        }
    }

    // Stars (white star shapes: cross + diagonals)
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
                float r = 5.0f;
                if (d_sq < r * r) {
                    float adx = fabsf((float)(x - sx));
                    float ady = fabsf((float)(y - sy));
                    bool on_cross = (adx < 1.2f && ady < r) || (ady < 1.2f && adx < r);
                    bool on_diag = fabsf(adx - ady) < 1.2f && adx < r * 0.7f && ady < r * 0.7f;
                    if (on_cross || on_diag) {
                        buf[x] = pal[PAL_STAR];
                    }
                }
            }
        }
    }

    // Sweat drops (white outline droplet)
    if (dp->sweat > 0.01f) {
        int sx = CENTER_X + 55, sy = CENTER_Y - 45;
        float r = 4.0f * dp->sweat;
        for (int x = sx - (int)r - 2; x <= sx + (int)r + 2; x++) {
            if (x < 0 || x >= SCREEN_W) continue;
            float d_sq = dist_sq(x, y, sx, sy);
            float r_sq = r * r;
            if (d_sq < r_sq) {
                float edge = fabsf(sqrtf(d_sq) - r);
                if (edge < 1.2f) buf[x] = pal[PAL_TEAR];
            }
        }
    }

    // Sparkle (tiny white dots near eyes)
    if (dp->sparkle > 0.01f) {
        static const int sparkle_pos[6][2] = {
            {CENTER_X - 45, CENTER_Y - 40}, {CENTER_X + 45, CENTER_Y - 40},
            {CENTER_X - 50, CENTER_Y + 35}, {CENTER_X + 50, CENTER_Y + 35},
            {CENTER_X - 25, CENTER_Y - 50}, {CENTER_X + 25, CENTER_Y - 50},
        };
        for (int i = 0; i < 6; i++) {
            int sx = sparkle_pos[i][0], sy = sparkle_pos[i][1];
            if (y < sy - 3 || y > sy + 3) continue;
            for (int x = sx - 3; x <= sx + 3; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                float d_sq = dist_sq(x, y, sx, sy);
                if (d_sq < 4.0f && dp->sparkle > 0.3f) {
                    buf[x] = pal[PAL_SHINE];
                }
            }
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
    .pal = PALETTE_LINEART,
};
