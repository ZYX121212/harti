#include "app_behavior.h"
#include "app_sensors.h"
#include "app_display.h"
#include "app_effects.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "behavior";

#define BEHAVIOR_TASK_STACK 2048
#define BEHAVIOR_TASK_PRIO  3

#define IDLE_BORED_SEC  10   // 10s 无互动 → BORED
#define IDLE_SLEEPY_SEC 30   // 30s 无互动 → SLEEPY

typedef enum {
    STATE_IDLE,
    STATE_HAPPY,
    STATE_CONTENT,
    STATE_SURPRISED,
    STATE_CONFUSED,
    STATE_SAD,
    STATE_WARM,
    STATE_COLD,
    STATE_BORED,
    STATE_SLEEPY,
} behavior_state_t;

static behavior_state_t current_state = STATE_IDLE;
static TickType_t last_event_ticks;

// 辅助: 设置情绪并更新状态
static void transition_to(behavior_state_t state, emotion_t emo) {
    current_state = state;
    display_set_emotion(emo);
}

// ── 空闲检查 (每秒调用) ─────────────────────────────

static void check_idle(void) {
    if (current_state == STATE_IDLE) {
        TickType_t now = xTaskGetTickCount();
        int elapsed_s = (now - last_event_ticks) * portTICK_PERIOD_MS / 1000;

        if (elapsed_s >= IDLE_SLEEPY_SEC && current_state == STATE_IDLE) {
            transition_to(STATE_SLEEPY, EMOTION_SLEEPY);
            ESP_LOGI(TAG, "IDLE → SLEEPY (%ds)", elapsed_s);
        } else if (elapsed_s >= IDLE_BORED_SEC && current_state == STATE_IDLE) {
            transition_to(STATE_BORED, EMOTION_BORED);
            ESP_LOGI(TAG, "IDLE → BORED (%ds)", elapsed_s);
        }
    }
}

// ── 传感器事件处理 ──────────────────────────────────

static void on_event(const sensor_event_msg_t *msg) {
    last_event_ticks = xTaskGetTickCount();

    // 任何交互从 BORED/SLEEPY 唤醒
    bool was_idle = (current_state == STATE_BORED || current_state == STATE_SLEEPY);

    switch (msg->type) {

    case EVT_TOUCH_HEAD:
        if (was_idle) {
            transition_to(STATE_IDLE, EMOTION_NEUTRAL);
            break;
        }
        if (current_state != STATE_CONTENT && current_state != STATE_HAPPY) {
            transition_to(STATE_HAPPY, EMOTION_HAPPY);
        }
        break;

    case EVT_TOUCH_RELEASE:
        if (current_state == STATE_HAPPY) {
            // 持续摸头超过一段时间 → CONTENT (简化: 每次都检查)
            transition_to(STATE_CONTENT, EMOTION_CONTENT);
        } else if (current_state == STATE_CONTENT) {
            transition_to(STATE_IDLE, EMOTION_NEUTRAL);
        }
        break;

    case EVT_SHAKE:
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        transition_to(STATE_SURPRISED, EMOTION_SURPRISED);
        break;

    case EVT_TAP:
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        if (msg->value >= 2.0f) { // 连拍两次 → 委屈
            transition_to(STATE_SAD, EMOTION_SAD);
        }
        break;

    case EVT_FLIP:
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        transition_to(STATE_SURPRISED, EMOTION_SURPRISED);
        break;

    case EVT_WARM_UP:
        if (was_idle) { transition_to(STATE_WARM, EMOTION_WARM); break; }
        transition_to(STATE_WARM, EMOTION_WARM);
        break;

    case EVT_COLD_DOWN:
        transition_to(STATE_COLD, EMOTION_COLD);
        break;

    case EVT_BLE_MEET:
        effects_trigger(EFFECT_STAR);
        break;

    case EVT_BLE_FRIEND:
        // value 字段携带关系等级 (0-4)
        if (msg->value >= 4.0f)           effects_trigger(EFFECT_GOLDEN);
        else if (msg->value >= 3.0f)      effects_trigger(EFFECT_RAINBOW);
        else if (msg->value >= 2.0f)      effects_trigger(EFFECT_HEART_PARTICLE);
        else                              effects_trigger(EFFECT_STAR);
        break;

    default: break;
    }
}

// ── 行为任务入口 ─────────────────────────────────────

static void behavior_task(void *arg) {
    QueueHandle_t queue = (QueueHandle_t)arg;
    ESP_LOGI(TAG, "Behavior task started");
    last_event_ticks = xTaskGetTickCount();

    sensor_event_msg_t msg;
    while (1) {
        // 阻塞等待事件, 超时 1 秒用于空闲计时
        if (xQueueReceive(queue, &msg, pdMS_TO_TICKS(1000))) {
            on_event(&msg);
        } else {
            check_idle();
        }
    }
}

void behavior_start(QueueHandle_t sensor_queue) {
    xTaskCreate(behavior_task, "behavior", BEHAVIOR_TASK_STACK,
                (void *)sensor_queue, BEHAVIOR_TASK_PRIO, NULL);
}
