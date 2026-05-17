#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "expressive_eyes.h"

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
    EMOTION_CONFUSED,
    EMOTION_CONTENT,
    EMOTION_COLD,
    EMOTION_WARM,
    EMOTION_HEART_EYES,
    EMOTION_COUNT
} emotion_t;

void display_init(void);
void display_set_emotion(emotion_t emotion);
void display_set_color_scheme(color_scheme_t scheme);
void display_update(void); // 每帧调用，处理动画

#ifdef __cplusplus
}
#endif

#endif
