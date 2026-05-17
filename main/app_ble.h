#ifndef APP_BLE_H
#define APP_BLE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "app_sensors.h"

#ifdef __cplusplus
extern "C" {
#endif

// 启动 BLE 任务, 返回 BLE 事件队列
// (后续 behavior_task 从此队列读取碰一碰事件)
QueueHandle_t ble_start(void);

#ifdef __cplusplus
}
#endif

#endif
