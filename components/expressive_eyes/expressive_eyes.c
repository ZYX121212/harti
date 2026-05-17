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

typedef struct {
    uint16_t bg;
    uint16_t bg_edge;
    uint16_t sclera;
    uint16_t iris;
    uint16_t pupil;
    uint16_t blush;
    uint16_t tear;
    uint16_t shine;
    uint16_t star;
} color_palette_t;

static const color_palette_t PALETTE_WHITE = {
    .bg      = RGB(255, 255, 255),  // 纯白背景
    .bg_edge = RGB(235, 235, 235),  // 浅灰边缘
    .sclera  = RGB(255, 255, 255),  // 眼白
    .iris    = RGB(40, 40, 40),     // 深灰虹膜
    .pupil   = RGB(0, 0, 0),        // 纯黑瞳孔
    .blush   = RGB(255, 160, 160),  // 淡粉腮红
    .tear    = RGB(180, 210, 255),  // 淡蓝眼泪
    .shine   = RGB(255, 255, 255),  // 白色高光
    .star    = RGB(255, 220, 0),    // 金黄星星
};

static const color_palette_t PALETTE_BLACK = {
    .bg      = RGB(0, 0, 0),        // 纯黑背景
    .bg_edge = RGB(20, 20, 20),     // 深灰边缘
    .sclera  = RGB(35, 35, 35),     // 深灰眼眶
    .iris    = RGB(160, 160, 160),  // 中灰虹膜 (发光感)
    .pupil   = RGB(255, 255, 255),  // 纯白瞳孔
    .blush   = RGB(60, 30, 30),     // 暗红腮红
    .tear    = RGB(40, 60, 80),     // 暗蓝眼泪
    .shine   = RGB(255, 255, 255),  // 白色高光
    .star    = RGB(255, 255, 100),  // 亮黄星星
};

// 心形查表: 每行(y)的最大x偏移 (心形关于x轴对称)
// y从 -13 到 +13, 共27行. 值0表示该行不在心形内.
static const int8_t heart_row_half[27] = {
    // y = -13 .. -10 (顶部凹陷, 两瓣之间)
    0, 0, 0, 0,
    // y = -9 .. -7 (两瓣开始)
    2, 4, 5,
    // y = -6 .. -4 (最大宽度)
    6, 7, 7,
    // y = -3 .. -1
    7, 6, 6,
    // y = 0 (中心)
    6,
    // y = 1 .. 3
    5, 5, 4,
    // y = 4 .. 6 (逐渐收窄)
    4, 3, 2,
    // y = 7 .. 9
    2, 1, 1,
    // y = 10 .. 13 (底部尖角)
    0, 0, 0, 0,
};

static eye_state_t current_state;
static uint16_t line_buf[SCREEN_W];
static color_scheme_t current_scheme = COLOR_SCHEME_WHITE;

static inline const color_palette_t *get_palette(void)
{
    return (current_scheme == COLOR_SCHEME_BLACK) ? &PALETTE_BLACK : &PALETTE_WHITE;
}

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
    .star_level = 0,
    .heart_mode = false
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
    .star_level = 0,
    .heart_mode = false
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
    .star_level = 0,
    .heart_mode = false
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
    .star_level = 0,
    .heart_mode = false
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
    .star_level = 0,
    .heart_mode = false
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
    .star_level = 0,
    .heart_mode = false
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
    .star_level = 0,
    .heart_mode = false
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
    .star_level = 0.65f,
    .heart_mode = false
};

const eye_state_t EYE_STATE_HEART_EYES = {
    .eye_offset_x = 0,
    .eye_offset_y = -2,
    .eye_separation = 52,
    .left_lid_open = 0.9f,
    .right_lid_open = 0.9f,
    .pupil_x = 0,
    .pupil_y = 0,
    .pupil_scale = 0.0f,  // 心形眼无瞳孔
    .curve_up = 0.35f,
    .curve_down = 0,
    .blush_level = 0.65f,
    .tear_level = 0,
    .star_level = 0.9f,
    .heart_mode = true,
};

static inline uint16_t blend_colors(uint16_t c1, uint16_t c2, float t)
{
    if (t <= 0) return c1;
    if (t >= 1) return c2;

    // 用 8.8 定点数代替浮点乘法
    int t256 = (int)(t * 256.0f);

    int r1 = (c1 >> 11) & 0x1F;
    int g1 = (c1 >> 5) & 0x3F;
    int b1 = c1 & 0x1F;

    int r2 = (c2 >> 11) & 0x1F;
    int g2 = (c2 >> 5) & 0x3F;
    int b2 = c2 & 0x1F;

    int r = r1 + (((r2 - r1) * t256 + 128) >> 8);
    int g = g1 + (((g2 - g1) * t256 + 128) >> 8);
    int b = b1 + (((b2 - b1) * t256 + 128) >> 8);

    return (r << 11) | (g << 5) | b;
}

static inline float dist_sq(float x1, float y1, float x2, float y2)
{
    float dx = x1 - x2;
    float dy = y1 - y2;
    return dx * dx + dy * dy;
}

// 背景渐变查找表: 按距离索引 (0..170)
#define BG_GRADIENT_MAX_DIST 171
static uint16_t bg_gradient_lut[BG_GRADIENT_MAX_DIST];

// 快速整数平方根 (用于值 <= ~30000)
static inline int fast_isqrt(int n)
{
    int r = 0;
    int bit = 1 << 14;
    while (bit > 0) {
        if (n >= r + bit) {
            n -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

// 构建背景渐变 LUT
static void build_bg_gradient_lut(void)
{
    const color_palette_t *pal = get_palette();
    for (int d = 0; d < BG_GRADIENT_MAX_DIST; d++) {
        float t = d / 160.0f;
        if (t > 1.0f) t = 1.0f;
        bg_gradient_lut[d] = blend_colors(pal->bg, pal->bg_edge, t);
    }
}

// 渲染单只眼睛
static void render_eye(int y, int eye_cx, int eye_cy, float lid_open,
                       float pup_x, float pup_y, float pup_scale,
                       float curve_up, float curve_down, bool is_left)
{
    const color_palette_t *pal = get_palette();
    const float eye_r = 36.0f;              // 眼睛半径
    const float iris_r = 20.0f;             // 虹膜半径
    const float pupil_base_r = 10.0f;       // 瞳孔基础半径

    // 心形查表偏移: y从-13到+13 (共27行)
    const int heart_table_offset = 13;

    // 边界框裁剪 (多留1像素给边缘抗锯齿)
    int x_start = eye_cx - (int)eye_r - 2;
    int x_end = eye_cx + (int)eye_r + 2;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    // 快速跳过不在眼睛内的扫描行
    float fy = y - eye_cy;
    if (fy < -eye_r - 1.5f || fy > eye_r + 1.5f) return;

    // 预计算虹膜/瞳孔位置 (不变量)
    float iris_cx = pup_x * 8.0f;
    float iris_cy = pup_y * 8.0f;
    float iris_r_sq = iris_r * iris_r;
    float pupil_r = pupil_base_r * pup_scale;
    float pupil_r_sq = pupil_r * pupil_r;
    float pupil2_cx = iris_cx + pup_x * 2;
    float pupil2_cy = iris_cy + pup_y * 2;

    // 眼睑遮罩
    float lid_curve = curve_up - curve_down;
    float top_lid_y = -eye_r * (1.0f - lid_open) - lid_curve * 15.0f;
    float bot_lid_y = eye_r * (1.0f - lid_open) + lid_curve * 10.0f;

    // 高光位置 (主高光 + 副高光)
    const float shine1_cx = -8.0f, shine1_cy = -10.0f;
    const float shine2_cx = 5.0f, shine2_cy = -5.0f;

    for (int x = x_start; x <= x_end; x++) {
        float fx = x - eye_cx;

        // 计算到眼睛中心的距离
        float r_sq = fx * fx + fy * fy;
        float eye_r_sq = eye_r * eye_r;

        if (r_sq >= eye_r_sq) continue;

        // 边缘抗锯齿: 在眼睛边缘1.5像素内做渐变
        // 仅在可能接近边缘时计算 sqrtf (优化: 用 r_sq 范围预判)
        float r_sq_min = (eye_r - 1.5f) * (eye_r - 1.5f);
        bool is_edge = (r_sq >= r_sq_min);

        if (is_edge) {
            float edge_dist = eye_r - sqrtf(r_sq);
            // 边缘过渡: 用背景色和眼睛内容色混合
            float aa = edge_dist / 1.5f;
            if (fy < top_lid_y || fy > bot_lid_y) {
                // 被眼睑遮挡的边缘
                line_buf[x] = blend_colors(line_buf[x], pal->sclera, aa * 0.3f);
            } else {
                // 可见边缘 - 计算内部颜色
                float iris_d_sq = dist_sq(fx, fy, iris_cx, iris_cy);
                float pupil_d_sq = dist_sq(fx, fy, pupil2_cx, pupil2_cy);
                float shine1_d_sq = dist_sq(fx, fy, shine1_cx, shine1_cy);
                float shine2_d_sq = dist_sq(fx, fy, shine2_cx, shine2_cy);

                uint16_t inner_color;
                if (shine1_d_sq < 25.0f && iris_d_sq < iris_r_sq)
                    inner_color = blend_colors(pal->iris, pal->shine, 0.9f);
                else if (shine2_d_sq < 9.0f && iris_d_sq < iris_r_sq)
                    inner_color = blend_colors(pal->iris, pal->shine, 0.7f);
                else if (pupil_d_sq < pupil_r_sq)
                    inner_color = pal->pupil;
                else if (iris_d_sq < iris_r_sq)
                    inner_color = pal->iris;
                else
                    inner_color = pal->sclera;

                line_buf[x] = blend_colors(line_buf[x], inner_color, aa);
            }
            continue;
        }

        // 在眼睛内部 (非边缘)
        {
            bool visible = true;
            if (fy < top_lid_y) visible = false;
            if (fy > bot_lid_y) visible = false;

            if (visible) {
                float iris_d_sq = dist_sq(fx, fy, iris_cx, iris_cy);
                float pupil_d_sq = dist_sq(fx, fy, pupil2_cx, pupil2_cy);
                float shine1_d_sq = dist_sq(fx, fy, shine1_cx, shine1_cy);
                float shine2_d_sq = dist_sq(fx, fy, shine2_cx, shine2_cy);

                if (current_state.heart_mode) {
                    // 心形虹膜: 用查表判断
                    int hx = (int)(fx - iris_cx);
                    int hy = (int)(fy - iris_cy);
                    int row = hy + heart_table_offset;
                    bool in_heart = false;
                    if (row >= 0 && row < 27) {
                        int max_x = heart_row_half[row];
                        if (max_x > 0 && abs(hx) <= max_x) {
                            in_heart = true;
                        }
                    }
                    if (in_heart) {
                        // 高光
                        if (shine1_d_sq < 25.0f) {
                            line_buf[x] = blend_colors(pal->iris, pal->shine, 0.85f);
                        } else if (shine2_d_sq < 9.0f) {
                            line_buf[x] = blend_colors(pal->iris, pal->shine, 0.65f);
                        } else {
                            line_buf[x] = pal->iris;
                        }
                    }
                    // 心形眼无瞳孔，sclera 已在背景层中
                } else {
                    // 原始圆形虹膜
                    // 主高光 (大) + 副高光 (小)
                    if (shine1_d_sq < 25.0f && iris_d_sq < iris_r_sq) {
                        line_buf[x] = blend_colors(line_buf[x], pal->shine, 0.9f);
                    }
                    else if (shine2_d_sq < 9.0f && iris_d_sq < iris_r_sq) {
                        line_buf[x] = blend_colors(line_buf[x], pal->shine, 0.7f);
                    }
                    else if (pupil_d_sq < pupil_r_sq) {
                        line_buf[x] = pal->pupil;
                    }
                    else if (iris_d_sq < iris_r_sq) {
                        line_buf[x] = pal->iris;
                    }
                    else {
                        line_buf[x] = pal->sclera;
                    }
                }
            }
        }
    }
}

// 渲染腮红
static void render_blush(int y, float level)
{
    if (level <= 0) return;

    const color_palette_t *pal = get_palette();
    int blush_y = CENTER_Y + 35;
    int left_x = CENTER_X - 60;
    int right_x = CENTER_X + 60;
    float blush_r = 24.0f;
    float r_sq = blush_r * blush_r;

    // 快速跳过不在腮红范围内的扫描行
    float dy = y - blush_y;
    if (dy < -blush_r || dy > blush_r) return;

    // 边界框: 覆盖左右两个腮红
    int x_start = left_x - (int)blush_r - 1;
    int x_end = right_x + (int)blush_r + 1;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    for (int x = x_start; x <= x_end; x++) {
        float d_left = dist_sq(x, y, left_x, blush_y);
        float d_right = dist_sq(x, y, right_x, blush_y);

        if (d_left < r_sq || d_right < r_sq) {
            float d = (d_left < d_right) ? d_left : d_right;
            float t = (1.0f - d / r_sq) * level * 0.7f;
            line_buf[x] = blend_colors(line_buf[x], pal->blush, t);
        }
    }
}

// 渲染眼泪
static void render_tears(int y, float level)
{
    if (level <= 0) return;

    const color_palette_t *pal = get_palette();
    int tear_y = CENTER_Y + 30;
    int left_x = CENTER_X - 35;
    int right_x = CENTER_X + 35;

    float dy = y - tear_y;
    if (dy <= 0 || dy >= 30 * level) return;

    // 水滴形状
    float r_left = 6.0f - dy * 0.08f;
    float r_right = 6.5f - dy * 0.08f;

    // 边界框: 覆盖左右眼泪
    int x_start = left_x - (int)r_left - 2;
    int x_end = right_x + (int)r_right + 2;
    if (x_start < 0) x_start = 0;
    if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

    float dy5 = dy - 5;
    float dy8 = dy - 8;

    for (int x = x_start; x <= x_end; x++) {
        float dx_left = x - left_x;
        float dx_right = x - right_x;

        if (dx_left * dx_left + dy5 * dy5 < r_left * r_left) {
            line_buf[x] = blend_colors(line_buf[x], pal->tear, 0.8f);
        }
        if (dx_right * dx_right + dy8 * dy8 < r_right * r_right) {
            line_buf[x] = blend_colors(line_buf[x], pal->tear, 0.8f);
        }
    }
}

// 渲染星星眼
static void render_stars(int y, float level)
{
    if (level <= 0) return;

    const color_palette_t *pal = get_palette();
    static const int star_positions[4][2] = {
        {CENTER_X - 20, CENTER_Y - 15},
        {CENTER_X + 20, CENTER_Y - 15},
        {CENTER_X - 35, CENTER_Y + 5},
        {CENTER_X + 35, CENTER_Y + 5},
    };

    for (int i = 0; i < 4; i++) {
        int sx = star_positions[i][0];
        int sy = star_positions[i][1];

        // 边界框裁剪: 星星半径 ~6
        if (y < sy - 7 || y > sy + 7) continue;

        int x_start = sx - 7;
        int x_end = sx + 7;
        if (x_start < 0) x_start = 0;
        if (x_end >= SCREEN_W) x_end = SCREEN_W - 1;

        for (int x = x_start; x <= x_end; x++) {
            float d_sq = dist_sq(x, y, sx, sy);
            if (d_sq < 36.0f) {
                float t = (1.0f - d_sq / 36.0f) * level;
                line_buf[x] = blend_colors(line_buf[x], pal->star, t * 0.7f);
            }
        }
    }
}

void eyes_render_frame(void)
{
    gc9a01_set_window(0, 0, SCREEN_W - 1, SCREEN_H - 1);

    const color_palette_t *pal = get_palette();

    // 眼睛中心位置
    float sep_half = current_state.eye_separation * 0.5f;
    int left_cx = CENTER_X - sep_half + current_state.eye_offset_x;
    int right_cx = CENTER_X + sep_half + current_state.eye_offset_x;
    int eye_cy = CENTER_Y + current_state.eye_offset_y;

    for (int y = 0; y < SCREEN_H; y++) {
        // 1. 背景层：径向渐变 (使用 LUT + 整数平方根)
        {
            int dy = y - CENTER_Y;
            int dy_sq = dy * dy;
            for (int x = 0; x < SCREEN_W; x++) {
                int dx = x - CENTER_X;
                int d_sq = dx * dx + dy_sq;
                int d = fast_isqrt(d_sq);
                if (d >= BG_GRADIENT_MAX_DIST) d = BG_GRADIENT_MAX_DIST - 1;
                line_buf[x] = bg_gradient_lut[d];
            }
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
        render_stars(y, current_state.star_level);

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
    out->heart_mode = b->heart_mode;
}

void eyes_set_color_scheme(color_scheme_t scheme)
{
    if (scheme >= COLOR_SCHEME_COUNT) return;
    current_scheme = scheme;
    build_bg_gradient_lut();
}

void eyes_init(void)
{
    current_state = EYE_STATE_NEUTRAL;
    current_scheme = COLOR_SCHEME_WHITE;
    build_bg_gradient_lut();
}
