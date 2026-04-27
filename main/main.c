#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

#include "app_display.h"
#include "../components/gc9a01/gc9a01.h"

static const char *TAG = "harti";

void app_main(void)
{
    ESP_LOGI(TAG, "Harti starting...");

    // 初始化显示
    gc9a01_init();
    display_init();

    ESP_LOGI(TAG, "Entering main loop");

    // 表情演示序列
    emotion_t sequence[] = {
        EMOTION_NEUTRAL,
        EMOTION_HAPPY,
        EMOTION_NEUTRAL,
        EMOTION_SURPRISED,
        EMOTION_NEUTRAL,
        EMOTION_SAD,
        EMOTION_NEUTRAL,
        EMOTION_ANGRY,
        EMOTION_NEUTRAL,
        EMOTION_EXCITED,
        EMOTION_BORED,
        EMOTION_SLEEPY,
        EMOTION_NEUTRAL,
    };
    int seq_len = sizeof(sequence) / sizeof(sequence[0]);
    int seq_idx = 0;

    int frame_count = 0;

    while (1) {
        display_update();
        frame_count++;

        // 每 2 秒切换一次表情
        if (frame_count % 120 == 0) {
            seq_idx = (seq_idx + 1) % seq_len;
            display_set_emotion(sequence[seq_idx]);
        }

        vTaskDelay(pdMS_TO_TICKS(16)); // ~60fps
    }
}
