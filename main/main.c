#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "harti_config.h"
#include "app_input.h"
#include "app_sensors.h"
#include "app_behavior.h"
#include "app_ble.h"
#include "face_api.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "gc9a01.h"

// Tilt angles from sensor task (updated at 100Hz, read at 20fps)
extern float g_tilt_pitch;
extern float g_tilt_roll;

static const char *TAG = "harti";

static void lvgl_tick_cb(void *arg) {
    lv_tick_inc(1);
}

// ── Display task (20fps) ──────────────────────────────────

static void display_task(void *arg) {
    ESP_LOGI(TAG, "Display task started");
    face_set_expression(EMOTION_NEUTRAL);

    while (1) {
        face_set_tilt(g_tilt_pitch, g_tilt_roll);
        face_animator_tick();
        face_render_frame();
        vTaskDelay(pdMS_TO_TICKS(DISPLAY_FRAME_MS));
    }
}

// ── Main entry ────────────────────────────────────────────

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
    esp_timer_start_periodic(lvgl_timer, 1000);

    // Hardware init
    gc9a01_init();
    face_init();
    face_render_frame();

    // Start display task first (highest priority, screen visible immediately)
    xTaskCreate(display_task, "display", DISPLAY_TASK_STACK,
                NULL, DISPLAY_TASK_PRIO, NULL);

    // Sensors / input / BLE / behavior delayed start
    QueueHandle_t sensor_queue = sensors_start();
    input_start();
    ble_start();
    behavior_start(sensor_queue);

    ESP_LOGI(TAG, "All tasks started");
}
