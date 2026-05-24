#include "sprite_robot.h"
#include "face_palette.h"
#include "face_common.h"
#include <math.h>

#define SCREEN_W 240
#define CENTER_X 120
#define CENTER_Y 120

/* ═══════════════════════════════════════════════════════════
   Head: rounded-square outline + antenna + ear bolts
   ═══════════════════════════════════════════════════════════ */

static void draw_face(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    (void)st;
    const uint16_t *pal = sp->pal;

    /* Solid black background — nothing to draw, display is already black */

    const int hw = 64, hh = 66, rr = 28;

    /* Compute rounded-rect x-bounds for this scanline */
    float fy = (float)y;
    int x_left = 0, x_right = SCREEN_W - 1;
    int in_rect = 0;

    if (fy >= (float)(CENTER_Y - hh) && fy <= (float)(CENTER_Y + hh)) {
        float hw_f = (float)hw, rr_f = (float)rr;
        float corner_dy = 0.0f;
        float corner_x = 0.0f;

        if (fy < (float)(CENTER_Y - hh + rr)) {
            /* Top corners: circular arc */
            corner_dy = (float)(CENTER_Y - hh + rr) - fy;
            corner_x = sqrtf(rr_f * rr_f - corner_dy * corner_dy);
        } else if (fy > (float)(CENTER_Y + hh - rr)) {
            /* Bottom corners: circular arc */
            corner_dy = fy - (float)(CENTER_Y + hh - rr);
            corner_x = sqrtf(rr_f * rr_f - corner_dy * corner_dy);
        }

        x_left  = CENTER_X - (int)(hw_f - rr_f + corner_x);
        x_right = CENTER_X + (int)(hw_f - rr_f + corner_x);
        in_rect = 1;
    }

    /* Draw 2px white border on left and right edges */
    if (in_rect) {
        for (int dx = 0; dx < 2; dx++) {
            int xl = x_left - dx;
            int xr = x_right + dx;
            if (xl >= 0 && xl < SCREEN_W) buf[xl] = pal[PAL_BROW];
            if (xr >= 0 && xr < SCREEN_W) buf[xr] = pal[PAL_BROW];
        }
    }

    /* Top and bottom flat edges of rounded rect */
    if (fy >= (float)(CENTER_Y - hh + rr) && fy <= (float)(CENTER_Y + hh - rr)) {
        /* Top edge */
        if (fy >= (float)(CENTER_Y - hh) && fy < (float)(CENTER_Y - hh + 2)) {
            int xl = CENTER_X - hw, xr = CENTER_X + hw;
            for (int x = xl; x <= xr; x++) {
                if (x >= 0 && x < SCREEN_W) buf[x] = pal[PAL_BROW];
            }
        }
        /* Bottom edge */
        if (fy > (float)(CENTER_Y + hh - 2) && fy <= (float)(CENTER_Y + hh)) {
            int xl = CENTER_X - hw, xr = CENTER_X + hw;
            for (int x = xl; x <= xr; x++) {
                if (x >= 0 && x < SCREEN_W) buf[x] = pal[PAL_BROW];
            }
        }
    }

    /* Antenna: vertical line + ball on top */
    if (y >= CENTER_Y - hh - 16 && y <= CENTER_Y - hh + 2) {
        for (int x = CENTER_X - 1; x <= CENTER_X + 1; x++) {
            if (x >= 0 && x < SCREEN_W) buf[x] = pal[PAL_BROW];
        }
    }
    fill_circle_scan(y, CENTER_X, CENTER_Y - hh - 19, 5.0f,
                     pal[PAL_SCLERA], buf, SCREEN_W);

    /* Ear bolts: white outer circle, black inner dot */
    int ear_y = CENTER_Y - 12;
    fill_circle_scan(y, CENTER_X - hw - 5, ear_y, 6.0f,
                     pal[PAL_SCLERA], buf, SCREEN_W);
    fill_circle_scan(y, CENTER_X - hw - 5, ear_y, 2.5f,
                     pal[PAL_BG], buf, SCREEN_W);
    fill_circle_scan(y, CENTER_X + hw + 5, ear_y, 6.0f,
                     pal[PAL_SCLERA], buf, SCREEN_W);
    fill_circle_scan(y, CENTER_X + hw + 5, ear_y, 2.5f,
                     pal[PAL_BG], buf, SCREEN_W);
}

/* ═══════════════════════════════════════════════════════════
   Eyes: white circle + black pupil + eyelids + outline
   ═══════════════════════════════════════════════════════════ */

static void draw_eye_impl(int y, const eye_params_t *ep, int eye_cx,
                          float eye_r, const uint16_t *pal, uint16_t *buf) {
    if (eye_r < 3.0f) return;

    float lid = ep->top_lid_mid.dy; /* >0 = droop, <0 = wide open */
    float pdx = ep->iris_center.dx * eye_r * 0.5f;
    float pdy = ep->iris_center.dy * eye_r * 0.5f;
    float pupil_r = eye_r * 0.42f * ep->pupil_scale + 1.5f;
    if (pupil_r < 2.0f) pupil_r = 2.0f;

    /* White fill for eye socket */
    fill_circle_scan(y, eye_cx, (int)CENTER_Y, eye_r,
                     pal[PAL_SCLERA], buf, SCREEN_W);

    /* Black pupil (drawn AFTER white fill, so it's on top) */
    int pupil_cx = eye_cx + (int)pdx;
    int pupil_cy = (int)CENTER_Y + (int)pdy;
    fill_circle_scan(y, pupil_cx, pupil_cy, pupil_r,
                     pal[PAL_PUPIL], buf, SCREEN_W);

    /* Top eyelid: black overlay from top of eye downward */
    if (lid > 0.02f) {
        float lid_y = (float)(CENTER_Y - (int)eye_r) + lid * eye_r * 2.0f;
        if ((float)y < lid_y) {
            int bound = (int)(eye_r);
            for (int x = eye_cx - bound; x <= eye_cx + bound; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                float dx = (float)(x - eye_cx);
                float dy_f = (float)(y - CENTER_Y);
                if (dx * dx + dy_f * dy_f < eye_r * eye_r) {
                    buf[x] = pal[PAL_BG];
                }
            }
        }
    }

    /* White outline ring (drawn last, over pupil + eyelids) */
    draw_ring_scan(y, eye_cx, (int)CENTER_Y,
                   eye_r - 1.0f, eye_r, pal[PAL_SCLERA], buf, SCREEN_W);

    /* Shine dot: small white dot at top-right of eye */
    if (ep->shine_intensity > 0.3f) {
        int sh_x = eye_cx + (int)(eye_r * 0.42f);
        int sh_y = (int)CENTER_Y - (int)(eye_r * 0.48f);
        fill_circle_scan(y, sh_x, sh_y, 2.5f,
                         pal[PAL_SHINE], buf, SCREEN_W);
    }
}

static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    draw_eye_impl(y, &st->eye[0], cx, sp->eye_radius, sp->pal, buf);
}

static void draw_eye_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    draw_eye_impl(y, &st->eye[1], cx, sp->eye_radius, sp->pal, buf);
}

/* ═══════════════════════════════════════════════════════════
   Brows: angular V-shape white lines
   ═══════════════════════════════════════════════════════════ */

static void draw_brow_impl(int y, const brow_params_t *bp, int eye_cx,
                           float brow_y_offset, const uint16_t *pal, uint16_t *buf) {
    float brow_y_px = (float)CENTER_Y + brow_y_offset;
    float half_thick = bp->thickness * 2.0f;
    if (half_thick < 1.0f) half_thick = 1.0f;

    float inner_x = (float)eye_cx - 16.0f + bp->inner.dx * 22.0f;
    float arch_x  = (float)eye_cx + bp->arch.dx * 10.0f;
    float tail_x  = (float)eye_cx + 16.0f + bp->tail.dx * 28.0f;

    float inner_y = brow_y_px + bp->inner.dy * 26.0f;
    float arch_y  = brow_y_px + bp->arch.dy * 26.0f;
    float tail_y  = brow_y_px + bp->tail.dy * 26.0f;

    /* Segment 1: inner → arch */
    float seg1_dx = arch_x - inner_x;
    float seg1_dy = arch_y - inner_y;
    float seg1_len = sqrtf(seg1_dx * seg1_dx + seg1_dy * seg1_dy);
    if (seg1_len > 0.01f) {
        /* Check distance from point (x,y) to this segment */
        int x_start = (int)inner_x - 4;
        int x_end   = (int)arch_x + 4;
        if (x_start < 0) x_start = 0;
        if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;
        for (int x = x_start; x <= x_end; x++) {
            float t = ((float)(x - (int)inner_x) * seg1_dx + ((float)y - inner_y) * seg1_dy) / (seg1_len * seg1_len);
            if (t < 0.0f || t > 1.0f) continue;
            float proj_x = inner_x + t * seg1_dx;
            float proj_y = inner_y + t * seg1_dy;
            float dist = sqrtf(((float)x - proj_x) * ((float)x - proj_x) + ((float)y - proj_y) * ((float)y - proj_y));
            if (dist < half_thick) {
                buf[x] = pal[PAL_BROW];
            }
        }
    }

    /* Segment 2: arch → tail */
    float seg2_dx = tail_x - arch_x;
    float seg2_dy = tail_y - arch_y;
    float seg2_len = sqrtf(seg2_dx * seg2_dx + seg2_dy * seg2_dy);
    if (seg2_len > 0.01f) {
        int x_start = (int)arch_x - 4;
        int x_end   = (int)tail_x + 4;
        if (x_start < 0) x_start = 0;
        if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;
        for (int x = x_start; x <= x_end; x++) {
            float t = ((float)(x - (int)arch_x) * seg2_dx + ((float)y - arch_y) * seg2_dy) / (seg2_len * seg2_len);
            if (t < 0.0f || t > 1.0f) continue;
            float proj_x = arch_x + t * seg2_dx;
            float proj_y = arch_y + t * seg2_dy;
            float dist = sqrtf(((float)x - proj_x) * ((float)x - proj_x) + ((float)y - proj_y) * ((float)y - proj_y));
            if (dist < half_thick) {
                buf[x] = pal[PAL_BROW];
            }
        }
    }
}

static void draw_brow_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    draw_brow_impl(y, &st->brow[0], cx, sp->brow_y_offset, sp->pal, buf);
}

static void draw_brow_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    draw_brow_impl(y, &st->brow[1], cx, sp->brow_y_offset, sp->pal, buf);
}

/* ═══════════════════════════════════════════════════════════
   Mouth: 3-bar speaker grille
   ═══════════════════════════════════════════════════════════ */

static void draw_mouth(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const mouth_params_t *mp = &st->mouth;
    const uint16_t *pal = sp->pal;

    int mouth_cy = CENTER_Y + (int)sp->mouth_y_center;
    int n_bars = 3;
    float gap = 9.0f;
    float top_y = (float)mouth_cy - (float)(n_bars - 1) * gap * 0.5f;
    float hw = 26.0f;

    for (int i = 0; i < n_bars; i++) {
        float t = (float)i / (float)(n_bars - 1);
        float bar_y = top_y + (float)i * gap;
        /* Openness spreads bars apart */
        bar_y += (t - 0.5f) * mp->openness * 42.0f;
        /* Smile/frown arc */
        float cdy = (mp->left_corner.dy + mp->right_corner.dy) * 0.5f;
        bar_y += -cdy * 14.0f * (1.0f - (1.0f - fabsf(t - 0.5f) * 2.0f) * (1.0f - fabsf(t - 0.5f) * 2.0f));

        int bar_yi = (int)bar_y;
        if (y < bar_yi - 1 || y > bar_yi + 1) continue;

        /* Cupid dip on top bar */
        if (i == 0 && mp->cupid_depth > 0.1f) {
            int x_start = CENTER_X - (int)hw;
            int x_end   = CENTER_X + (int)hw;
            if (x_start < 0) x_start = 0;
            if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;
            float dip = mp->cupid_depth * 7.0f;
            for (int x = x_start; x <= x_end; x++) {
                float bt = (float)(x - x_start) / (float)(x_end - x_start);
                float v_y;
                if (bt < 0.5f) {
                    v_y = bar_y + dip * (bt * 2.0f);
                } else {
                    v_y = bar_y + dip * ((1.0f - bt) * 2.0f);
                }
                if (fabsf((float)y - v_y) < 1.5f) {
                    buf[x] = pal[PAL_MOUTH];
                }
            }
        } else {
            int x_start = CENTER_X - (int)hw;
            int x_end   = CENTER_X + (int)hw;
            if (x_start < 0) x_start = 0;
            if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;
            for (int x = x_start; x <= x_end; x++) {
                buf[x] = pal[PAL_MOUTH];
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   Blush: none for robot (no blend_colors, pure B&W)
   ═══════════════════════════════════════════════════════════ */

static void draw_blush(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    (void)y; (void)st; (void)sp; (void)buf;
    /* Robot has no blush — pure B&W design */
}

/* ═══════════════════════════════════════════════════════════
   Decor overlay: none (was panel lines + indicators — removed)
   ═══════════════════════════════════════════════════════════ */

static void draw_decor_overlay(int y, const face_state_t *st,
                                const sprite_set_t *sp, uint16_t *buf) {
    (void)y; (void)st; (void)sp; (void)buf;
    /* No overlay decor for robot */
}

/* ═══════════════════════════════════════════════════════════
   Props: threshold-based rendering (pure B&W, no blending)
   opacity > 0.3 → pure white; otherwise skip
   ═══════════════════════════════════════════════════════════ */

static void draw_props(int y, const face_state_t *st,
                       const sprite_set_t *sp, uint16_t *buf) {
    for (int i = 0; i < st->decor.prop_count; i++) {
        const prop_instance_t *p = &st->decor.props[i];
        if (p->opacity <= 0.3f) continue;

        float r = 100.0f * p->distance;
        float px = CENTER_X + r * cosf(p->angle);
        float py = CENTER_Y - r * sinf(p->angle);
        float sz = 10.0f + p->scale * 12.0f;

        /* Pure white — no blend */
        uint16_t color = sp->pal[PAL_SCLERA];

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

/* ═══════════════════════════════════════════════════════════
   Sprite definition
   ═══════════════════════════════════════════════════════════ */

const sprite_set_t SPRITE_ROBOT = {
    .name = "robot",
    .eye_radius = 20.0f,
    .eye_half_spacing = 30.0f,
    .mouth_y_center = 50.0f,
    .brow_y_offset = -44.0f,
    .blush_y_offset = 33.0f,
    .draw_face = draw_face,
    .draw_blush = draw_blush,
    .draw_mouth = draw_mouth,
    .draw_eye_left = draw_eye_left,
    .draw_eye_right = draw_eye_right,
    .draw_brow_left = draw_brow_left,
    .draw_brow_right = draw_brow_right,
    .draw_decor_overlay = draw_decor_overlay,
    .draw_props = draw_props,
    .pal = PALETTE_ROBOT,
};
