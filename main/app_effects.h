#ifndef APP_EFFECTS_H
#define APP_EFFECTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EFFECT_NONE = 0,
    EFFECT_STAR,           // 单颗小星星闪一下 (点头之交)
    EFFECT_HEART_PARTICLE, // 2-3颗心形粒子升起 (朋友)
    EFFECT_RAINBOW,        // 彩虹弧线 (好友)
    EFFECT_GOLDEN,         // 金光扩散 (挚友)
    EFFECT_COUNT
} effect_type_t;

// 触发特效 (高级可中断低级)
void effects_trigger(effect_type_t type);

// 每帧更新, dt为帧间隔(秒)
void effects_update(float dt);

// 每扫描行调用, 在渲染眼睛后叠加特效
// line_buf: 该行像素buffer, screen_w: 屏幕宽度(固定240)
void effects_apply_line(int y, uint16_t *line_buf, int screen_w);

#ifdef __cplusplus
}
#endif

#endif
