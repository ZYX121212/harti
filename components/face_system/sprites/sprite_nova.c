#include "sprite_nova.h"
#include "face_palette.h"
#include "face_common.h"
#include <math.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

/* ── NOVA head shape constants ────────────────────────────── */

#define HEAD_CX CENTER_X
#define HEAD_CY (CENTER_Y - 30)
#define HEAD_RX 55.0f
#define HEAD_RY 68.0f

/* ── draw_face: filled white head on black background ─────── */

static void draw_face(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    (void)st;
    const uint16_t *pal = sp->pal;

    // Fill entire line black first
    for (int x = 0; x < SCREEN_W; x++) buf[x] = pal[PAL_BG];

    // Draw filled white head ellipse
    float ny = (float)(y - HEAD_CY) / HEAD_RY;
    if (fabsf(ny) > 1.0f) return;

    float xspan = HEAD_RX * sqrtf(1.0f - ny * ny);
    int x0 = (int)(HEAD_CX - xspan);
    int x1 = (int)(HEAD_CX + xspan);
    if (x0 < 0) x0 = 0;
    if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;

    for (int x = x0; x <= x1; x++) {
        buf[x] = pal[PAL_SKIN];  // white filled head
    }
}

/* ── eye: filled white circle + black pupil + catchlight ──── */

static void draw_eye_nova(int y, const eye_params_t *ep, int eye_cx, int eye_cy,
                          const uint16_t *pal, uint16_t *buf) {
    float eye_r = 22.0f;
    float fy = y - eye_cy;
    float pad = 2.5f;
    if (fy < -eye_r - pad || fy > eye_r + pad) return;

    int x_start = eye_cx - (int)eye_r - (int)pad;
    int x_end   = eye_cx + (int)eye_r + (int)pad;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    float eye_r_sq = eye_r * eye_r;
    float shine = ep->shine_intensity;

    // Pupil offset
    float pdx = ep->iris_center.dx * 5.0f;
    float pdy = ep->iris_center.dy * 5.0f;
    // Pupil: black circle with catchlights
    float pupil_r = eye_r * 0.42f;
    float pupil_r_sq = pupil_r * pupil_r;

    // Catchlights (white)
    float cl_r = 2.8f * sqrtf(shine);
    float clx = pdx - pupil_r * 0.3f;
    float cly = pdy - pupil_r * 0.3f;
    float cl_r_sq = cl_r * cl_r;

    for (int x = x_start; x <= x_end; x++) {
        float fx = x - eye_cx;
        float r_sq = fx * fx + fy * fy;
        if (r_sq >= eye_r_sq) continue;

        // Filled white eye
        buf[x] = pal[PAL_SCLERA];

        // Black pupil
        if (dist_sq(fx, fy, pdx, pdy) < pupil_r_sq) {
            // White catchlight on black pupil
            if (shine > 0.1f && dist_sq(fx, fy, clx, cly) < cl_r_sq) {
                buf[x] = pal[PAL_SCLERA];  // white catchlight
            } else {
                buf[x] = pal[PAL_PUPIL];   // black pupil
            }
        }
    }
}

static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    int eye_cy = HEAD_CY - 2 + (int)(st->eye[0].position.dy * 15.0f);
    draw_eye_nova(y, &st->eye[0], eye_cx, eye_cy, sp->pal, buf);
}

static void draw_eye_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = HEAD_CY - 2 + (int)(st->eye[1].position.dy * 15.0f);
    draw_eye_nova(y, &st->eye[1], eye_cx, eye_cy, sp->pal, buf);
}

/* ── mouth: bezier smile, expression-driven ───────────────── */

static void draw_mouth(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const mouth_params_t *mp = &st->mouth;
    const uint16_t *pal = sp->pal;

    float my = HEAD_CY + 40.0f;
    float hw = 14.0f;
    float thick = 2.5f + mp->openness * 3.0f;

    /* Quadratic bezier smile: corners → center dip */
    float x0 = (float)CENTER_X - hw;
    float y0 = my - mp->openness * 3.0f;
    float x1 = (float)CENTER_X;
    float y1 = my + 5.0f + mp->openness * 8.0f;
    float x2 = (float)CENTER_X + hw;
    float y2 = y0;

    draw_quad_bezier_scan(y, x0, y0, x1, y1, x2, y2,
                          thick, pal[PAL_MOUTH], buf, SCREEN_W);
}

/* ── brows: thick white arcs ──────────────────────────────── */

static void draw_brow_nova(int y, const brow_params_t *bp, int eye_cx, int eye_cy,
                           const uint16_t *pal, uint16_t *buf) {
    float brow_y = eye_cy - 26.0f;
    float half_thick = bp->thickness * 2.0f;
    if (half_thick < 2.5f) half_thick = 2.5f;

    float inner_x = eye_cx - 9.0f;
    float inner_y = brow_y;
    float arch_y  = brow_y - 4.0f;
    float tail_x  = eye_cx + 9.0f;
    float tail_y  = brow_y;

    int x_start = (int)inner_x - 3;
    int x_end   = (int)tail_x + 3;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    for (int x = x_start; x <= x_end; x++) {
        float t = (float)(x - inner_x) / (tail_x - inner_x + 0.001f);
        if (t < 0.0f || t > 1.0f) continue;
        float curve_y = (1-t)*(1-t)*inner_y + 2*(1-t)*t*arch_y + t*t*tail_y;
        if (fabsf(y - curve_y) < half_thick) {
            buf[x] = pal[PAL_BROW];
        }
    }
}

static void draw_brow_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    int eye_cy = HEAD_CY - 2 + (int)(st->eye[0].position.dy * 15.0f);
    draw_brow_nova(y, &st->brow[0], eye_cx, eye_cy, sp->pal, buf);
}

static void draw_brow_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int eye_cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int eye_cy = HEAD_CY - 2 + (int)(st->eye[1].position.dy * 15.0f);
    draw_brow_nova(y, &st->brow[1], eye_cx, eye_cy, sp->pal, buf);
}

/* ── blush: ring ellipses under eyes ──────────────────────── */

static void draw_blush(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    float level = st->decor.blush;
    if (level <= 0.01f) return;
    const uint16_t *pal = sp->pal;
    int bcy = HEAD_CY + 25;

    for (int side = 0; side < 2; side++) {
        int bx = (side == 0) ? CENTER_X - 38 : CENTER_X + 38;
        for (int i = 0; i < 3; i++) {
            int cx = bx + (i - 1) * 6;
            int cy = bcy + i * 2;
            float rx = 4.0f, ry = 2.5f * level;
            if (y < cy - (int)ry - 1 || y > cy + (int)ry + 1) continue;
            int xs = cx - (int)rx - 1, xe = cx + (int)rx + 1;
            if (xs < 0) xs = 0;
            if (xe >= SCREEN_W) xe = SCREEN_W - 1;
            for (int x = xs; x <= xe; x++) {
                float ex = (float)(x - cx) / rx;
                float ey = (float)(y - cy) / ry;
                float e = ex * ex + ey * ey;
                if (e < 1.15f && e > 0.45f) buf[x] = pal[PAL_BLUSH];
            }
        }
    }
}

/* ── decor_overlay: animated tea-drinking (pick up → sip → put down → vanish) ── */

#define CUP_REST_X    50.0f
#define CUP_REST_Y   152.0f
#define CUP_MOUTH_X  105.0f
#define CUP_MOUTH_Y  127.0f
#define CUP_RIM_HW    15.0f
#define CUP_BASE_HW   10.0f
#define CUP_BASE_H    22.0f
#define CUP_BOW        3.0f

static void draw_decor_overlay(int y, const face_state_t *st,
                                const sprite_set_t *sp, uint16_t *buf) {
    (void)st;
    const uint16_t *pal = sp->pal;

    /* ── Frame counter ────────────────────────────────────── */
    static int tick = 0;
    int frame = tick / SCREEN_H;
    tick++;
    if (tick > SCREEN_H * 60000) tick = 0;

    /* ── Animation timeline (200 frames ≈ 3.3s @ 60fps) ──── */
    const int cycle = 200;
    float p = (float)(frame % cycle) / (float)cycle;  /* 0..1 */

    float move_t, tilt;
    bool visible;

    if (p < 0.18f) {
        /* idle on left, slight bob */
        visible = true;  move_t = 0.0f;  tilt = 0.0f;
    } else if (p < 0.35f) {
        /* rise to mouth */
        visible = true;
        float t = (p - 0.18f) / 0.17f;
        move_t = t * t * (3.0f - 2.0f * t);  /* smoothstep */
        tilt   = move_t;
    } else if (p < 0.55f) {
        /* hold at mouth, drinking */
        visible = true;  move_t = 1.0f;  tilt = 1.0f;
    } else if (p < 0.72f) {
        /* lower back to rest */
        visible = true;
        float t = (p - 0.55f) / 0.17f;
        move_t = 1.0f - t * t * (3.0f - 2.0f * t);
        tilt   = move_t;
    } else {
        /* cup vanishes */
        visible = false;
    }

    if (!visible) return;

    /* ── Interpolate cup position ──────────────────────────── */
    float cup_cx    = CUP_REST_X + (CUP_MOUTH_X - CUP_REST_X) * move_t;
    float cup_rim_y = CUP_REST_Y + (CUP_MOUTH_Y - CUP_REST_Y) * move_t;

    /* subtle idle bob when resting */
    if (move_t < 0.08f) {
        cup_rim_y += sinf((float)frame * 0.1f) * 1.8f * (1.0f - move_t / 0.08f);
    }

    /* drinking micro-bob at mouth */
    if (move_t > 0.9f) {
        cup_rim_y += sinf((float)frame * 0.2f) * 1.2f * ((move_t - 0.9f) / 0.1f);
    }

    /* ── Tilt: rim leans right, base shifts left ───────────── */
    float tilt_px   = tilt * 5.0f;
    float rim_cx    = cup_cx + tilt_px;
    float base_cx   = cup_cx - tilt_px;
    float cup_base_y = cup_rim_y + CUP_BASE_H;

    /* ── Cup body: white filled, black outlined ────────────── */
    if (y >= (int)cup_rim_y && y <= (int)cup_base_y) {
        float t = ((float)y - cup_rim_y) / (cup_base_y - cup_rim_y);
        float local_cx = rim_cx + (base_cx - rim_cx) * t;
        float hw = CUP_RIM_HW + (CUP_BASE_HW - CUP_RIM_HW) * t;
        hw += sinf(t * 3.14159f) * CUP_BOW;

        int x0 = (int)(local_cx - hw);
        int x1 = (int)(local_cx + hw);
        if (x0 < 0) x0 = 0;
        if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;

        for (int x = x0; x <= x1; x++) buf[x] = pal[PAL_SKIN];

        int l_edge = (int)(local_cx - hw);
        int r_edge = (int)(local_cx + hw);
        for (int dx = -1; dx <= 1; dx++) {
            int el = l_edge + dx;
            int er = r_edge + dx;
            if (el >= 0 && el < SCREEN_W) buf[el] = pal[PAL_PUPIL];
            if (er >= 0 && er < SCREEN_W) buf[er] = pal[PAL_PUPIL];
        }
    }

    /* ── Cup rim: black ellipse ────────────────────────────── */
    if (y >= (int)cup_rim_y - 2 && y <= (int)cup_rim_y + 2) {
        const float rim_ry = 3.0f;
        float ny = ((float)y - cup_rim_y) / rim_ry;
        if (fabsf(ny) < 1.0f) {
            float xspan = CUP_RIM_HW * sqrtf(1.0f - ny * ny);
            int x0 = (int)(rim_cx - xspan);
            int x1 = (int)(rim_cx + xspan);
            if (x0 < 0) x0 = 0;
            if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
            for (int x = x0; x <= x1; x++) buf[x] = pal[PAL_PUPIL];
        }
    }

    /* ── Cup base: black line ──────────────────────────────── */
    if (y >= (int)cup_base_y - 1 && y <= (int)cup_base_y + 1) {
        int x0 = (int)(base_cx - CUP_BASE_HW - 1);
        int x1 = (int)(base_cx + CUP_BASE_HW + 1);
        if (x0 < 0) x0 = 0;
        if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
        for (int x = x0; x <= x1; x++) buf[x] = pal[PAL_PUPIL];
    }

    /* ── Steam: white on black bg, black on white head ─────── */
    uint16_t steam_color = (move_t > 0.5f) ? pal[PAL_PUPIL] : pal[PAL_SCLERA];
    for (int w = 0; w < 3; w++) {
        float base_x = cup_cx - 4.0f + (float)w * 4.0f;
        float steam_top = cup_rim_y - 28.0f - (float)w * 6.0f;
        float amp = 2.0f + (float)w * 1.2f;
        float freq = 0.04f + (float)w * 0.01f;
        float steam_phase = (float)w * 1.5f + (float)frame * 0.04f;

        if (y >= (int)steam_top && y <= (int)cup_rim_y) {
            float dist = cup_rim_y - (float)y;
            float offset = wave_offset(dist, amp, freq, steam_phase);
            int sx = (int)(base_x + offset);
            if (sx >= 0 && sx < SCREEN_W) buf[sx] = steam_color;
            if (sx + 1 < SCREEN_W) buf[sx + 1] = steam_color;
        }
    }

    /* ── Hand: small oval, fades out when cup at mouth ─────── */
    if (move_t < 0.6f) {
        float hand_vis = 1.0f - (move_t / 0.6f);
        float hand_cx = cup_cx + CUP_RIM_HW + 5.0f;
        float hand_cy = cup_rim_y + 10.0f;
        float hand_rx = 6.0f;
        float hand_ry = 4.5f * hand_vis;
        float hny = ((float)y - hand_cy) / hand_ry;
        if (fabsf(hny) < 1.0f) {
            float xspan = hand_rx * sqrtf(1.0f - hny * hny);
            int x0 = (int)(hand_cx - xspan);
            int x1 = (int)(hand_cx + xspan);
            if (x0 < 0) x0 = 0;
            if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
            for (int x = x0; x <= x1; x++) buf[x] = pal[PAL_SKIN];
            if (x0 > 0) buf[x0 - 1] = pal[PAL_PUPIL];
            if (x1 < SCREEN_W - 1) buf[x1 + 1] = pal[PAL_PUPIL];
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

const sprite_set_t SPRITE_NOVA = {
    .name = "nova",
    .eye_radius = 22.0f,
    .eye_half_spacing = 30.0f,
    .mouth_y_center = 40.0f,
    .brow_y_offset = -26.0f,
    .blush_y_offset = 25.0f,
    .draw_face = draw_face,
    .draw_blush = draw_blush,
    .draw_mouth = draw_mouth,
    .draw_eye_left = draw_eye_left,
    .draw_eye_right = draw_eye_right,
    .draw_brow_left = draw_brow_left,
    .draw_brow_right = draw_brow_right,
    .draw_decor_overlay = draw_decor_overlay,
    .draw_props = draw_props,
    .pal = PALETTE_NOVA,
};
