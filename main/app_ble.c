#include "app_ble.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "ble";

#define BLE_TASK_STACK 3072
#define BLE_TASK_PRIO  1

static QueueHandle_t ble_queue;

static void ble_task(void *arg) {
    ESP_LOGI(TAG, "BLE task started (stub mode)");

    // TODO: BLE 广播 + 扫描 + 配对 + 数据交换
    // 当前仅心跳日志, 后续迭代实现完整功能
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGD(TAG, "BLE stub heartbeat");
    }
}

QueueHandle_t ble_start(void) {
    ble_queue = xQueueCreate(5, sizeof(sensor_event_msg_t));
    xTaskCreate(ble_task, "ble", BLE_TASK_STACK, NULL, BLE_TASK_PRIO, NULL);
    return ble_queue;
}
