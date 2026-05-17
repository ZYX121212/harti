#include "app_display.h"
#include "expressive_eyes.h"
#include "esp_log.h"
#include "esp_random.h"
#include <math.h>

static const char *TAG = "display";

// 当前状态
static emotion_t current_emotion = EMOTION_NEUTRAL;
static emotion_t previous_emotion = EMOTION_NEUTRAL;
static emotion_t target_emotion = EMOTION_NEUTRAL;
static float transition_t = 1.0f; // 0-1, 1 表示完成

// 微动作状态
static float breath_phase = 0;
static float micro_saccade_phase = 0;
static int blink_state = 0; // 0: 不眨眼, 1: 眨眼中, 2: 恢复中
static float blink_t = 0;
static int frames_until_next_blink = 180; // 约 3 秒 @ 60fps

// 缓动函数: ease out cubic
static float ease_out_cubic(float t)
{
    return 1 - powf(1 - t, 3);
}

// 获取表情对应的预设状态
static const eye_state_t *get_emotion_state(emotion_t e)
{
    switch (e) {
        case EMOTION_HAPPY: return &EYE_STATE_HAPPY;
        case EMOTION_SAD: return &EYE_STATE_SAD;
        case EMOTION_SURPRISED: return &EYE_STATE_SURPRISED;
        case EMOTION_SLEEPY: return &EYE_STATE_SLEEPY;
        case EMOTION_ANGRY: return &EYE_STATE_ANGRY;
        case EMOTION_BORED: return &EYE_STATE_BORED;
        case EMOTION_EXCITED: return &EYE_STATE_EXCITED;
        case EMOTION_CONFUSED: return &EYE_STATE_CONFUSED;
        case EMOTION_CONTENT:  return &EYE_STATE_CONTENT;
        case EMOTION_COLD:     return &EYE_STATE_COLD;
        case EMOTION_WARM:     return &EYE_STATE_WARM;
        case EMOTION_HEART_EYES: return &EYE_STATE_HEART_EYES;
        default: return &EYE_STATE_NEUTRAL;
    }
}

void display_init(void)
{
    eyes_init();
    ESP_LOGI(TAG, "Display initialized");
}

void display_set_emotion(emotion_t emotion)
{
    if (emotion >= EMOTION_COUNT) return;
    if (emotion == target_emotion) return;

    ESP_LOGI(TAG, "Set emotion: %d", emotion);
    previous_emotion = current_emotion;
    target_emotion = emotion;
    transition_t = 0;
}

void display_set_color_scheme(color_scheme_t scheme)
{
    if (scheme >= COLOR_SCHEME_COUNT) return;
    eyes_set_color_scheme(scheme);
    ESP_LOGI(TAG, "Set color scheme: %d", scheme);
}


void display_update(void)
{
    // 1. 表情过渡
    if (transition_t < 1.0f) {
        transition_t += 0.04f;
        if (transition_t > 1.0f) transition_t = 1.0f;
        if (transition_t >= 1.0f) {
            current_emotion = target_emotion;
        }
    }

    // 2. 随机眨眼
    if (blink_state == 0) {
        frames_until_next_blink--;
        if (frames_until_next_blink <= 0) {
            blink_state = 1;
            blink_t = 0;
        }
    } else if (blink_state == 1) {
        blink_t += 0.25f;
        if (blink_t >= 1.0f) {
            blink_t = 1.0f;
            blink_state = 2;
        }
    } else {
        blink_t -= 0.15f;
        if (blink_t <= 0) {
            blink_t = 0;
            blink_state = 0;
            frames_until_next_blink = 120 + (esp_random() % 240); // 2-6 秒
        }
    }

    // 3. 呼吸动画 (眼睛轻微缩放)
    breath_phase += 0.03f;
    float breath = sinf(breath_phase) * 0.03f + 1.0f;

    // 4. 眼球微动 (micro-saccades)
    micro_saccade_phase += 0.08f;
    float micro_x = sinf(micro_saccade_phase * 1.3f) * 0.05f;
    float micro_y = cosf(micro_saccade_phase * 0.9f) * 0.05f;

    // 获取基础状态并混合
    const eye_state_t *base = get_emotion_state(current_emotion);
    eye_state_t state = *base;

    // 应用呼吸效果
    state.left_lid_open *= breath;
    state.right_lid_open *= breath;
    if (state.left_lid_open > 1) state.left_lid_open = 1;
    if (state.right_lid_open > 1) state.right_lid_open = 1;

    // 应用眼球微动
    state.pupil_x += micro_x;
    state.pupil_y += micro_y;

    // 应用眨眼
    if (blink_state != 0) {
        float b = 1.0f - blink_t;
        if (b < 0) b = 0;
        state.left_lid_open *= b;
        state.right_lid_open *= b;
    }

    // 如果在过渡中，和之前的表情混合
    if (transition_t < 1.0f) {
        const eye_state_t *prev = get_emotion_state(previous_emotion);
        eye_state_t blended;
        float t = ease_out_cubic(transition_t);
        eyes_blend(prev, &state, t, &blended);
        eyes_set_state(&blended);
    } else {
        eyes_set_state(&state);
    }

    // 渲染
    eyes_render_frame();
}
