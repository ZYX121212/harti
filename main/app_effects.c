#include "app_effects.h"
#include <math.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

#define RGB(r,g,b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3))

typedef enum { PHASE_IN, PHASE_HOLD, PHASE_OUT } effect_phase_t;

static effect_type_t current_effect = EFFECT_NONE;
static float effect_timer = 0;
static effect_phase_t effect_phase = PHASE_IN;

static const float FADE_IN_DURATION  = 0.3f;
static const float HOLD_DURATION     = 1.5f;
static const float FADE_OUT_DURATION = 0.5f;

void effects_trigger(effect_type_t type) {
    if (type >= EFFECT_COUNT) return;
    // 高级特效可以打断低级
    if (current_effect != EFFECT_NONE && type <= current_effect) return;
    current_effect = type;
    effect_phase = PHASE_IN;
    effect_timer = 0;
}

void effects_update(float dt) {
    if (current_effect == EFFECT_NONE) return;
    effect_timer += dt;
    switch (effect_phase) {
        case PHASE_IN:
            if (effect_timer >= FADE_IN_DURATION) {
                effect_timer -= FADE_IN_DURATION;
                effect_phase = PHASE_HOLD;
            }
            break;
        case PHASE_HOLD:
            if (effect_timer >= HOLD_DURATION) {
                effect_timer -= HOLD_DURATION;
                effect_phase = PHASE_OUT;
            }
            break;
        case PHASE_OUT:
            if (effect_timer >= FADE_OUT_DURATION) {
                current_effect = EFFECT_NONE;
                effect_timer = 0;
                effect_phase = PHASE_IN;
            }
            break;
    }
}

static float current_alpha(void) {
    if (current_effect == EFFECT_NONE) return 0;
    switch (effect_phase) {
        case PHASE_IN:  return effect_timer / FADE_IN_DURATION;
        case PHASE_HOLD: return 1.0f;
        case PHASE_OUT: return 1.0f - (effect_timer / FADE_OUT_DURATION);
    }
    return 0;
}

// 定点数颜色混合 (与 expressive_eyes 中一致的实现)
static inline uint16_t ef_blend(uint16_t c1, uint16_t c2, float t) {
    if (t <= 0) return c1;
    if (t >= 1) return c2;
    int t256 = (int)(t * 256.0f);
    int r = ((c1 >> 11) & 0x1F) + (((((c2 >> 11) & 0x1F) - ((c1 >> 11) & 0x1F)) * t256 + 128) >> 8);
    int g = ((c1 >> 5) & 0x3F)  + (((((c2 >> 5) & 0x3F)  - ((c1 >> 5) & 0x3F))  * t256 + 128) >> 8);
    int b = (c1 & 0x1F)        + ((((c2 & 0x1F)         - (c1 & 0x1F))         * t256 + 128) >> 8);
    return (r << 11) | (g << 5) | b;
}

// 软光叠加: 在位置(px,py)以半径radius和颜色color叠加到像素上
static void soft_add(uint16_t *pixel, float px, float py, float cx, float cy,
                     float radius, uint16_t color, float alpha) {
    float dx = px - cx, dy = py - cy;
    float d_sq = dx * dx + dy * dy;
    float r_sq = radius * radius;
    if (d_sq < r_sq) {
        float t = (1.0f - d_sq / r_sq) * alpha;
        *pixel = ef_blend(*pixel, color, t);
    }
}

void effects_apply_line(int y, uint16_t *line_buf, int screen_w) {
    if (current_effect == EFFECT_NONE) return;

    float alpha = current_alpha();
    if (alpha <= 0.01f) return;

    uint16_t star_color  = RGB(255, 220, 0);   // 金黄
    uint16_t heart_color = RGB(255, 100, 130); // 粉红
    uint16_t gold_color  = RGB(255, 215, 0);   // 金色

    switch (current_effect) {

    case EFFECT_STAR: {
        // 单颗星从头顶缓慢上升
        float star_cy = CENTER_Y - 60 + effect_timer * 15.0f;
        for (int x = 0; x < SCREEN_W; x++) {
            soft_add(&line_buf[x], (float)x, (float)y,
                     CENTER_X, star_cy, 8.0f, star_color, alpha * 0.8f);
        }
        break;
    }

    case EFFECT_HEART_PARTICLE: {
        // 3颗心形粒子从底部上升, 带横向摆动
        for (int p = 0; p < 3; p++) {
            float px = CENTER_X - 15 + p * 15.0f;
            float rise = effect_timer * 40.0f;
            if (rise > 70.0f) rise = 70.0f;
            float py = CENTER_Y + 50 - rise + sinf(effect_timer * 3.0f + (float)p) * 8.0f;
            for (int x = 0; x < SCREEN_W; x++) {
                soft_add(&line_buf[x], (float)x, (float)y,
                         px, py, 5.0f, heart_color, alpha * 0.75f);
            }
        }
        break;
    }

    case EFFECT_RAINBOW: {
        // 水平彩虹弧线在屏幕上方
        float arc_cy = CENTER_Y - 70, arc_r = 80.0f, thick = 12.0f;
        float dy = y - arc_cy;
        if (dy >= -thick && dy <= thick) {
            for (int x = 0; x < SCREEN_W; x++) {
                float d = sqrtf((x - CENTER_X) * (x - CENTER_X) + dy * dy);
                float ring = fabsf(d - arc_r);
                if (ring < thick) {
                    float angle = atan2f(dy, x - CENTER_X);
                    float hue = (angle + 3.1416f) / 6.2832f;
                    uint16_t color;
                    if (hue < 0.166f)      color = RGB(255, 0, 0);
                    else if (hue < 0.333f) color = RGB(255, 165, 0);
                    else if (hue < 0.5f)   color = RGB(255, 255, 0);
                    else if (hue < 0.666f) color = RGB(0, 255, 0);
                    else if (hue < 0.833f) color = RGB(0, 100, 255);
                    else                   color = RGB(128, 0, 255);
                    float t = (1.0f - ring / thick) * alpha * 0.5f;
                    line_buf[x] = ef_blend(line_buf[x], color, t);
                }
            }
        }
        break;
    }

    case EFFECT_GOLDEN: {
        // 6个同心衍射环从中心向外扩散
        for (int r = 0; r < 6; r++) {
            float ring_r = 15.0f + r * 18.0f + effect_timer * 8.0f;
            float ring_thick = 5.0f - r * 0.4f;
            if (ring_thick < 1.5f) ring_thick = 1.5f;
            for (int x = 0; x < SCREEN_W; x++) {
                float d = sqrtf((x - CENTER_X) * (x - CENTER_X) +
                               (y - CENTER_Y) * (y - CENTER_Y));
                float ring = fabsf(d - ring_r);
                if (ring < ring_thick) {
                    float t = (1.0f - ring / ring_thick) * alpha * 0.55f;
                    line_buf[x] = ef_blend(line_buf[x], gold_color, t);
                }
            }
        }
        break;
    }

    default: break;
    }
}
