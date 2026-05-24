#include "sprite_pig.h"
#include "face_palette.h"
#include "face_common.h"
#include <math.h>

#define SCREEN_W 240
#define CENTER_X 120
#define CENTER_Y 120

/* ════════════════════════════════════════════════════════════════
 *  Chibi pig face geometry
 * ══════════════════════════════════════════════════════════════ */

#define HEAD_CX  120.0f
#define HEAD_CY  125.0f
#define HEAD_R     90.0f

/* Snout: oval below eyes */
#define SNOUT_CX  120.0f
#define SNOUT_CY  155.0f
#define SNOUT_RX   26.0f
#define SNOUT_RY   17.0f
#define NOSTRIL_R   4.0f
#define NOSTRIL_DX  8.0f
#define NOSTRIL_DY  3.0f

/* Eyes: chibi oval */
#define EYE_CY          104.0f
#define EYE_HALF_SPACE   26.0f
#define EYE_RX            9.0f
#define EYE_RY           13.0f

/* Mouth */
#define MOUTH_Y    178.0f
#define MOUTH_HW    14.0f
#define MOUTH_DIP    5.0f

/* Blush */
#define BLUSH_CY  120.0f

/* Left ear: floppy triangle bezier points */
#define L_TIP_X   50.0f
#define L_TIP_Y   54.0f
#define L_OUT_X   36.0f
#define L_OUT_Y   20.0f
#define L_IN_X    76.0f
#define L_IN_Y    38.0f
#define L_CTRL_OUT_X 30.0f
#define L_CTRL_OUT_Y 28.0f
#define L_CTRL_IN_X  64.0f
#define L_CTRL_IN_Y  22.0f

/* Right ear (mirrored around 120) */
#define R_TIP_X  190.0f
#define R_TIP_Y   54.0f
#define R_OUT_X  204.0f
#define R_OUT_Y   20.0f
#define R_IN_X   164.0f
#define R_IN_Y    38.0f
#define R_CTRL_OUT_X 210.0f
#define R_CTRL_OUT_Y  28.0f
#define R_CTRL_IN_X  176.0f
#define R_CTRL_IN_Y   22.0f

/* ════════════════════════════════════════════════════════════════
 *  Helper: solve quadratic-bezier for x at a given y
 * ══════════════════════════════════════════════════════════════ */

static float bez_x_at_y(float x0, float y0, float x1, float y1,
                        float x2, float y2, float target_y) {
    float a = y0 - 2.0f * y1 + y2;
    float b = 2.0f * (y1 - y0);
    float c = y0 - target_y;
    float t;

    if (fabsf(a) < 0.0005f) {
        if (fabsf(b) < 0.0005f) return -1.0f;
        t = -c / b;
    } else {
        float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f) return -1.0f;
        float sd = sqrtf(disc);
        float t0 = (-b + sd) / (2.0f * a);
        float t1 = (-b - sd) / (2.0f * a);
        if      (t0 >= 0.0f && t0 <= 1.0f) t = t0;
        else if (t1 >= 0.0f && t1 <= 1.0f) t = t1;
        else return -1.0f;
    }
    if (t < 0.0f || t > 1.0f) return -1.0f;

    float s = 1.0f - t;
    return s * s * x0 + 2.0f * s * t * x1 + t * t * x2;
}

/* ════════════════════════════════════════════════════════════════
 *  draw_face: white head + floppy ears + outlined snout
 * ══════════════════════════════════════════════════════════════ */

static void draw_face(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    (void)st;
    const uint16_t *pal = sp->pal;

    /* 1. Fill entire row black */
    for (int x = 0; x < SCREEN_W; x++) buf[x] = pal[PAL_BG];

    /* 2. White filled head circle */
    fill_circle_scan(y, (int)HEAD_CX, (int)HEAD_CY, HEAD_R, pal[PAL_SKIN], buf, SCREEN_W);

    /* 3. Ears — bezier-bounded filled shapes, white fill + black edge */
    /* Left ear */
    if (y >= (int)L_OUT_Y - 2 && y <= (int)L_TIP_Y + 2) {
        float lx = bez_x_at_y(L_TIP_X, L_TIP_Y, L_CTRL_OUT_X, L_CTRL_OUT_Y,
                              L_OUT_X, L_OUT_Y, (float)y);
        float rx;
        if (y <= (int)L_IN_Y) {
            rx = bez_x_at_y(L_TIP_X, L_TIP_Y, L_CTRL_IN_X, L_CTRL_IN_Y,
                            L_IN_X, L_IN_Y, (float)y);
        } else {
            float t = (float)(y - L_IN_Y) / (L_OUT_Y - L_IN_Y);
            rx = L_IN_X + t * (L_OUT_X - L_IN_X);
        }
        if (lx >= 0.0f && rx >= 0.0f && lx < rx) {
            int xs = (int)lx, xe = (int)rx;
            if (xs < 0) xs = 0;
            if (xe >= SCREEN_W) xe = SCREEN_W - 1;
            for (int x = xs; x <= xe; x++) buf[x] = pal[PAL_SKIN];
            /* Black outline at bezier edges */
            for (int dx = -2; dx <= 2; dx++) {
                int olx = (int)lx + dx, orx = (int)rx + dx;
                if (olx >= 0 && olx < SCREEN_W) buf[olx] = pal[PAL_BG_EDGE];
                if (orx >= 0 && orx < SCREEN_W) buf[orx] = pal[PAL_BG_EDGE];
            }
        }
    }

    /* Right ear (mirrored) */
    if (y >= (int)R_OUT_Y - 2 && y <= (int)R_TIP_Y + 2) {
        float lx, rx;
        if (y <= (int)R_IN_Y) {
            rx = bez_x_at_y(R_TIP_X, R_TIP_Y, R_CTRL_OUT_X, R_CTRL_OUT_Y,
                            R_OUT_X, R_OUT_Y, (float)y);
            lx = bez_x_at_y(R_TIP_X, R_TIP_Y, R_CTRL_IN_X, R_CTRL_IN_Y,
                            R_IN_X, R_IN_Y, (float)y);
        } else {
            rx = bez_x_at_y(R_TIP_X, R_TIP_Y, R_CTRL_OUT_X, R_CTRL_OUT_Y,
                            R_OUT_X, R_OUT_Y, (float)y);
            float t = (float)(y - R_IN_Y) / (R_OUT_Y - R_IN_Y);
            lx = R_IN_X + t * (R_OUT_X - R_IN_X);
        }
        if (lx >= 0.0f && rx >= 0.0f && lx < rx) {
            int xs = (int)lx, xe = (int)rx;
            if (xs < 0) xs = 0;
            if (xe >= SCREEN_W) xe = SCREEN_W - 1;
            for (int x = xs; x <= xe; x++) buf[x] = pal[PAL_SKIN];
            for (int dx = -2; dx <= 2; dx++) {
                int olx = (int)lx + dx, orx = (int)rx + dx;
                if (olx >= 0 && olx < SCREEN_W) buf[olx] = pal[PAL_BG_EDGE];
                if (orx >= 0 && orx < SCREEN_W) buf[orx] = pal[PAL_BG_EDGE];
            }
        }
    }

    /* 4. Snout: white filled ellipse + black outline */
    float sny = (float)(y - (int)SNOUT_CY) / SNOUT_RY;
    if (fabsf(sny) <= 1.0f) {
        float sxspan = SNOUT_RX * sqrtf(1.0f - sny * sny);
        int sx0 = (int)(SNOUT_CX - sxspan);
        int sx1 = (int)(SNOUT_CX + sxspan);
        if (sx0 < 0) sx0 = 0;
        if (sx1 >= SCREEN_W) sx1 = SCREEN_W - 1;
        for (int x = sx0; x <= sx1; x++) buf[x] = pal[PAL_SKIN];
        /* Black outline ring at ellipse edge */
        for (int x = sx0; x <= sx1; x++) {
            float nx = (float)(x - SNOUT_CX) / SNOUT_RX;
            float e = nx * nx + sny * sny;
            if (e > 0.85f && e < 1.05f) buf[x] = pal[PAL_BG_EDGE];
        }
    }

    /* 5. Nostrils: black filled circles */
    fill_circle_scan(y, (int)(SNOUT_CX - NOSTRIL_DX), (int)(SNOUT_CY + NOSTRIL_DY),
                     NOSTRIL_R, pal[PAL_PUPIL], buf, SCREEN_W);
    fill_circle_scan(y, (int)(SNOUT_CX + NOSTRIL_DX), (int)(SNOUT_CY + NOSTRIL_DY),
                     NOSTRIL_R, pal[PAL_PUPIL], buf, SCREEN_W);
}

/* ════════════════════════════════════════════════════════════════
 *  Eyes: chibi oval with double shine
 * ══════════════════════════════════════════════════════════════ */

static void draw_eye_pig(int y, const eye_params_t *ep, int eye_cx, int eye_cy,
                          const uint16_t *pal, uint16_t *buf) {
    float fy = (float)(y - eye_cy);
    if (fabsf(fy) > EYE_RY + 2.0f) return;

    float pdx = ep->iris_center.dx * 3.5f;
    float pdy = ep->iris_center.dy * 3.5f;
    float pupil_r = 6.5f;
    float pupil_r_sq = pupil_r * pupil_r;
    float shine = ep->shine_intensity;

    int x_start = eye_cx - (int)EYE_RX - 3;
    int x_end   = eye_cx + (int)EYE_RX + 3;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    for (int x = x_start; x <= x_end; x++) {
        float fx = (float)(x - eye_cx);
        float e = (fx * fx) / (EYE_RX * EYE_RX) + (fy * fy) / (EYE_RY * EYE_RY);
        if (e > 1.0f) continue;

        buf[x] = pal[PAL_SCLERA];

        if (dist_sq(fx, fy, pdx, pdy) < pupil_r_sq) {
            float shine1_x = pdx - pupil_r * 0.35f;
            float shine1_y = pdy - pupil_r * 0.4f;
            float shine2_x = pdx + pupil_r * 0.3f;
            float shine2_y = pdy + pupil_r * 0.35f;
            float shine_r = 2.2f * sqrtf(shine);
            float shine_r_sq = shine_r * shine_r;
            float shine2_r = 1.5f * sqrtf(shine);
            float shine2_r_sq = shine2_r * shine2_r;

            if (shine > 0.1f &&
                (dist_sq(fx, fy, shine1_x, shine1_y) < shine_r_sq ||
                 dist_sq(fx, fy, shine2_x, shine2_y) < shine2_r_sq)) {
                buf[x] = pal[PAL_SHINE];
            } else {
                buf[x] = pal[PAL_PUPIL];
            }
        }
    }
}

static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)EYE_HALF_SPACE + (int)(st->eye[0].position.dx * 15.0f);
    int eye_cy = (int)EYE_CY + (int)(st->eye[0].position.dy * 15.0f);
    draw_eye_pig(y, &st->eye[0], eye_cx, eye_cy, sp->pal, buf);
}

static void draw_eye_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)EYE_HALF_SPACE + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = (int)EYE_CY + (int)(st->eye[1].position.dy * 15.0f);
    draw_eye_pig(y, &st->eye[1], eye_cx, eye_cy, sp->pal, buf);
}

/* ════════════════════════════════════════════════════════════════
 *  Mouth: W-shaped black curve under snout
 * ══════════════════════════════════════════════════════════════ */

static void draw_mouth(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const mouth_params_t *mp = &st->mouth;
    const uint16_t *pal = sp->pal;

    float half_thick = 2.0f + mp->openness * 1.5f;
    float dip = MOUTH_DIP + mp->openness * 3.0f;

    float x0 = (float)CENTER_X - MOUTH_HW;
    float y0 = MOUTH_Y;
    float xm = (float)CENTER_X;
    float ym = MOUTH_Y + dip;
    float x2 = (float)CENTER_X + MOUTH_HW;
    float y2 = MOUTH_Y;

    draw_quad_bezier_scan(y, x0, y0, xm, ym, x2, y2,
                          half_thick, pal[PAL_MOUTH], buf, SCREEN_W);
}

/* ════════════════════════════════════════════════════════════════
 *  Brows: thin black arcs above eyes
 * ══════════════════════════════════════════════════════════════ */

static void draw_brow_pig(int y, const brow_params_t *bp, int eye_cx, int eye_cy,
                           const uint16_t *pal, uint16_t *buf) {
    float thick = bp->thickness;
    if (thick < 0.3f) return;

    float half_thick = thick * 1.5f;
    float brow_y = (float)eye_cy - EYE_RY - 6.0f;

    float inner_x = (float)eye_cx - 10.0f;
    float inner_y = brow_y + bp->arch.dy * 4.0f;
    float arch_x  = (float)eye_cx;
    float arch_y  = inner_y - 3.0f * thick;
    float tail_x  = (float)eye_cx + 10.0f;
    float tail_y  = brow_y + bp->tail.dy * 3.0f;

    draw_cubic_bezier_scan(y,
                           inner_x, inner_y,
                           arch_x - 2.0f, arch_y,
                           arch_x + 2.0f, arch_y,
                           tail_x, tail_y,
                           half_thick, pal[PAL_BROW], buf, SCREEN_W);
}

static void draw_brow_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)EYE_HALF_SPACE + (int)(st->eye[0].position.dx * 15.0f);
    int eye_cy = (int)EYE_CY + (int)(st->eye[0].position.dy * 15.0f);
    draw_brow_pig(y, &st->brow[0], eye_cx, eye_cy, sp->pal, buf);
}

static void draw_brow_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)EYE_HALF_SPACE + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = (int)EYE_CY + (int)(st->eye[1].position.dy * 15.0f);
    draw_brow_pig(y, &st->brow[1], eye_cx, eye_cy, sp->pal, buf);
}

/* ════════════════════════════════════════════════════════════════
 *  Blush: black filled dots under eyes
 * ══════════════════════════════════════════════════════════════ */

static void draw_blush(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    float level = st->decor.blush;
    if (level < 0.05f) return;
    const uint16_t *pal = sp->pal;

    float blush_r = 7.0f;
    for (int side = 0; side < 2; side++) {
        int bx = CENTER_X + (side == 0 ? -44 : 44);
        fill_circle_scan(y, bx, (int)BLUSH_CY, blush_r, pal[PAL_BLUSH], buf, SCREEN_W);
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Decor overlay: not used for pig
 * ══════════════════════════════════════════════════════════════ */

static void draw_decor_overlay(int y, const face_state_t *st,
                                const sprite_set_t *sp, uint16_t *buf) {
    (void)y; (void)st; (void)sp; (void)buf;
}

/* ── draw_props: threshold opacity, no blending ─────────────── */

static void draw_props(int y, const face_state_t *st,
                       const sprite_set_t *sp, uint16_t *buf) {
    for (int i = 0; i < st->decor.prop_count; i++) {
        const prop_instance_t *p = &st->decor.props[i];
        if (p->opacity <= 0.5f) continue;

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

/* ════════════════════════════════════════════════════════════════
 *  Sprite definition
 * ══════════════════════════════════════════════════════════════ */

const sprite_set_t SPRITE_PIG = {
    .name = "pig",
    .eye_radius = EYE_RY,
    .eye_half_spacing = EYE_HALF_SPACE,
    .mouth_y_center = MOUTH_Y - (float)CENTER_Y,
    .brow_y_offset = -(EYE_RY + 6.0f),
    .blush_y_offset = BLUSH_CY - (float)CENTER_Y,
    .draw_face = draw_face,
    .draw_blush = draw_blush,
    .draw_mouth = draw_mouth,
    .draw_eye_left = draw_eye_left,
    .draw_eye_right = draw_eye_right,
    .draw_brow_left = draw_brow_left,
    .draw_brow_right = draw_brow_right,
    .draw_decor_overlay = draw_decor_overlay,
    .draw_props = draw_props,
    .pal = PALETTE_PIG,
};
