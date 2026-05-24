#include "app_display.h"
#include "gc9a01.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include <math.h>
#include <stdlib.h>

static const char *TAG = "display";

// ========= 屏幕与颜色 =========
static const int16_t SCREEN_W = 240;
static const int16_t SCREEN_H = 240;
static const int16_t CENTER_X = SCREEN_W / 2;

static const uint16_t COLOR_BG = COLOR_BLACK;
static const uint16_t COLOR_WHITE_U16 = COLOR_WHITE;
static const uint16_t COLOR_BLACK_U16 = COLOR_BLACK;
static const uint16_t COLOR_PINK = 0xF388;

// ========= 布局 =========
static const int16_t EYE_Y = 100;
static const int16_t EYE_SPACING = 50;
static const int16_t EYE_RADIUS = 40;
static const int16_t MOUTH_Y = 160;

typedef enum {
    FACE_SMILE = 0,
    FACE_SURPRISE = 1,
    FACE_TALK = 2,
    FACE_MOOD_COUNT = 3
} face_mood_t;

static uint16_t frame_buf[SCREEN_H][SCREEN_W];

static uint64_t blink_timer = 0;
static uint64_t mood_timer = 0;
static bool is_blinking = false;
static int current_mood = FACE_SMILE;
static float eye_offset_x = 0.0f;
static float tap_shake_x = 0.0f;
static float tap_shake_y = 0.0f;

// 由 behavior 模块触发的“拍打抖动”帧计数
volatile int g_tap_shake_frames = 0;

static inline int rnd_range(int min_inclusive, int max_exclusive) {
    if (max_exclusive <= min_inclusive) return min_inclusive;
    return min_inclusive + (int)(esp_random() % (uint32_t)(max_exclusive - min_inclusive));
}

static inline bool in_bounds(int16_t x, int16_t y) {
    return x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H;
}

static inline void put_px(int16_t x, int16_t y, uint16_t color) {
    if (in_bounds(x, y)) {
        frame_buf[y][x] = color;
    }
}

static void fill_screen(uint16_t color) {
    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            frame_buf[y][x] = color;
        }
    }
}

static void draw_line(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        put_px((int16_t)x0, (int16_t)y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    int x = r;
    int y = 0;
    int err = 1 - x;

    while (x >= y) {
        put_px(cx + x, cy + y, color);
        put_px(cx + y, cy + x, color);
        put_px(cx - y, cy + x, color);
        put_px(cx - x, cy + y, color);
        put_px(cx - x, cy - y, color);
        put_px(cx - y, cy - x, color);
        put_px(cx + y, cy - x, color);
        put_px(cx + x, cy - y, color);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

static void fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    for (int16_t dy = -r; dy <= r; dy++) {
        int16_t yy = cy + dy;
        if (yy < 0 || yy >= SCREEN_H) continue;
        float fx = sqrtf((float)(r * r - dy * dy));
        int16_t dx = (int16_t)fx;
        int16_t x0 = cx - dx;
        int16_t x1 = cx + dx;
        if (x0 < 0) x0 = 0;
        if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
        for (int16_t x = x0; x <= x1; x++) {
            frame_buf[yy][x] = color;
        }
    }
}

static void fill_ellipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint16_t color) {
    if (rx <= 0 || ry <= 0) return;
    for (int16_t dy = -ry; dy <= ry; dy++) {
        int16_t yy = cy + dy;
        if (yy < 0 || yy >= SCREEN_H) continue;
        float norm_y = (float)dy / (float)ry;
        float xspan = rx * sqrtf(1.0f - norm_y * norm_y);
        int16_t dx = (int16_t)xspan;
        int16_t x0 = cx - dx;
        int16_t x1 = cx + dx;
        if (x0 < 0) x0 = 0;
        if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
        for (int16_t x = x0; x <= x1; x++) {
            frame_buf[yy][x] = color;
        }
    }
}

static void fill_arc_ring(int16_t cx,
                          int16_t cy,
                          int16_t outer_r,
                          int16_t inner_r,
                          float start_deg,
                          float end_deg,
                          uint16_t color) {
    const float pi = 3.14159265f;
    float start = start_deg * pi / 180.0f;
    float end = end_deg * pi / 180.0f;
    if (end < start) {
        float tmp = start;
        start = end;
        end = tmp;
    }

    int r2_outer = outer_r * outer_r;
    int r2_inner = inner_r * inner_r;
    int16_t min_x = cx - outer_r;
    int16_t max_x = cx + outer_r;
    int16_t min_y = cy - outer_r;
    int16_t max_y = cy + outer_r;
    if (min_x < 0) min_x = 0;
    if (max_x >= SCREEN_W) max_x = SCREEN_W - 1;
    if (min_y < 0) min_y = 0;
    if (max_y >= SCREEN_H) max_y = SCREEN_H - 1;

    for (int16_t y = min_y; y <= max_y; y++) {
        for (int16_t x = min_x; x <= max_x; x++) {
            int dx = x - cx;
            int dy = y - cy;
            int d2 = dx * dx + dy * dy;
            if (d2 > r2_outer || d2 < r2_inner) continue;
            float ang = atan2f((float)dy, (float)dx);
            if (ang < 0) ang += 2.0f * pi;
            if (ang >= start && ang <= end) {
                frame_buf[y][x] = color;
            }
        }
    }
}

static void flush_frame(void) {
    gc9a01_set_window(0, 0, SCREEN_W - 1, SCREEN_H - 1);
    for (int y = 0; y < SCREEN_H; y++) {
        gc9a01_send_pixels(frame_buf[y], SCREEN_W);
    }
}

static void draw_eye_gfx(int16_t x, int16_t y, int16_t radius, float look_x, float scale_y) {
    if (scale_y < 0.1f) {
        draw_line(x - radius, y, x + radius, y, COLOR_WHITE_U16);
        return;
    }

    fill_circle(x, y, radius, COLOR_WHITE_U16);
    draw_circle(x, y, radius, COLOR_WHITE_U16);

    int16_t pr = (int16_t)(radius * 0.4f);
    int16_t px = x + (int16_t)look_x;
    int16_t py = y;
    fill_circle(px, py, pr, COLOR_BLACK_U16);
    fill_circle(px - pr / 2, py - pr / 2, 3, COLOR_WHITE_U16);
    fill_circle(px + pr / 3, py + pr / 3, 1, COLOR_WHITE_U16);
}

static void draw_mouth_gfx(int16_t x, int16_t y, int mood, uint64_t now_ms) {
    int16_t my = y + 10;
    switch (mood) {
        case FACE_SMILE:
            fill_arc_ring(x, my, 20, 15, 200.0f, 340.0f, COLOR_PINK);
            break;
        case FACE_SURPRISE:
            fill_ellipse(x, my, 8, 12, COLOR_PINK);
            break;
        case FACE_TALK: {
            int mouth_open = (int)((sinf((float)now_ms / 100.0f) + 1.0f) * 10.0f);
            if (mouth_open < 2) mouth_open = 2;
            fill_ellipse(x, my, 10, mouth_open, COLOR_PINK);
            break;
        }
        default:
            fill_arc_ring(x, my, 20, 15, 200.0f, 340.0f, COLOR_PINK);
            break;
    }
}

static void set_face_mood(int mood) {
    if (mood >= 0 && mood < FACE_MOOD_COUNT) {
        current_mood = mood;
    }
}

static void face_tick(uint64_t now_ms) {
    if (!is_blinking && (now_ms - blink_timer) > (uint64_t)rnd_range(2000, 6000)) {
        is_blinking = true;
        blink_timer = now_ms;
    }
    if (is_blinking && (now_ms - blink_timer) > 150) {
        is_blinking = false;
        blink_timer = now_ms;
    }

    float time_scale = (float)now_ms / 1000.0f;
    eye_offset_x = sinf(time_scale) * 10.0f;

    if (g_tap_shake_frames > 0) {
        tap_shake_x = sinf((float)g_tap_shake_frames * 0.8f) * 6.0f;
        tap_shake_y = cosf((float)g_tap_shake_frames * 0.6f) * 4.0f;
        g_tap_shake_frames--;
    } else {
        tap_shake_x = 0.0f;
        tap_shake_y = 0.0f;
    }

    if ((now_ms - mood_timer) > 5000) {
        current_mood = (current_mood + 1) % FACE_MOOD_COUNT;
        mood_timer = now_ms;
    }
}

static void draw_face(uint64_t now_ms) {
    fill_screen(COLOR_BG);

    float scale_y = 1.0f;
    if (is_blinking) {
        uint64_t bp = now_ms - blink_timer;
        if (bp < 75) {
            scale_y = 1.0f - ((float)bp / 75.0f);
        } else {
            scale_y = ((float)(bp - 75) / 75.0f);
        }
    }

    draw_eye_gfx((int16_t)(CENTER_X - EYE_SPACING + tap_shake_x),
                 (int16_t)(EYE_Y + tap_shake_y), EYE_RADIUS, eye_offset_x, scale_y);
    draw_eye_gfx((int16_t)(CENTER_X + EYE_SPACING + tap_shake_x),
                 (int16_t)(EYE_Y + tap_shake_y), EYE_RADIUS, eye_offset_x, scale_y);
    draw_mouth_gfx(CENTER_X, MOUTH_Y, current_mood, now_ms);

    flush_frame();
}

void display_init(void) {
    fill_screen(COLOR_BG);
    flush_frame();
    uint64_t now = (uint64_t)(esp_timer_get_time() / 1000ULL);
    blink_timer = now;
    mood_timer = now;
    ESP_LOGI(TAG, "Display initialized (simple vector face)");
}

void display_set_emotion(emotion_t emotion) {
    switch (emotion) {
        case EMOTION_SURPRISED:
        case EMOTION_CONFUSED:
            set_face_mood(FACE_SURPRISE);
            break;
        case EMOTION_EXCITED:
        case EMOTION_HEART_EYES:
        case EMOTION_ANGRY:
            set_face_mood(FACE_TALK);
            break;
        default:
            set_face_mood(FACE_SMILE);
            break;
    }
}

void display_update(void) {
    uint64_t now_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);
    face_tick(now_ms);
    draw_face(now_ms);
}
