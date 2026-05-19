#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "app_display.h"
#include "app_sensors.h"
#include "app_behavior.h"
#include "app_effects.h"
#include "app_ble.h"
#include "face_api.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "gc9a01.h"

static const char *TAG = "harti";

#define DISPLAY_TASK_STACK 4096
#define DISPLAY_TASK_PRIO  5

static void lvgl_tick_cb(void *arg) {
    lv_tick_inc(1);
}

// ── 显示任务 (60fps) ─────────────────────────────────

static void display_task(void *arg) {
    ESP_LOGI(TAG, "Display task started");
    display_set_emotion(EMOTION_NEUTRAL);

    while (1) {
        display_update();
        effects_update(1.0f / 60.0f);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ── 主入口 ───────────────────────────────────────────

void app_main(void) {
    ESP_LOGI(TAG, "Harti starting...");

    // LVGL init (animation only, no display driver)
    lv_init();

    // 1ms tick timer for LVGL animation heartbeat
    esp_timer_handle_t lvgl_timer;
    esp_timer_create_args_t lvgl_timer_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    esp_timer_create(&lvgl_timer_args, &lvgl_timer);
    esp_timer_start_periodic(lvgl_timer, 1000);  // 1000 us = 1 ms

    // Hardware init
    gc9a01_init();
    display_init();

    // Register effects callback
    face_post_line_cb = effects_apply_line;

    // Start sensor task, get event queue
    QueueHandle_t sensor_queue = sensors_start();

    // Start BLE task (stub)
    ble_start();

    // Start behavior task (consumes sensor events)
    behavior_start(sensor_queue);

    // Start display task (highest priority, 60fps)
    xTaskCreate(display_task, "display", DISPLAY_TASK_STACK,
                NULL, DISPLAY_TASK_PRIO, NULL);

    ESP_LOGI(TAG, "All tasks started");
}
