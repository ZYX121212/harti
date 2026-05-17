#ifndef APP_SENSORS_H
#define APP_SENSORS_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EVT_NONE = 0,
    EVT_TOUCH_HEAD,
    EVT_TOUCH_RELEASE,
    EVT_SHAKE,
    EVT_TAP,
    EVT_FLIP,
    EVT_WARM_UP,
    EVT_COLD_DOWN,
    EVT_TEMP_STABLE,
    EVT_BLE_MEET,
    EVT_BLE_FRIEND,
} sensor_event_t;

typedef struct {
    sensor_event_t type;
    float value; // 温度数值 / 加速度幅值 / 关系等级
} sensor_event_msg_t;

// 启动传感器任务, 返回事件队列句柄 (behavior_task 从此队列读取)
QueueHandle_t sensors_start(void);

#ifdef __cplusplus
}
#endif

#endif
