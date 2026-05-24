#include "app_ble.h"
#include "harti_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ble";

static void ble_task(void *arg) {
    ESP_LOGI(TAG, "BLE task started (stub mode)");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGD(TAG, "BLE stub heartbeat");
    }
}

void ble_start(void) {
    xTaskCreate(ble_task, "ble", BLE_TASK_STACK, NULL, BLE_TASK_PRIO, NULL);
}
