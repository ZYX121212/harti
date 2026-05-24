#include "sprite_cat.h"
#include "face_palette.h"
#include "face_common.h"
#include <math.h>
#include <stdlib.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

/* ═══════════════════════════════════════════════════════════
   Cat sprite — pure black & white line art on black bg
   ═══════════════════════════════════════════════════════════ */

/* ── Cat nose: small white triangle ──────────────────────── */
static void draw_nose(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    (void)st;
    const uint16_t *pal = sp->pal;
    float nx = CENTER_X, ny = CENTER_Y + 20.0f;
    fill_triangle_scan(y, nx, ny - 5.0f, nx - 5.0f, ny + 3.0f, nx + 5.0f, ny + 3.0f,
                       pal[PAL_SCLERA], buf, SCREEN_W);
}

/* ── Face: black background ──────────────────────────────── */
static void draw_face(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    (void)st;
    const uint16_t *pal = sp->pal;
    for (int x = 0; x < SCREEN_W; x++) buf[x] = pal[PAL_BG];
}

/* ── Cat eye: white circle + black slit pupil + blink lid ── */
static void draw_eye_impl(int y, const eye_params_t *ep, int eye_cx, int eye_cy,
                          const uint16_t *pal, uint16_t *buf) {
    const float eye_r = 24.0f;

    float fy = y - eye_cy;
    if (fy < -eye_r - 2 || fy > eye_r + 2) return;

    /* Blink lid */
    float lid_open = 1.0f - (ep->top_lid_mid.dy * 1.6f);
    if (lid_open < 0.02f) lid_open = 0.02f;
    if (lid_open > 1.0f) lid_open = 1.0f;
    float lid_y = eye_cy - eye_r + eye_r * 2.0f * (1.0f - lid_open);

    int x_start = eye_cx - (int)eye_r - 2;
    int x_end   = eye_cx + (int)eye_r + 2;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    /* Pupil position */
    float px = eye_cx + ep->iris_center.dx * 6.0f;
    float py = eye_cy + ep->iris_center.dy * 6.0f;

    /* Pupil dilates to round when eyes are wide open (real cat behavior):
       surprised/scared → round; relaxed/sleepy → slit */
    float lid_openness = -ep->top_lid_mid.dy;
    float dilate = (lid_openness > 0.05f) ? lid_openness * 10.0f : 0.0f;
    float slit_rx = 3.5f + ep->pupil_scale * 7.0f + dilate;
    float slit_ry = 10.0f + ep->pupil_scale * 1.0f - dilate * 0.5f;

    for (int x = x_start; x <= x_end; x++) {
        float fx = x - eye_cx;
        if (fx * fx + fy * fy >= eye_r * eye_r) continue;

        /* Lid occlusion */
        if (y < lid_y) { buf[x] = pal[PAL_BG]; continue; }

        /* Pupil ellipse test */
        float dx = (x - px) / slit_rx;
        float dy = (y - py) / slit_ry;
        if (dx * dx + dy * dy < 1.0f)
            buf[x] = pal[PAL_PUPIL];
        else
            buf[x] = pal[PAL_SCLERA];
    }
}

static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    int cy = CENTER_Y + (int)(st->eye[0].position.dy * 15.0f);
    draw_eye_impl(y, &st->eye[0], cx, cy, sp->pal, buf);
}
static void draw_eye_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int cy = CENTER_Y + (int)(st->eye[1].position.dy * 15.0f);
    draw_eye_impl(y, &st->eye[1], cx, cy, sp->pal, buf);
}

/* ── Cat brows: angular white strokes above eyes ──────────── */
static void draw_brow_impl(int y, const brow_params_t *bp, int eye_cx, int eye_cy,
                           const uint16_t *pal, float brow_y_offset, uint16_t *buf) {
    float by = eye_cy + brow_y_offset;
    float thick = 1.8f + bp->thickness * 1.2f;

    /* Inner corner (near nose, higher) → outer tail (lower, longer) */
    float ix = eye_cx + bp->inner.dx * 18.0f - 6.0f;
    float iy = by + bp->inner.dy * 10.0f - 4.0f;
    float ay = by + bp->arch.dy * 16.0f - 6.0f;
    float tx = eye_cx + bp->tail.dx * 26.0f + 6.0f;
    float ty = by + bp->tail.dy * 10.0f + 2.0f;

    int xs = (int)(ix < tx ? ix : tx) - 3;
    int xe = (int)(ix > tx ? ix : tx) + 3;
    if (xs < 0) xs = 0;
    if (xe >= SCREEN_W) xe = SCREEN_W - 1;

    for (int x = xs; x <= xe; x++) {
        float t = (x - ix) / (tx - ix + 0.001f);
        if (t < 0.0f || t > 1.0f) continue;
        /* Cubic bezier: inner → arch → arch → tail for crisp angle */
        float cy = cubic_bezier(iy, ay, ay, ty, t);
        if (fabsf(y - cy) < thick) buf[x] = pal[PAL_SCLERA];
    }
}

static void draw_brow_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X - (int)sp->eye_half_spacing + (int)(st->eye[0].position.dx * 15.0f);
    int cy = CENTER_Y + (int)(st->eye[0].position.dy * 15.0f);
    draw_brow_impl(y, &st->brow[0], cx, cy, sp->pal, sp->brow_y_offset, buf);
}
static void draw_brow_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X + (int)sp->eye_half_spacing + (int)(st->eye[1].position.dx * 15.0f);
    int cy = CENTER_Y + (int)(st->eye[1].position.dy * 15.0f);
    draw_brow_impl(y, &st->brow[1], cx, cy, sp->pal, sp->brow_y_offset, buf);
}

/* ── Cat mouth: white ω shape + nose ─────────────────────── */
static void draw_mouth(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const mouth_params_t *mp = &st->mouth;
    int mcy = CENTER_Y + (int)sp->mouth_y_center;
    const uint16_t *pal = sp->pal;
    float hw = 20.0f;

    /* Nose above mouth */
    draw_nose(y, st, sp, buf);

    int xs = CENTER_X - (int)hw - 3, xe = CENTER_X + (int)hw + 3;
    if (xs < 0) xs = 0;
    if (xe >= SCREEN_W) xe = SCREEN_W - 1;

    float lcx = CENTER_X + mp->left_corner.dx * hw;
    float rcx = CENTER_X + mp->right_corner.dx * hw;
    float rise = 6.0f * (1.0f + mp->cupid_depth * 0.5f);
    float thick = 1.8f;

    for (int x = xs; x <= xe; x++) {
        if (x < lcx - 2 || x > rcx + 2) continue;
        float t = (x - lcx) / (rcx - lcx + 0.001f);
        if (t < 0.0f || t > 1.0f) continue;

        float corner_y = mcy + mp->left_corner.dy * 6.0f * (1.0f - t)
                               + mp->right_corner.dy * 6.0f * t;
        float dip = mcy + 4.0f;
        float peak = mcy - rise;
        float upper_y;
        if (t < 0.5f) {
            float tt = t * 2.0f;
            upper_y = quad_bezier(corner_y, dip, peak, tt);
        } else {
            float tt = (t - 0.5f) * 2.0f;
            upper_y = quad_bezier(peak, dip, corner_y, tt);
        }

        if (y >= upper_y - thick && y <= upper_y + thick)
            buf[x] = pal[PAL_SCLERA];

        if (mp->openness > 0.03f) {
            float lower_y = mcy + mp->openness * 6.0f;
            if (y >= lower_y - 1.5f && y <= lower_y + 1.5f)
                buf[x] = pal[PAL_SCLERA];
        }
    }
}

/* ── Blush: no-op (cat doesn't need blush in B&W) ─────────── */
static void draw_blush(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    (void)y; (void)st; (void)sp; (void)buf;
}

/* ── Whiskers + decor overlay ─────────────────────────────── */
static void draw_decor_overlay(int y, const face_state_t *st,
                                const sprite_set_t *sp, uint16_t *buf) {
    const decor_params_t *dp = &st->decor;
    const uint16_t *pal = sp->pal;

    /* Whiskers: 3 per side, fanning from snout area */
    float whisker_origin_x[2] = { CENTER_X - 16.0f, CENTER_X + 16.0f };
    float whisker_origin_y = CENTER_Y + 28.0f;
    float whisker_angles[3] = { -0.15f, 0.0f, 0.18f };  /* up, level, down */
    float whisker_len = 38.0f;
    float whisker_thick = 1.0f;

    for (int side = 0; side < 2; side++) {
        float ox = whisker_origin_x[side];
        float oy = whisker_origin_y;
        int dir = side ? 1 : -1;

        for (int w = 0; w < 3; w++) {
            float angle = whisker_angles[w];
            /* Endpoint */
            float ex = ox + dir * whisker_len * cosf(angle);
            float ey = oy + whisker_len * sinf(angle);
            /* Control point y for slight droop */
            float cy = oy + whisker_len * sinf(angle) * 0.3f + 4.0f;

            /* Scanline: check if this row's y is near the whisker curve */
            int wxs = (int)(ox < ex ? ox : ex) - 1;
            int wxe = (int)(ox > ex ? ox : ex) + 1;
            if (wxs < 0) wxs = 0;
            if (wxe >= SCREEN_W) wxe = SCREEN_W - 1;

            for (int x = wxs; x <= wxe; x++) {
                float t = (x - ox) / (ex - ox + 0.001f);
                if (t < 0.0f || t > 1.0f) continue;
                float cy_bezier = quad_bezier(oy, cy, ey, t);
                if (fabsf(y - cy_bezier) < whisker_thick)
                    buf[x] = pal[PAL_SCLERA];
            }
        }
    }

    /* Stars */
    if (dp->stars > 0.01f) {
        static const int pos[3][2] = {
            {CENTER_X, CENTER_Y - 90},
            {CENTER_X - 50, CENTER_Y - 70},
            {CENTER_X + 50, CENTER_Y - 70}
        };
        for (int i = 0; i < 3; i++) {
            if (dist_sq(CENTER_X, y, pos[i][0], pos[i][1]) >= 20.0f) continue;
            int sx = pos[i][0];
            for (int x = sx - 5; x <= sx + 5; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                if (dist_sq(x, y, sx, pos[i][1]) < 16.0f)
                    buf[x] = pal[PAL_SCLERA];
            }
        }
    }

    /* Sparkle */
    if (dp->sparkle > 0.01f) {
        for (int i = 0; i < 4; i++) {
            int sx = CENTER_X + (i % 2 ? 42 : -42);
            int sy = CENTER_Y - 40 + (i / 2) * 22;
            if (y < sy - 3 || y > sy + 3) continue;
            for (int x = sx - 3; x <= sx + 3; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                if (dist_sq(x, y, sx, sy) < 5.0f)
                    buf[x] = pal[PAL_SCLERA];
            }
        }
    }
}

/* ── Props ────────────────────────────────────────────────── */
static void draw_props(int y, const face_state_t *st,
                       const sprite_set_t *sp, uint16_t *buf) {
    for (int i = 0; i < st->decor.prop_count; i++) {
        const prop_instance_t *p = &st->decor.props[i];
        if (p->opacity <= 0.01f) continue;

        float r = 100.0f * p->distance;
        float px = CENTER_X + r * cosf(p->angle);
        float py = CENTER_Y - r * sinf(p->angle);
        float sz = 10.0f + p->scale * 12.0f;
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

const sprite_set_t SPRITE_CAT = {
    .name = "cat",
    .eye_radius = 24.0f,
    .eye_half_spacing = 28.0f,
    .mouth_y_center = 52.0f,
    .brow_y_offset = -32.0f,
    .blush_y_offset = 32.0f,
    .draw_face = draw_face,
    .draw_blush = draw_blush,
    .draw_mouth = draw_mouth,
    .draw_eye_left = draw_eye_left,
    .draw_eye_right = draw_eye_right,
    .draw_brow_left = draw_brow_left,
    .draw_brow_right = draw_brow_right,
    .draw_decor_overlay = draw_decor_overlay,
    .draw_props = draw_props,
    .pal = PALETTE_CAT,
};
