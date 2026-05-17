#ifndef EXPRESSIVE_EYES_H
#define EXPRESSIVE_EYES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // 眼睛位置 (屏幕中心为原点)
    float eye_offset_x;
    float eye_offset_y;
    float eye_separation;

    // 眼睑状态 (0.0 闭合 - 1.0 睁开)
    float left_lid_open;
    float right_lid_open;

    // 眼球
    float pupil_x;        // 瞳孔偏移 X (-1.0 到 1.0)
    float pupil_y;        // 瞳孔偏移 Y (-1.0 到 1.0)
    float pupil_scale;    // 瞳孔大小比例

    // 表情变形
    float curve_up;       // 眼角上弯程度 (开心)
    float curve_down;     // 眼角下弯程度 (难过)

    // 装饰
    float blush_level;    // 腮红 (0.0-1.0)
    float tear_level;     // 眼泪 (0.0-1.0)
    float star_level;     // 星星眼 (0.0-1.0)
    bool heart_mode;      // true = 虹膜绘制为心形
} eye_state_t;

typedef enum {
    COLOR_SCHEME_WHITE = 0,
    COLOR_SCHEME_BLACK,
    COLOR_SCHEME_COUNT
} color_scheme_t;

void eyes_init(void);
void eyes_set_state(const eye_state_t *state);
void eyes_render_frame(void);
void eyes_blend(const eye_state_t *a, const eye_state_t *b, float t, eye_state_t *out);
void eyes_set_color_scheme(color_scheme_t scheme);

// 预设状态
extern const eye_state_t EYE_STATE_NEUTRAL;
extern const eye_state_t EYE_STATE_HAPPY;
extern const eye_state_t EYE_STATE_SAD;
extern const eye_state_t EYE_STATE_SURPRISED;
extern const eye_state_t EYE_STATE_SLEEPY;
extern const eye_state_t EYE_STATE_ANGRY;
extern const eye_state_t EYE_STATE_BORED;
extern const eye_state_t EYE_STATE_EXCITED;
extern const eye_state_t EYE_STATE_HEART_EYES;
extern const eye_state_t EYE_STATE_CONFUSED;
extern const eye_state_t EYE_STATE_CONTENT;
extern const eye_state_t EYE_STATE_COLD;
extern const eye_state_t EYE_STATE_WARM;

#ifdef __cplusplus
}
#endif

#endif
