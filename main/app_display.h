#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EMOTION_NEUTRAL = 0,
    EMOTION_HAPPY,
    EMOTION_SAD,
    EMOTION_SURPRISED,
    EMOTION_SLEEPY,
    EMOTION_ANGRY,
    EMOTION_BORED,
    EMOTION_EXCITED,
    EMOTION_COUNT
} emotion_t;

void display_init(void);
void display_set_emotion(emotion_t emotion);
void display_update(void); // 每帧调用，处理动画

#ifdef __cplusplus
}
#endif

#endif
