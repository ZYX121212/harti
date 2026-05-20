#include "sprite_robot.h"
#include "face_palette.h"
#include <math.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120
#define BG_GRADIENT_MAX 171

static inline float dist_sq(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

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

/* ── Background: metallic gradient ────────────────────────── */
static uint16_t bg_lut[BG_GRADIENT_MAX];
static const uint16_t *active_pal = NULL;

static void build_bg_lut(const uint16_t *pal) {
    for (int d = 0; d < BG_GRADIENT_MAX; d++) {
        float t = d / 160.0f;
        if (t > 1.0f) t = 1.0f;
        bg_lut[d] = blend_colors(pal[PAL_BG], pal[PAL_BG_EDGE], t);
    }
}

static void draw_face(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    if (active_pal != sp->pal) {
        build_bg_lut(sp->pal);
        active_pal = sp->pal;
    }
    int dy = y - CENTER_Y;
    int dy_sq = dy * dy;
    for (int x = 0; x < SCREEN_W; x++) {
        int dx = x - CENTER_X;
        int d_sq = dx * dx + dy_sq;
        int d = (int)sqrtf((float)d_sq);
        if (d >= BG_GRADIENT_MAX) d = BG_GRADIENT_MAX - 1;
        buf[x] = bg_lut[d];
    }
}

/* ── Hexagonal eye with LED glow ──────────────────────────── */
static void draw_eye_impl(int y, const eye_params_t *ep, int eye_cx, int eye_cy,
                          const uint16_t *pal, uint16_t *buf) {
    const float eye_w = 30.0f;  // hex half-width
    const float eye_h = 22.0f;  // hex half-height

    float fy = y - eye_cy;
    if (fy < -eye_h - 4 || fy > eye_h + 4) return;

    int x_start = eye_cx - (int)eye_w - 4;
    int x_end   = eye_cx + (int)eye_w + 4;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    float lid_open = 1.0f - (ep->top_lid_mid.dy * 1.4f);
    if (lid_open < 0.05f) lid_open = 0.05f;
    if (lid_open > 1.0f) lid_open = 1.0f;

    float iris_cx = ep->iris_center.dx * 6.0f;
    float iris_cy = ep->iris_center.dy * 6.0f;

    for (int x = x_start; x <= x_end; x++) {
        float fx = x - eye_cx;
        // Hexagon test: shrink by lid_open vertically
        float hx = fx / eye_w;
        float hy = fy / (eye_h * lid_open);
        float hex = fmaxf(fabsf(hx), fabsf(hy));
        float hex_alt = fabsf(hx) * 0.866f + fabsf(hy) * 0.5f;
        float hex_dist = fmaxf(hex, hex_alt);

        if (hex_dist > 1.0f) continue;

        // LED panel interior
        float iris_d = sqrtf(dist_sq(fx, fy, iris_cx, iris_cy));
        float iris_r = 18.0f;

        if (iris_d < iris_r) {
            // Center LED glow
            float glow = expf(-iris_d * iris_d / (iris_r * iris_r * 0.25f));
            buf[x] = blend_colors(buf[x], pal[PAL_SHINE], glow * 0.8f * ep->shine_intensity);
            // Iris ring
            float ring = iris_d / iris_r;
            uint16_t led_color = blend_colors(pal[PAL_IRIS], pal[PAL_SHINE], glow * 0.5f);
            uint16_t dark_led = blend_colors(pal[PAL_IRIS], pal[PAL_PUPIL], 0.6f);
            buf[x] = blend_colors(buf[x], blend_colors(led_color, dark_led, ring * ring), 0.85f);
        } else {
            // Panel surface
            buf[x] = blend_colors(buf[x], pal[PAL_SCLERA], 0.7f);
        }

        // Hex frame edge
        float frame = 1.0f - hex_dist;
        if (frame < 0.08f) {
            buf[x] = blend_colors(buf[x], pal[PAL_BROW], (0.08f - frame) / 0.08f * 0.9f);
        }
    }
}

static void draw_eye_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X - (int)sp->eye_half_spacing;
    draw_eye_impl(y, &st->eye[0], cx, CENTER_Y, sp->pal, buf);
}
static void draw_eye_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X + (int)sp->eye_half_spacing;
    draw_eye_impl(y, &st->eye[1], cx, CENTER_Y, sp->pal, buf);
}

/* ── Robot brows: angular lines ───────────────────────────── */
static void draw_brow_impl(int y, const brow_params_t *bp, int eye_cx, int eye_cy,
                           const uint16_t *pal, float brow_y_offset, uint16_t *buf) {
    float brow_y_px = eye_cy + brow_y_offset;
    float dy = y - brow_y_px;
    float half_thick = bp->thickness * 2.5f;
    if (fabsf(dy) > half_thick) return;

    float inner_x = eye_cx + bp->inner.dx * 22.0f;
    float tail_x  = eye_cx + bp->tail.dx * 28.0f;
    int x_start = (int)inner_x - 3;
    int x_end   = (int)tail_x + 3;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    for (int x = x_start; x <= x_end; x++) {
        float t = (float)(x - inner_x) / (tail_x - inner_x + 0.001f);
        if (t < 0.0f || t > 1.0f) continue;
        // Angular brow: straight line with slight angle change
        float angle_y = brow_y_px + bp->arch.dy * 15.0f * sinf(t * 3.14159f);
        float dist = fabsf(y - angle_y);
        if (dist < half_thick) {
            float alpha = (half_thick - dist) / half_thick * 0.85f;
            buf[x] = blend_colors(buf[x], pal[PAL_BROW], alpha);
        }
    }
}

static void draw_brow_left(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X - (int)sp->eye_half_spacing;
    draw_brow_impl(y, &st->brow[0], cx, CENTER_Y, sp->pal, sp->brow_y_offset, buf);
}
static void draw_brow_right(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    int cx = CENTER_X + (int)sp->eye_half_spacing;
    draw_brow_impl(y, &st->brow[1], cx, CENTER_Y, sp->pal, sp->brow_y_offset, buf);
}

/* ── Robot mouth: rectangular grid ────────────────────────── */
static void draw_mouth(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    const mouth_params_t *mp = &st->mouth;
    int mouth_cy = CENTER_Y + (int)sp->mouth_y_center;
    const uint16_t *pal = sp->pal;
    float half_width = 24.0f;
    float openness_offset = mp->openness * 10.0f;

    int x_start = CENTER_X - (int)half_width - 3;
    int x_end   = CENTER_X + (int)half_width + 3;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    float top_y = mouth_cy - openness_offset;
    float bot_y = mouth_cy + openness_offset;

    for (int x = x_start; x <= x_end; x++) {
        float t = (float)(x - x_start) / (x_end - x_start);
        if (t < 0.0f || t > 1.0f) continue;
        float inset = (1.0f - fabsf(t - 0.5f) * 2.0f) * half_width * 0.15f;
        float current_top = top_y + inset;
        float current_bot = bot_y - inset;

        if (y >= current_top - 1.0f && y <= current_bot + 1.0f) {
            if (y > current_top + 1.0f && y < current_bot - 1.0f && mp->openness > 0.05f) {
                // Grid bars inside open mouth
                if (((int)(y - current_top) / 4) % 2 == 0) {
                    buf[x] = blend_colors(buf[x], pal[PAL_PUPIL], 0.6f);
                } else {
                    buf[x] = blend_colors(buf[x], pal[PAL_MOUTH], 0.4f);
                }
            } else {
                buf[x] = blend_colors(buf[x], pal[PAL_MOUTH], 0.8f);
            }
        }
    }
}

/* ── Robot blush: LED-style glow ──────────────────────────── */
static void draw_blush(int y, const face_state_t *st, const sprite_set_t *sp, uint16_t *buf) {
    float level = st->decor.blush;
    if (level <= 0) return;
    const uint16_t *pal = sp->pal;
    int blush_cy = CENTER_Y + (int)sp->blush_y_offset;
    float blush_r = 18.0f;
    float dy = y - blush_cy;
    if (fabsf(dy) > blush_r) return;

    for (int side = -1; side <= 1; side += 2) {
        int cx = CENTER_X + side * 55;
        for (int x = cx - (int)blush_r - 1; x <= cx + (int)blush_r + 1; x++) {
            if (x < 0 || x >= SCREEN_W) continue;
            float d = sqrtf(dist_sq(x, y, cx, blush_cy));
            if (d < blush_r) {
                float glow = (1.0f - d / blush_r) * level;
                buf[x] = blend_colors(buf[x], pal[PAL_BLUSH], glow * glow * 0.7f);
            }
        }
    }
}

/* ── Robot decor: panel lines + indicators ────────────────── */
static void draw_decor_overlay(int y, const face_state_t *st,
                                const sprite_set_t *sp, uint16_t *buf) {
    const decor_params_t *dp = &st->decor;
    const uint16_t *pal = sp->pal;

    // Panel lines: vertical seams
    if (y >= CENTER_Y - 60 && y <= CENTER_Y + 60) {
        for (int px : {CENTER_X - 50, CENTER_X + 50}) {
            if (px < 0 || px >= SCREEN_W) continue;
            buf[px] = blend_colors(buf[px], pal[PAL_BROW], 0.25f);
            if (px + 1 < SCREEN_W) buf[px + 1] = blend_colors(buf[px + 1], pal[PAL_BROW], 0.15f);
        }
    }
    // Horizontal seam
    int seam_y = CENTER_Y - 40;
    if (y >= seam_y - 1 && y <= seam_y + 1) {
        for (int x = CENTER_X - 60; x <= CENTER_X + 60; x++) {
            if (x < 0 || x >= SCREEN_W) continue;
            buf[x] = blend_colors(buf[x], pal[PAL_BROW], 0.2f);
        }
    }

    // Status indicators (blink based on decor params)
    if (dp->stars > 0) {
        // Top indicator
        if (y >= CENTER_Y - 100 && y <= CENTER_Y - 95) {
            for (int x = CENTER_X - 8; x <= CENTER_X + 8; x++) {
                if (x < 0 || x >= SCREEN_W) continue;
                buf[x] = blend_colors(buf[x], pal[PAL_STAR], dp->stars * 0.8f);
            }
        }
    }

    if (dp->sparkle > 0) {
        // Side indicators
        for (int side = -1; side <= 1; side += 2) {
            int ix = CENTER_X + side * 70;
            if (y >= CENTER_Y - 8 && y <= CENTER_Y + 8 && ix >= 0 && ix < SCREEN_W) {
                for (int x = ix - 3; x <= ix + 3; x++) {
                    if (x < 0 || x >= SCREEN_W) continue;
                    buf[x] = blend_colors(buf[x], pal[PAL_SHINE], dp->sparkle * 0.6f);
                }
            }
        }
    }
}

/* ── Sprite definition ──────────────────────────────────────── */
const sprite_set_t SPRITE_ROBOT = {
    .name = "robot",
    .eye_radius = 30.0f,
    .eye_half_spacing = 28.0f,
    .mouth_y_center = 50.0f,
    .brow_y_offset = -38.0f,
    .blush_y_offset = 33.0f,
    .draw_face = draw_face,
    .draw_blush = draw_blush,
    .draw_mouth = draw_mouth,
    .draw_eye_left = draw_eye_left,
    .draw_eye_right = draw_eye_right,
    .draw_brow_left = draw_brow_left,
    .draw_brow_right = draw_brow_right,
    .draw_decor_overlay = draw_decor_overlay,
    .pal = PALETTE_ROBOT,
};
