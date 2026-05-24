#include "sprite_pixel.h"
#include "face_palette.h"
#include "face_common.h"
#include <math.h>
#include <stdlib.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120
#define PIXEL_GRID 4

/* ── Pixel face: solid black background ─────────────────────── */
static void draw_face(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const uint16_t *pal = sp->pal;
    for (int x = 0; x < SCREEN_W; x++) {
        buf[x] = pal[PAL_BG];
    }
}

/* ── Pixel eye: blocky b&w squares ──────────────────────────── */
static void draw_eye_impl(int y, const eye_params_t *ep, int eye_cx, int eye_cy,
                          const uint16_t *pal, uint16_t *buf) {
    const int eye_sz = 32;
    int fy = y - eye_cy;
    if (fy < -eye_sz - 2 || fy > eye_sz + 2) return;

    int qy = (y / PIXEL_GRID) * PIXEL_GRID;

    float lid_open = 1.0f - (ep->top_lid_mid.dy * 1.4f);
    if (lid_open < 0.05f) lid_open = 0.05f;
    if (lid_open > 1.0f) lid_open = 1.0f;

    int top_lid_y = eye_cy + (int)(-(eye_sz - 10) * (1.0f - lid_open));
    int bot_lid_y = eye_cy + (int)((eye_sz - 10) * (1.0f - lid_open));
    int top_lid_qy = (top_lid_y / PIXEL_GRID) * PIXEL_GRID;

    int iris_qx = ((int)(eye_cx + ep->iris_center.dx * 7.0f) / PIXEL_GRID) * PIXEL_GRID;
    int iris_qy = ((int)(eye_cy + ep->iris_center.dy * 7.0f) / PIXEL_GRID) * PIXEL_GRID;
    int iris_r = 24;

    int x_start = eye_cx - eye_sz - 2; if (x_start < 0) x_start = 0;
    int x_end   = eye_cx + eye_sz + 2; if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    for (int x = x_start; x <= x_end; x++) {
        float fx = x - eye_cx;
        float r_sq = fx * fx + fy * fy;
        if (r_sq >= eye_sz * eye_sz) continue;

        /* Eyelid occlusion */
        if (y < top_lid_y || y > bot_lid_y) continue;

        int qx = (x / PIXEL_GRID) * PIXEL_GRID;
        int dx = qx - iris_qx;
        int dy = qy - iris_qy;
        int iris_dist_sq = dx * dx + dy * dy;

        if (iris_dist_sq < iris_r * iris_r) {
            buf[x] = pal[PAL_IRIS]; /* white iris */

            /* Black limbal ring at iris edge */
            if (ep->iris_detail > 0.01f) {
                int ring_inner = (iris_r - PIXEL_GRID) * (iris_r - PIXEL_GRID);
                if (iris_dist_sq > ring_inner) {
                    buf[x] = pal[PAL_PUPIL];
                }
            }

            /* Black pupil (16×16 px block) */
            int pupil_hw = PIXEL_GRID * 2;
            if (abs(qx - iris_qx) <= pupil_hw && abs(qy - iris_qy) <= pupil_hw) {
                buf[x] = pal[PAL_PUPIL];

                /* White shine inside pupil (upper-right) */
                if (abs(qx - (iris_qx + PIXEL_GRID)) <= PIXEL_GRID / 2 &&
                    abs(qy - (iris_qy - PIXEL_GRID)) <= PIXEL_GRID / 2) {
                    buf[x] = pal[PAL_SHINE];
                }
            }
        } else {
            buf[x] = pal[PAL_SCLERA]; /* white sclera around iris */
        }

        /* Eyelash: black dashes at top lid edge */
        if (ep->eyelash > 0.01f && qy == top_lid_qy) {
            if ((abs(qx) / PIXEL_GRID) % 2 == 0) {
                buf[x] = pal[PAL_PUPIL];
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

/* ── Pixel brow: white blocky bar ───────────────────────────── */
static void draw_brow_impl(int y, const brow_params_t *bp, int eye_cx, int eye_cy,
                           const uint16_t *pal, float brow_y_offset, uint16_t *buf) {
    int qy = (y / PIXEL_GRID) * PIXEL_GRID;
    int brow_y_px = eye_cy + brow_y_offset;
    int brow_qy = ((int)brow_y_px / PIXEL_GRID) * PIXEL_GRID;

    if (qy < brow_qy - PIXEL_GRID || qy > brow_qy + PIXEL_GRID) return;

    int qx_start = eye_cx - 30;
    int qx_end   = eye_cx + 30;
    for (int x = qx_start; x <= qx_end; x++) {
        int qx = (x / PIXEL_GRID) * PIXEL_GRID;
        if (fabsf((float)(qy - brow_qy)) <= PIXEL_GRID) {
            buf[qx] = pal[PAL_BROW];
            if (qx + 1 < SCREEN_W) buf[qx + 1] = pal[PAL_BROW];
            if (qx + 2 < SCREEN_W) buf[qx + 2] = pal[PAL_BROW];
            if (qx + 3 < SCREEN_W) buf[qx + 3] = pal[PAL_BROW];
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

/* ── Pixel mouth: blocky white line ─────────────────────────── */
static void draw_mouth(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const mouth_params_t *mp = &st->mouth;
    int mouth_cy = CENTER_Y + (int)sp->mouth_y_center;
    const uint16_t *pal = sp->pal;

    float half_width = 22.0f;
    int x_start = CENTER_X - (int)half_width;
    int x_end   = CENTER_X + (int)half_width;

    for (int x = x_start; x <= x_end; x++) {
        if (x < 0 || x >= SCREEN_W) continue;

        float t = (float)(x - x_start) / (x_end - x_start);
        float mouth_y = mouth_cy + mp->openness * 12.0f * sinf(t * 3.14159f);

        /* Cupid's bow: center dip */
        if (mp->cupid_depth > 0.01f) {
            float center_t = 1.0f - fabsf(t - 0.5f) * 2.0f;
            mouth_y += center_t * center_t * mp->cupid_depth * 3.0f;
        }

        int mqy = ((int)mouth_y / PIXEL_GRID) * PIXEL_GRID;
        int qy = (y / PIXEL_GRID) * PIXEL_GRID;

        /* Thicker line when mouth is open */
        int thickness = (mp->openness > 0.2f) ? PIXEL_GRID * 2 : PIXEL_GRID;
        if (abs(qy - mqy) <= thickness) {
            buf[x] = pal[PAL_MOUTH];
            if (x + 1 < SCREEN_W) buf[x + 1] = pal[PAL_MOUTH];
        }

        /* Tooth: extra white block above mouth when open */
        if (mp->tooth_show > 0.01f && mp->openness > 0.08f) {
            int tooth_qy = mqy - PIXEL_GRID;
            if (abs(qy - tooth_qy) <= PIXEL_GRID / 2) {
                buf[x] = pal[PAL_MOUTH];
            }
        }

    }
}

/* ── Pixel blush: solid white squares ───────────────────────── */
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
            if (x < 0 || x >= SCREEN_W) continue;
            int qx = (x / PIXEL_GRID) * PIXEL_GRID;
            if (abs(qx - base_qx) <= PIXEL_GRID * 2 && abs(qy - blush_qy) <= PIXEL_GRID * 2) {
                buf[x] = pal[PAL_BLUSH];
            }
        }
    }
}

/* ── Pixel decor: white blocky stars ────────────────────────── */
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
                buf[x] = pal[PAL_STAR];
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
            if (qx_s >= 0 && qx_s < SCREEN_W) {
                buf[qx_s] = pal[PAL_SHINE];
            }
        }
    }
}

/* ── draw_props ─────────────────────────────────────────────── */
static void draw_props(int y, const face_state_t *st,
                       const sprite_set_t *sp, uint16_t *buf) {
    for (int i = 0; i < st->decor.prop_count; i++) {
        const prop_instance_t *p = &st->decor.props[i];
        if (p->opacity <= 0.5f) continue; /* threshold instead of blend */

        float r = 100.0f * p->distance;
        float px = CENTER_X + r * cosf(p->angle);
        float py = CENTER_Y - r * sinf(p->angle);
        float sz = 10.0f + p->scale * 12.0f;

        uint16_t color;
        switch (p->type) {
        case PROP_HEART:      color = sp->pal[PAL_BLUSH]; break;
        case PROP_TEACUP:     color = sp->pal[PAL_SKIN];  break;
        case PROP_HAND:       color = sp->pal[PAL_SKIN];  break;
        case PROP_STAR_SMALL: color = sp->pal[PAL_STAR];  break;
        case PROP_SWEAT_DROP: color = sp->pal[PAL_TEAR];  break;
        case PROP_FINGER_HEART: color = sp->pal[PAL_SCLERA]; break;
        default: continue;
        }

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
    .draw_props = draw_props,
    .pal = PALETTE_PIXEL,
};
