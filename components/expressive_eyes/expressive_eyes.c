#include "expressive_eyes.h"
#include "../gc9a01/gc9a01.h"
#include <string.h>
#include <math.h>

#define SCREEN_W 240
#define SCREEN_H 240
#define CENTER_X 120
#define CENTER_Y 120

// 颜色定义 (RGB565)
#define RGB(r,g,b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3))

#define COLOR_BG         RGB(245, 240, 235)  // 暖米色背景
#define COLOR_SCLERA     RGB(255, 255, 255)  // 眼白
#define COLOR_IRIS       RGB(100, 140, 220)  // 虹膜 (蓝色)
#define COLOR_PUPIL      RGB(30, 30, 40)     // 瞳孔
#define COLOR_SKIN       RGB(255, 220, 210)  // 皮肤/腮红底色
#define COLOR_BLUSH      RGB(255, 160, 160)  // 腮红
#define COLOR_TEAR       RGB(180, 220, 255)  // 眼泪
#define COLOR_SHINE      RGB(255, 255, 255)  // 高光

static eye_state_t current_state;
static uint16_t line_buf[SCREEN_W];

// 预设状态定义
const eye_state_t EYE_STATE_NEUTRAL = {
    .eye_offset_x = 0,
    .eye_offset_y = 0,
    .eye_separation = 52,
    .left_lid_open = 1.0f,
    .right_lid_open = 1.0f,
    .pupil_x = 0,
    .pupil_y = 0,
    .pupil_scale = 0.6f,
    .curve_up = 0,
    .curve_down = 0,
    .blush_level = 0,
    .tear_level = 0,
    .star_level = 0
};

const eye_state_t EYE_STATE_HAPPY = {
    .eye_offset_x = 0,
    .eye_offset_y = -2,
    .eye_separation = 52,
    .left_lid_open = 0.82f,
    .right_lid_open = 0.82f,
    .pupil_x = 0,
    .pupil_y = -0.1f,
    .pupil_scale = 0.65f,
    .curve_up = 0.45f,
    .curve_down = 0,
    .blush_level = 0.55f,
    .tear_level = 0,
    .star_level = 0
};

const eye_state_t EYE_STATE_SAD = {
    .eye_offset_x = 0,
    .eye_offset_y = 6,
    .eye_separation = 52,
    .left_lid_open = 0.65f,
    .right_lid_open = 0.65f,
    .pupil_x = 0,
    .pupil_y = 0.3f,
    .pupil_scale = 0.55f,
    .curve_up = 0,
    .curve_down = 0.5f,
    .blush_level = 0.2f,
    .tear_level = 0.6f,
    .star_level = 0
};

const eye_state_t EYE_STATE_SURPRISED = {
    .eye_offset_x = 0,
    .eye_offset_y = -4,
    .eye_separation = 52,
    .left_lid_open = 1.0f,
    .right_lid_open = 1.0f,
    .pupil_x = 0,
    .pupil_y = 0,
    .pupil_scale = 0.45f,
    .curve_up = 0,
    .curve_down = 0,
    .blush_level = 0.15f,
    .tear_level = 0,
    .star_level = 0
};

const eye_state_t EYE_STATE_SLEEPY = {
    .eye_offset_x = 0,
    .eye_offset_y = 4,
    .eye_separation = 52,
    .left_lid_open = 0.35f,
    .right_lid_open = 0.35f,
    .pupil_x = 0,
    .pupil_y = 0.2f,
    .pupil_scale = 0.5f,
    .curve_up = 0.1f,
    .curve_down = 0,
    .blush_level = 0.1f,
    .tear_level = 0,
    .star_level = 0
};

const eye_state_t EYE_STATE_ANGRY = {
    .eye_offset_x = 0,
    .eye_offset_y = -2,
    .eye_separation = 54,
    .left_lid_open = 0.75f,
    .right_lid_open = 0.75f,
    .pupil_x = 0,
    .pupil_y = 0,
    .pupil_scale = 0.55f,
    .curve_up = 0,
    .curve_down = -0.35f, // 下弯眉
    .blush_level = 0,
    .tear_level = 0,
    .star_level = 0
};

const eye_state_t EYE_STATE_BORED = {
    .eye_offset_x = 0,
    .eye_offset_y = 2,
    .eye_separation = 52,
    .left_lid_open = 0.55f,
    .right_lid_open = 0.55f,
    .pupil_x = 0,
    .pupil_y = -0.2f,
    .pupil_scale = 0.55f,
    .curve_up = 0,
    .curve_down = 0.15f,
    .blush_level = 0,
    .tear_level = 0,
    .star_level = 0
};

const eye_state_t EYE_STATE_EXCITED = {
    .eye_offset_x = 0,
    .eye_offset_y = -4,
    .eye_separation = 52,
    .left_lid_open = 0.95f,
    .right_lid_open = 0.95f,
    .pupil_x = 0,
    .pupil_y = -0.1f,
    .pupil_scale = 0.7f,
    .curve_up = 0.35f,
    .curve_down = 0,
    .blush_level = 0.4f,
    .tear_level = 0,
    .star_level = 0.65f
};

static inline uint16_t blend_colors(uint16_t c1, uint16_t c2, float t)
{
    if (t <= 0) return c1;
    if (t >= 1) return c2;

    // RGB565 拆分成通道
    int r1 = (c1 >> 11) & 0x1F;
    int g1 = (c1 >> 5) & 0x3F;
    int b1 = c1 & 0x1F;

    int r2 = (c2 >> 11) & 0x1F;
    int g2 = (c2 >> 5) & 0x3F;
    int b2 = c2 & 0x1F;

    // 混合
    int r = r1 + (int)((r2 - r1) * t);
    int g = g1 + (int)((g2 - g1) * t);
    int b = b1 + (int)((b2 - b1) * t);

    return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
}

static inline float dist_sq(float x1, float y1, float x2, float y2)
{
    float dx = x1 - x2;
    float dy = y1 - y2;
    return dx * dx + dy * dy;
}

// 渲染单只眼睛
static void render_eye(int y, int eye_cx, int eye_cy, float lid_open,
                       float pup_x, float pup_y, float pup_scale,
                       float curve_up, float curve_down, bool is_left)
{
    const float eye_r = 36.0f;              // 眼睛半径
    const float iris_r = 20.0f;             // 虹膜半径
    const float pupil_base_r = 10.0f;       // 瞳孔基础半径

    for (int x = 0; x < SCREEN_W; x++) {
        float fx = x - eye_cx;
        float fy = y - eye_cy;

        // 计算到眼睛中心的距离平方
        float r_sq = dist_sq(fx, fy, 0, 0);
        float eye_r_sq = eye_r * eye_r;

        if (r_sq < eye_r_sq) {
            // 在眼睛范围内

            // 眼睑遮罩：根据 lid_open 计算上下眼睑位置
            float lid_curve = curve_up - curve_down;
            float top_lid_y = -eye_r * (1.0f - lid_open) - lid_curve * 15.0f;
            float bot_lid_y = eye_r * (1.0f - lid_open) + lid_curve * 10.0f;

            bool visible = true;
            if (fy < top_lid_y) visible = false;
            if (fy > bot_lid_y) visible = false;

            if (visible) {
                // 计算虹膜位置
                float iris_cx = pup_x * 8.0f;
                float iris_cy = pup_y * 8.0f;
                float iris_r_sq = iris_r * iris_r;
                float iris_d_sq = dist_sq(fx, fy, iris_cx, iris_cy);

                // 计算瞳孔位置
                float pupil_r = pupil_base_r * pup_scale;
                float pupil_r_sq = pupil_r * pupil_r;
                float pupil_d_sq = dist_sq(fx, fy, iris_cx + pup_x * 2, iris_cy + pup_y * 2);

                // 高光
                float shine_cx = -8, shine_cy = -10;
                float shine_d_sq = dist_sq(fx, fy, shine_cx, shine_cy);

                if (shine_d_sq < 25.0f && iris_d_sq < iris_r_sq) {
                    line_buf[x] = blend_colors(line_buf[x], COLOR_SHINE, 0.9f);
                }
                else if (pupil_d_sq < pupil_r_sq) {
                    line_buf[x] = COLOR_PUPIL;
                }
                else if (iris_d_sq < iris_r_sq) {
                    line_buf[x] = COLOR_IRIS;
                }
                else {
                    line_buf[x] = COLOR_SCLERA;
                }
            }
        }
    }
}

// 渲染腮红
static void render_blush(int y, float level)
{
    if (level <= 0) return;

    int blush_y = CENTER_Y + 35;
    int left_x = CENTER_X - 60;
    int right_x = CENTER_X + 60;
    float blush_r = 24.0f;

    for (int x = 0; x < SCREEN_W; x++) {
        float d_left = dist_sq(x, y, left_x, blush_y);
        float d_right = dist_sq(x, y, right_x, blush_y);
        float r_sq = blush_r * blush_r;

        if (d_left < r_sq || d_right < r_sq) {
            float d = (d_left < d_right) ? d_left : d_right;
            float t = (1.0f - d / r_sq) * level * 0.7f;
            line_buf[x] = blend_colors(line_buf[x], COLOR_BLUSH, t);
        }
    }
}

// 渲染眼泪
static void render_tears(int y, float level)
{
    if (level <= 0) return;

    int tear_y = CENTER_Y + 30;
    int left_x = CENTER_X - 35;
    int right_x = CENTER_X + 35;

    for (int x = 0; x < SCREEN_W; x++) {
        // 左眼泪
        float dy = y - tear_y;
        float dx_left = x - left_x;
        float dx_right = x - right_x;

        if (dy > 0) {
            // 水滴形状
            float r_left = 6.0f - dy * 0.08f;
            float r_right = 6.5f - dy * 0.08f;

            if (dx_left * dx_left + (dy - 5) * (dy - 5) < r_left * r_left && dy < 30 * level) {
                line_buf[x] = blend_colors(line_buf[x], COLOR_TEAR, 0.8f);
            }
            if (dx_right * dx_right + (dy - 8) * (dy - 8) < r_right * r_right && dy < 30 * level) {
                line_buf[x] = blend_colors(line_buf[x], COLOR_TEAR, 0.8f);
            }
        }
    }
}

// 渲染星星眼
static void render_stars(int y, float level)
{
    if (level <= 0) return;

    int star_positions[4][2] = {
        {CENTER_X - 20, CENTER_Y - 15},
        {CENTER_X + 20, CENTER_Y - 15},
        {CENTER_X - 35, CENTER_Y + 5},
        {CENTER_X + 35, CENTER_Y + 5},
    };

    for (int i = 0; i < 4; i++) {
        int sx = star_positions[i][0];
        int sy = star_positions[i][1];

        float d_sq = dist_sq(x, y, sx, sy);
        if (d_sq < 36.0f) {
            float t = (1.0f - d_sq / 36.0f) * level;
            line_buf[x] = blend_colors(line_buf[x], COLOR_YELLOW, t * 0.7f);
        }
    }
}

void eyes_render_frame(void)
{
    gc9a01_set_window(0, 0, SCREEN_W - 1, SCREEN_H - 1);

    // 眼睛中心位置
    float sep_half = current_state.eye_separation * 0.5f;
    int left_cx = CENTER_X - sep_half + current_state.eye_offset_x;
    int right_cx = CENTER_X + sep_half + current_state.eye_offset_x;
    int eye_cy = CENTER_Y + current_state.eye_offset_y;

    for (int y = 0; y < SCREEN_H; y++) {
        // 1. 背景层：径向渐变
        for (int x = 0; x < SCREEN_W; x++) {
            float d = sqrtf(dist_sq(x, y, CENTER_X, CENTER_Y));
            float t = d / 160.0f;
            if (t > 1) t = 1;
            // 从浅米色到稍深米色
            line_buf[x] = blend_colors(COLOR_BG, RGB(235, 228, 220), t);
        }

        // 2. 眼睛层
        render_eye(y, left_cx, eye_cy, current_state.left_lid_open,
                   current_state.pupil_x, current_state.pupil_y, current_state.pupil_scale,
                   current_state.curve_up, current_state.curve_down, true);
        render_eye(y, right_cx, eye_cy, current_state.right_lid_open,
                   current_state.pupil_x, current_state.pupil_y, current_state.pupil_scale,
                   current_state.curve_up, current_state.curve_down, false);

        // 3. 装饰层
        render_blush(y, current_state.blush_level);
        render_tears(y, current_state.tear_level);
        // render_stars(y, current_state.star_level); // 简单起见暂不实现

        // 发送这一行
        gc9a01_send_pixels(line_buf, SCREEN_W);
    }
}

void eyes_set_state(const eye_state_t *state)
{
    memcpy(&current_state, state, sizeof(eye_state_t));
}

void eyes_blend(const eye_state_t *a, const eye_state_t *b, float t, eye_state_t *out)
{
    out->eye_offset_x = a->eye_offset_x + (b->eye_offset_x - a->eye_offset_x) * t;
    out->eye_offset_y = a->eye_offset_y + (b->eye_offset_y - a->eye_offset_y) * t;
    out->eye_separation = a->eye_separation + (b->eye_separation - a->eye_separation) * t;
    out->left_lid_open = a->left_lid_open + (b->left_lid_open - a->left_lid_open) * t;
    out->right_lid_open = a->right_lid_open + (b->right_lid_open - a->right_lid_open) * t;
    out->pupil_x = a->pupil_x + (b->pupil_x - a->pupil_x) * t;
    out->pupil_y = a->pupil_y + (b->pupil_y - a->pupil_y) * t;
    out->pupil_scale = a->pupil_scale + (b->pupil_scale - a->pupil_scale) * t;
    out->curve_up = a->curve_up + (b->curve_up - a->curve_up) * t;
    out->curve_down = a->curve_down + (b->curve_down - a->curve_down) * t;
    out->blush_level = a->blush_level + (b->blush_level - a->blush_level) * t;
    out->tear_level = a->tear_level + (b->tear_level - a->tear_level) * t;
    out->star_level = a->star_level + (b->star_level - a->star_level) * t;
}

void eyes_init(void)
{
    current_state = EYE_STATE_NEUTRAL;
}
