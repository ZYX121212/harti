#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "app_display.h"
#include "app_sensors.h"
#include "app_behavior.h"
#include "app_effects.h"
#include "app_ble.h"
#include "expressive_eyes.h"
#include "gc9a01.h"

static const char *TAG = "harti";

#define DISPLAY_TASK_STACK 4096
#define DISPLAY_TASK_PRIO  5

// ── 显示任务 (60fps) ─────────────────────────────────

static void display_task(void *arg) {
    ESP_LOGI(TAG, "Display task started");
    display_set_emotion(EMOTION_NEUTRAL);

    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        display_update();
        effects_update(1.0f / 60.0f);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(16));
    }
}

// ── 主入口 ───────────────────────────────────────────

void app_main(void) {
    ESP_LOGI(TAG, "Harti starting...");

    // 硬件初始化
    gc9a01_init();
    display_init();

    // 注册特效叠加回调 (每行渲染后、SPI发送前调用)
    eyes_post_line_cb = effects_apply_line;

    // 启动传感器任务, 获取事件队列
    QueueHandle_t sensor_queue = sensors_start();

    // 启动 BLE 任务 (stub)
    ble_start();

    // 启动行为任务 (消费传感器事件)
    behavior_start(sensor_queue);

    // 启动显示任务 (最高优先级, 保证 60fps)
    xTaskCreate(display_task, "display", DISPLAY_TASK_STACK,
                NULL, DISPLAY_TASK_PRIO, NULL);

    ESP_LOGI(TAG, "All tasks started");
}
