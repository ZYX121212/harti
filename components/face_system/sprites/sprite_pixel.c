#include "sprite_pixel.h"
#include "face_palette.h"
#include <math.h>
#include <stdlib.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120
#define PIXEL_GRID 4

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

/* ── Pixel-face: quantized background ──────────────────────── */
static void draw_face(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const uint16_t *pal = sp->pal;
    int py = (y / PIXEL_GRID) * PIXEL_GRID;
    int dy = py - CENTER_Y;
    float sx = 1.0f + st->face.squash_x * 0.3f;
    float sy = 1.0f + st->face.stretch_y * 0.3f;
    if (sx < 0.5f) sx = 0.5f;
    if (sy < 0.5f) sy = 0.5f;
    for (int x = 0; x < SCREEN_W; x++) {
        int px = (x / PIXEL_GRID) * PIXEL_GRID;
        int dx = px - CENTER_X;
        float dx_s = (float)dx / sx;
        float dy_s = (float)dy / sy;
        int d = (int)sqrtf(dx_s * dx_s + dy_s * dy_s);
        float t = (float)d / 130.0f;
        if (t > 1.0f) t = 1.0f;
        buf[x] = blend_colors(pal[PAL_BG], pal[PAL_BG_EDGE], t);
    }
}

/* ── Pixel eye: blocky squares ────────────────────────────── */
static void draw_eye_impl(int y, const eye_params_t *ep, int eye_cx, int eye_cy,
                          const uint16_t *pal, uint16_t *buf) {
    const int eye_sz = 32;
    int fy = y - eye_cy;
    if (fy < -eye_sz - 2 || fy > eye_sz + 2) return;

    int x_start = eye_cx - eye_sz - 2;
    int x_end   = eye_cx + eye_sz + 2;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    float lid_open = 1.0f - (ep->top_lid_mid.dy * 1.4f);
    if (lid_open < 0.05f) lid_open = 0.05f;
    if (lid_open > 1.0f) lid_open = 1.0f;

    // Quantize iris position
    int iris_qx = ((int)(eye_cx + ep->iris_center.dx * 7.0f) / PIXEL_GRID) * PIXEL_GRID;
    int iris_qy = ((int)(eye_cy + ep->iris_center.dy * 7.0f) / PIXEL_GRID) * PIXEL_GRID;
    int iris_r = 24;

    for (int x = x_start; x <= x_end; x++) {
        float fx = x - eye_cx;
        float r_sq = fx * fx + fy * fy;
        if (r_sq >= eye_sz * eye_sz) continue;

        // Simple top lid
        float top_lid = -(eye_sz - 10) * (1.0f - lid_open);
        float bot_lid = (eye_sz - 10) * (1.0f - lid_open);

        /* eyelash: darker pixels above top lid */
        if (ep->eyelash > 0.01f && fy >= top_lid - PIXEL_GRID && fy < top_lid + 2.0f) {
            int lash_qx = (x / PIXEL_GRID) * PIXEL_GRID;
            if ((lash_qx / PIXEL_GRID) % 2 == 0 && fy < top_lid + 1.0f) {
                buf[x] = blend_colors(buf[x], pal[PAL_PUPIL], ep->eyelash * 0.35f);
            }
        }
        if (fy < top_lid || fy > bot_lid) continue;

        // Pixel-block iris: pure color, no gradient
        int qx = (x / PIXEL_GRID) * PIXEL_GRID;
        int qy = (y / PIXEL_GRID) * PIXEL_GRID;
        int dx = qx - iris_qx;
        int dy = qy - iris_qy;
        int iris_dist_sq = dx * dx + dy * dy;
        if (iris_dist_sq < iris_r * iris_r) {
            int iris_edge = (iris_r - PIXEL_GRID) * (iris_r - PIXEL_GRID);
            // Pupil: 2x2 grid black block in center
            if (abs(qx - iris_qx) <= PIXEL_GRID && abs(qy - iris_qy) <= PIXEL_GRID) {
                buf[x] = pal[PAL_PUPIL];
            } else if (abs(qx - iris_qx) <= PIXEL_GRID * 2 && abs(qy - iris_qy - PIXEL_GRID * 2) <= PIXEL_GRID) {
                // Shine: 1 grid white block
                buf[x] = blend_colors(pal[PAL_IRIS], pal[PAL_SHINE], 0.7f);
            } else if (ep->iris_detail > 0.01f && iris_dist_sq > iris_edge) {
                // iris_detail: dark ring at iris edge
                buf[x] = blend_colors(pal[PAL_IRIS], pal[PAL_PUPIL], ep->iris_detail * 0.5f);
            } else {
                buf[x] = pal[PAL_IRIS];
            }
        }
    }
}

static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[0].position.dy * 15.0f);
    draw_eye_impl(y, &st->eye[0], cx, eye_cy, sp->pal, buf);
}
static void draw_eye_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[1].position.dy * 15.0f);
    draw_eye_impl(y, &st->eye[1], cx, eye_cy, sp->pal, buf);
}

/* ── Pixel brow: blocky line ──────────────────────────────── */
static void draw_brow_impl(int y, const brow_params_t *bp, int eye_cx, int eye_cy,
                           const uint16_t *pal, float brow_y_offset, uint16_t *buf) {
    int qy = (y / PIXEL_GRID) * PIXEL_GRID;
    float brow_y_px = eye_cy + brow_y_offset;
    int brow_qy = ((int)brow_y_px / PIXEL_GRID) * PIXEL_GRID;
    // Only draw on grid-aligned rows
    if (qy < brow_qy - PIXEL_GRID || qy > brow_qy + PIXEL_GRID) return;

    int qx_start = eye_cx - 30;
    int qx_end   = eye_cx + 30;
    for (int x = qx_start; x <= qx_end; x++) {
        int qx = (x / PIXEL_GRID) * PIXEL_GRID;
        if (fabsf((float)(qy - brow_qy)) <= PIXEL_GRID) {
            buf[qx] = blend_colors(buf[qx], pal[PAL_BROW], 0.8f);
            if (qx + 1 < SCREEN_W) buf[qx + 1] = buf[qx];
            if (qx + 2 < SCREEN_W) buf[qx + 2] = buf[qx];
            if (qx + 3 < SCREEN_W) buf[qx + 3] = buf[qx];
        }
    }
}

static void draw_brow_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[0].position.dy * 15.0f);
    draw_brow_impl(y, &st->brow[0], cx, eye_cy, sp->pal, sp->brow_y_offset, buf);
}
static void draw_brow_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = CENTER_Y + (int)(st->eye[1].position.dy * 15.0f);
    draw_brow_impl(y, &st->brow[1], cx, eye_cy, sp->pal, sp->brow_y_offset, buf);
}

/* ── Pixel mouth: 2px horizontal line ─────────────────────── */
static void draw_mouth(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const mouth_params_t *mp = &st->mouth;
    int mouth_cy = CENTER_Y + (int)sp->mouth_y_center;
    const uint16_t *pal = sp->pal;
    int qy = (y / PIXEL_GRID) * PIXEL_GRID;
    int mqy = (mouth_cy / PIXEL_GRID) * PIXEL_GRID;

    if (qy < mqy - PIXEL_GRID || qy > mqy + PIXEL_GRID) return;

    float half_width = 22.0f;
    int x_start = CENTER_X - (int)half_width;
    int x_end   = CENTER_X + (int)half_width;

    for (int x = x_start; x <= x_end; x++) {
        int qx = (x / PIXEL_GRID) * PIXEL_GRID;
        float t = (float)(x - x_start) / (x_end - x_start);
        float mouth_y = mqy + mp->openness * 12.0f * sinf(t * 3.14159f);
        /* cupid_depth: add a small dip at center */
        if (mp->cupid_depth > 0.01f) {
            float center_t = 1.0f - fabsf(t - 0.5f) * 2.0f;
            mouth_y += center_t * center_t * mp->cupid_depth * 3.0f;
        }
        if (fabsf(qy - mouth_y) <= PIXEL_GRID * 1.5f) {
            buf[x] = blend_colors(buf[x], pal[PAL_MOUTH], 0.9f);
            if (x + 1 < SCREEN_W) buf[x + 1] = buf[x];
        }
        /* tooth_show: white pixel line below mouth when open */
        if (mp->tooth_show > 0.01f && mp->openness > 0.05f) {
            float tooth_y = mouth_y - PIXEL_GRID;
            if (fabsf(qy - tooth_y) <= PIXEL_GRID) {
                buf[x] = blend_colors(buf[x], pal[PAL_SCLERA], mp->tooth_show * 0.6f);
                if (x + 1 < SCREEN_W) buf[x + 1] = buf[x];
            }
        }
    }
}

/* ── Pixel blush: grid-aligned squares ────────────────────── */
static void draw_blush(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    float level = st->decor.blush;
    if (level <= 0) return;
    const uint16_t *pal = sp->pal;
    int qy = (y / PIXEL_GRID) * PIXEL_GRID;
    int blush_qy = ((CENTER_Y + 35) / PIXEL_GRID) * PIXEL_GRID;
    if (abs(qy - blush_qy) > PIXEL_GRID * 3) return;

    for (int side = -1; side <= 1; side += 2) {
        int base_qx = (CENTER_X + side * 55) / PIXEL_GRID * PIXEL_GRID;
        for (int x = base_qx - PIXEL_GRID * 3; x <= base_qx + PIXEL_GRID * 3; x++) {
            int qx = (x / PIXEL_GRID) * PIXEL_GRID;
            if (abs(qx - base_qx) <= PIXEL_GRID * 2 && abs(qy - blush_qy) <= PIXEL_GRID * 2) {
                buf[x] = blend_colors(buf[x], pal[PAL_BLUSH], level * 0.6f);
            }
        }
    }
}

/* ── Pixel decor: pixellated stars ────────────────────────── */
static void draw_decor_overlay(int y, const face_state_t *st,
                                const sprite_set_t *sp, uint16_t *buf) {
    const decor_params_t *dp = &st->decor;
    const uint16_t *pal = sp->pal;
    int qy = (y / PIXEL_GRID) * PIXEL_GRID;

    if (dp->stars > 0) {
        for (int i = 0; i < 4; i++) {
            int sx = CENTER_X + ((i % 2) * 2 - 1) * (25 + (i / 2) * 15);
            int sy = CENTER_Y - 20 + (i / 2) * 15;
            int sqx = (sx / PIXEL_GRID) * PIXEL_GRID;
            int sqy = (sy / PIXEL_GRID) * PIXEL_GRID;
            if (qy != sqy) continue;
            int qx_s = sqx - PIXEL_GRID, qx_e = sqx + PIXEL_GRID;
            if (qx_s < 0) qx_s = 0;
            if (qx_e >= SCREEN_W) qx_e = SCREEN_W - 1;
            for (int x = qx_s; x <= qx_e; x++) {
                buf[x] = blend_colors(buf[x], pal[PAL_STAR], dp->stars * 0.8f);
            }
        }
    }

    if (dp->sparkle > 0) {
        for (int i = 0; i < 4; i++) {
            int sx = CENTER_X + ((i % 2) * 2 - 1) * 40;
            int sy = CENTER_Y - 40 + (i / 2) * 15;
            int sqy = (sy / PIXEL_GRID) * PIXEL_GRID;
            if (qy != sqy) continue;
            int qx_s = (sx / PIXEL_GRID) * PIXEL_GRID;
            buf[qx_s] = blend_colors(buf[qx_s], pal[PAL_SHINE], dp->sparkle * 0.5f);
        }
    }
}

/* ── Sprite definition ──────────────────────────────────────── */
const sprite_set_t SPRITE_PIXEL = {
    .name = "pixel",
    .eye_radius = 32.0f,
    .eye_half_spacing = 26.0f,
    .mouth_y_center = 46.0f,
    .brow_y_offset = -36.0f,
    .blush_y_offset = 35.0f,
    .draw_face = draw_face,
    .draw_blush = draw_blush,
    .draw_mouth = draw_mouth,
    .draw_eye_left = draw_eye_left,
    .draw_eye_right = draw_eye_right,
    .draw_brow_left = draw_brow_left,
    .draw_brow_right = draw_brow_right,
    .draw_decor_overlay = draw_decor_overlay,
    .pal = PALETTE_PIXEL,
};
