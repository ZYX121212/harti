#ifndef APP_BEHAVIOR_H
#define APP_BEHAVIOR_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// 启动行为任务
// sensor_queue: 来自 sensors_start() 的事件队列
void behavior_start(QueueHandle_t sensor_queue);

#ifdef __cplusplus
}
#endif

#endif
