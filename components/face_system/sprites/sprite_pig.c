#include "sprite_pig.h"
#include "face_palette.h"
#include "face_common.h"
#include <math.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

/* ════════════════════════════════════════════════════════════════
 *  Pig face geometry
 * ══════════════════════════════════════════════════════════════ */

#define HEAD_CX  120.0f
#define HEAD_CY  120.0f
#define HEAD_R    88.0f

/* Snout: oval below eyes */
#define SNOUT_CX  120.0f
#define SNOUT_CY  150.0f
#define SNOUT_RX   32.0f
#define SNOUT_RY   20.0f
#define NOSTRIL_R   4.0f
#define NOSTRIL_DX 10.0f
#define NOSTRIL_DY  3.0f

/* Eyes */
#define EYE_CY          105.0f
#define EYE_HALF_SPACE  26.0f
#define EYE_R            13.0f

/* Blush */
#define BLUSH_CY  132.0f

/* Mouth */
#define MOUTH_Y    178.0f
#define MOUTH_HW    14.0f
#define MOUTH_DIP    5.0f

/* ════════════════════════════════════════════════════════════════
 *  Ear bezier data
 * ══════════════════════════════════════════════════════════════ */

/* Left ear: tip→outer_ctrl→outer_base (left edge), tip→inner_ctrl→inner_base (right edge) */
#define L_TIP_X  40.0f
#define L_TIP_Y  32.0f
#define L_OUT_X  24.0f
#define L_OUT_Y  68.0f
#define L_IN_X   68.0f
#define L_IN_Y   48.0f
#define L_CTRL_OUT_X 18.0f
#define L_CTRL_OUT_Y 48.0f
#define L_CTRL_IN_X  54.0f
#define L_CTRL_IN_Y  36.0f

/* Right ear (mirrored around CENTER_X=120) */
#define R_TIP_X  200.0f
#define R_TIP_Y   32.0f
#define R_OUT_X  216.0f
#define R_OUT_Y   68.0f
#define R_IN_X   172.0f
#define R_IN_Y    48.0f
#define R_CTRL_OUT_X 222.0f
#define R_CTRL_OUT_Y  48.0f
#define R_CTRL_IN_X  186.0f
#define R_CTRL_IN_Y   36.0f

/* ════════════════════════════════════════════════════════════════
 *  Helper: solve quadratic-bezier for x at a given y
 *  Returns -1 if no solution in t ∈ [0,1].
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
 *  draw_face: pink background + filled head + ears + snout
 * ══════════════════════════════════════════════════════════════ */

static void draw_face(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    (void)st;
    const uint16_t *pal = sp->pal;

    /* 1. Fill entire row with background pink */
    for (int x = 0; x < SCREEN_W; x++) buf[x] = pal[PAL_BG];

    /* 2. Filled head circle + outline ring */
    fill_circle_scan(y, (int)HEAD_CX, (int)HEAD_CY, HEAD_R, pal[PAL_SKIN], buf, SCREEN_W);
    draw_ring_scan(y, (int)HEAD_CX, (int)HEAD_CY,
                   HEAD_R - 2.5f, HEAD_R + 0.5f,
                   pal[PAL_BG_EDGE], buf, SCREEN_W);

    /* 3. Ears — bezier-bounded filled shapes */
    /* Left ear */
    if (y >= (int)L_TIP_Y - 2 && y <= (int)L_OUT_Y + 2) {
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
            int xs = (int)lx;
            int xe = (int)rx;
            if (xs < 0) xs = 0;
            if (xe >= SCREEN_W) xe = SCREEN_W - 1;
            /* Ear fill */
            for (int x = xs; x <= xe; x++) buf[x] = pal[PAL_SKIN];
            /* Outline at bezier edges */
            int hl = 2;
            for (int dx = -hl; dx <= hl; dx++) {
                int olx = (int)lx + dx;
                int orx = (int)rx + dx;
                if (olx >= 0 && olx < SCREEN_W) buf[olx] = pal[PAL_BG_EDGE];
                if (orx >= 0 && orx < SCREEN_W) buf[orx] = pal[PAL_BG_EDGE];
            }
        }
    }

    /* Right ear (mirrored) */
    if (y >= (int)R_TIP_Y - 2 && y <= (int)R_OUT_Y + 2) {
        float lx = bez_x_at_y(R_TIP_X, R_TIP_Y, R_CTRL_IN_X, R_CTRL_IN_Y,
                              R_IN_X, R_IN_Y, (float)y);
        float rx = bez_x_at_y(R_TIP_X, R_TIP_Y, R_CTRL_OUT_X, R_CTRL_OUT_Y,
                              R_OUT_X, R_OUT_Y, (float)y);
        /* For right ear: inner edge is left-x, outer edge is right-x */
        if (y <= (int)R_IN_Y) {
            /* both bezier-defined */
        } else {
            float t = (float)(y - R_IN_Y) / (R_OUT_Y - R_IN_Y);
            lx = R_IN_X + t * (R_OUT_X - R_IN_X);
        }
        if (lx >= 0.0f && rx >= 0.0f && lx < rx) {
            int xs = (int)lx;
            int xe = (int)rx;
            if (xs < 0) xs = 0;
            if (xe >= SCREEN_W) xe = SCREEN_W - 1;
            for (int x = xs; x <= xe; x++) buf[x] = pal[PAL_SKIN];
            int hl = 2;
            for (int dx = -hl; dx <= hl; dx++) {
                int olx = (int)lx + dx;
                int orx = (int)rx + dx;
                if (olx >= 0 && olx < SCREEN_W) buf[olx] = pal[PAL_BG_EDGE];
                if (orx >= 0 && orx < SCREEN_W) buf[orx] = pal[PAL_BG_EDGE];
            }
        }
    }

    /* 4. Snout: filled ellipse with outline */
    float sny = (float)(y - (int)SNOUT_CY) / SNOUT_RY;
    if (fabsf(sny) <= 1.0f) {
        float sxspan = SNOUT_RX * sqrtf(1.0f - sny * sny);
        int sx0 = (int)(SNOUT_CX - sxspan);
        int sx1 = (int)(SNOUT_CX + sxspan);
        if (sx0 < 0) sx0 = 0;
        if (sx1 >= SCREEN_W) sx1 = SCREEN_W - 1;
        /* Snout fill (pale pink) */
        for (int x = sx0; x <= sx1; x++) buf[x] = pal[PAL_TONGUE];
        /* Snout outline ring (2px) */
        float edge_inner = 0.92f;
        if (fabsf(sny) > edge_inner) {
            /* near top/bottom of ellipse */
        }
        /* Thin outline: check distance from ellipse edge */
        for (int x = sx0; x <= sx1; x++) {
            float nx = (float)(x - SNOUT_CX) / SNOUT_RX;
            float e = nx * nx + sny * sny;
            if (e > 0.88f && e < 1.05f) {
                buf[x] = pal[PAL_BG_EDGE];
            }
        }
    }

    /* 5. Nostrils: two small filled circles on the snout */
    fill_circle_scan(y, (int)(SNOUT_CX - NOSTRIL_DX), (int)(SNOUT_CY + NOSTRIL_DY),
                     NOSTRIL_R, pal[PAL_MOUTH], buf, SCREEN_W);
    fill_circle_scan(y, (int)(SNOUT_CX + NOSTRIL_DX), (int)(SNOUT_CY + NOSTRIL_DY),
                     NOSTRIL_R, pal[PAL_MOUTH], buf, SCREEN_W);
}

/* ════════════════════════════════════════════════════════════════
 *  Eyes: white filled circle + black pupil + catchlight
 * ══════════════════════════════════════════════════════════════ */

static void draw_eye_pig(int y, const eye_params_t *ep, int eye_cx, int eye_cy,
                          const uint16_t *pal, uint16_t *buf) {
    float fy = (float)(y - eye_cy);
    if (fabsf(fy) > EYE_R + 2.0f) return;

    float eye_r_sq = EYE_R * EYE_R;
    float pad = 2.5f;
    int x_start = eye_cx - (int)EYE_R - (int)pad;
    int x_end   = eye_cx + (int)EYE_R + (int)pad;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    float pdx = ep->iris_center.dx * 4.0f;
    float pdy = ep->iris_center.dy * 4.0f;
    float pupil_r = EYE_R * 0.45f;
    float pupil_r_sq = pupil_r * pupil_r;
    float shine = ep->shine_intensity;

    float cl_r = 2.5f * sqrtf(shine);
    float clx = pdx - pupil_r * 0.3f;
    float cly = pdy - pupil_r * 0.3f;
    float cl_r_sq = cl_r * cl_r;

    for (int x = x_start; x <= x_end; x++) {
        float fx = (float)(x - eye_cx);
        float r_sq = fx * fx + fy * fy;
        if (r_sq > eye_r_sq) continue;

        /* White eye fill */
        buf[x] = pal[PAL_SCLERA];

        /* Eye outline ring */
        float r = sqrtf(r_sq);
        if (EYE_R - r < 2.0f) {
            buf[x] = pal[PAL_BG_EDGE];
            continue;
        }

        /* Black pupil */
        if (dist_sq(fx, fy, pdx, pdy) < pupil_r_sq) {
            if (shine > 0.1f && dist_sq(fx, fy, clx, cly) < cl_r_sq) {
                buf[x] = pal[PAL_SHINE];   /* white catchlight */
            } else {
                buf[x] = pal[PAL_PUPIL];   /* black pupil */
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
 *  Mouth: quadratic-bezier smile curve under snout
 * ══════════════════════════════════════════════════════════════ */

static void draw_mouth(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const mouth_params_t *mp = &st->mouth;
    const uint16_t *pal = sp->pal;

    float half_thick = 2.0f + mp->openness * 2.0f;
    float dip = MOUTH_DIP + mp->openness * 4.0f;

    /* Quadratic bezier: left corner → center dip → right corner */
    float x0 = (float)CENTER_X - MOUTH_HW;
    float y0 = MOUTH_Y;
    float x1 = (float)CENTER_X;
    float y1 = MOUTH_Y + dip;
    float x2 = (float)CENTER_X + MOUTH_HW;
    float y2 = MOUTH_Y;

    draw_quad_bezier_scan(y, x0, y0, x1, y1, x2, y2,
                          half_thick, pal[PAL_MOUTH], buf, SCREEN_W);
}

/* ════════════════════════════════════════════════════════════════
 *  Brows: subtle bezier arcs above eyes (expression-driven)
 * ══════════════════════════════════════════════════════════════ */

static void draw_brow_pig(int y, const brow_params_t *bp, int eye_cx, int eye_cy,
                           const uint16_t *pal, uint16_t *buf) {
    float thick = bp->thickness;
    if (thick < 0.3f) return;  /* only show when expression pushes brows */

    float half_thick = thick * 1.8f;
    float brow_y = (float)eye_cy - EYE_R - 6.0f;

    float inner_x = (float)eye_cx - 8.0f;
    float inner_y = brow_y + bp->arch.dy * 4.0f;
    float arch_x  = (float)eye_cx;
    float arch_y  = inner_y - 5.0f * thick;
    float tail_x  = (float)eye_cx + 8.0f;
    float tail_y  = brow_y + bp->tail.dy * 3.0f;

    /* Use cubic bezier scan for smooth brow */
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
 *  Blush: rosy filled circles under/outside eyes
 * ══════════════════════════════════════════════════════════════ */

static void draw_blush(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    float level = st->decor.blush;
    if (level < 0.05f) return;
    const uint16_t *pal = sp->pal;

    float blush_r = 13.0f * level;
    for (int side = 0; side < 2; side++) {
        int bx = (side == 0) ? CENTER_X - 52 : CENTER_X + 52;
        int by = (int)BLUSH_CY;
        fill_circle_scan(y, bx, by, blush_r, pal[PAL_BLUSH], buf, SCREEN_W);
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Decor overlay: none (pig design is complete without extras)
 * ══════════════════════════════════════════════════════════════ */

static void draw_decor_overlay(int y, const face_state_t *st,
                                const sprite_set_t *sp, uint16_t *buf) {
    (void)y; (void)st; (void)sp; (void)buf;
}

/* ════════════════════════════════════════════════════════════════
 *  Sprite definition
 * ══════════════════════════════════════════════════════════════ */

const sprite_set_t SPRITE_PIG = {
    .name = "pig",
    .eye_radius = EYE_R,
    .eye_half_spacing = EYE_HALF_SPACE,
    .mouth_y_center = MOUTH_Y - (float)CENTER_Y,
    .brow_y_offset = -(EYE_R + 6.0f),
    .blush_y_offset = BLUSH_CY - (float)CENTER_Y,
    .draw_face = draw_face,
    .draw_blush = draw_blush,
    .draw_mouth = draw_mouth,
    .draw_eye_left = draw_eye_left,
    .draw_eye_right = draw_eye_right,
    .draw_brow_left = draw_brow_left,
    .draw_brow_right = draw_brow_right,
    .draw_decor_overlay = draw_decor_overlay,
    .pal = PALETTE_PIG,
};
