#include "app_behavior.h"
#include "app_sensors.h"
#include "app_display.h"
#include "app_effects.h"
#include "esp_log.h"
#include "freertos/task.h"

extern volatile int g_tap_shake_frames;

static const char *TAG = "behavior";

#define BEHAVIOR_TASK_STACK 2048
#define BEHAVIOR_TASK_PRIO  3

#define IDLE_BORED_SEC  10   // 10s 无互动 → BORED
#define IDLE_SLEEPY_SEC 30   // 30s 无互动 → SLEEPY
#define SURPRISE_TIMEOUT_SEC 3   // SURPRISED 后 3s 无互动 → CONFUSED
#define CONFUSED_TIMEOUT_SEC 5   // CONFUSED 后 5s 无互动 → IDLE

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
static TickType_t state_start_ticks; // 进入当前状态的时间

// 辅助: 设置情绪并更新状态
static void transition_to(behavior_state_t state, emotion_t emo) {
    current_state = state;
    state_start_ticks = xTaskGetTickCount();
    display_set_emotion(emo);
}

// ── 空闲检查 (每秒调用) ─────────────────────────────

static void check_idle(void) {
    TickType_t now = xTaskGetTickCount();
    int elapsed_s = (now - last_event_ticks) * portTICK_PERIOD_MS / 1000;
    int state_elapsed_s = (now - state_start_ticks) * portTICK_PERIOD_MS / 1000;

    // SURPRISED → CONFUSED 过渡 (摇晃/翻转后 3s 无新互动)
    if (current_state == STATE_SURPRISED && state_elapsed_s >= SURPRISE_TIMEOUT_SEC) {
        transition_to(STATE_CONFUSED, EMOTION_CONFUSED);
        ESP_LOGI(TAG, "SURPRISED → CONFUSED (%ds)", state_elapsed_s);
        return;
    }

    // CONFUSED → IDLE 过渡 (困惑 5s 后恢复)
    if (current_state == STATE_CONFUSED && state_elapsed_s >= CONFUSED_TIMEOUT_SEC) {
        transition_to(STATE_IDLE, EMOTION_NEUTRAL);
        ESP_LOGI(TAG, "CONFUSED → IDLE (%ds)", state_elapsed_s);
        return;
    }

    if (elapsed_s >= IDLE_SLEEPY_SEC && current_state != STATE_SLEEPY) {
        transition_to(STATE_SLEEPY, EMOTION_SLEEPY);
        ESP_LOGI(TAG, "→ SLEEPY (%ds)", elapsed_s);
    } else if (elapsed_s >= IDLE_BORED_SEC && current_state == STATE_IDLE) {
        transition_to(STATE_BORED, EMOTION_BORED);
        ESP_LOGI(TAG, "IDLE → BORED (%ds)", elapsed_s);
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

    case EVT_SHAKE: {
        // 方向区分: 0=全向→EXCITED, 1=水平→HAPPY, 2=垂直→SURPRISED
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        if (msg->value >= 1.5f) {       // 垂直摇
            transition_to(STATE_SURPRISED, EMOTION_SURPRISED);
        } else if (msg->value >= 0.5f) { // 水平摇
            transition_to(STATE_HAPPY, EMOTION_HAPPY);
        } else {                         // 全向摇
            transition_to(STATE_SURPRISED, EMOTION_EXCITED);
        }
        break;
    }

    case EVT_TAP:
        g_tap_shake_frames = 10; // ~200ms 眼睛抖动
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        if (msg->value >= 2.0f) { // 连拍两次 → 委屈
            transition_to(STATE_SAD, EMOTION_SAD);
        }
        break;

    case EVT_FLIP:
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        transition_to(STATE_SURPRISED, EMOTION_SURPRISED);
        break;

    case EVT_TWIST:
        // 快速旋转 → 惊喜 + 星星特效
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        transition_to(STATE_SURPRISED, EMOTION_SURPRISED);
        effects_trigger(EFFECT_STAR);
        ESP_LOGI(TAG, "TWIST → SURPRISED");
        break;

    case EVT_TILT:
        // 倾斜方向: 0前/1左/2右→困惑, 3后→惊讶
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        if (msg->value >= 2.5f) { // 后倾
            transition_to(STATE_SURPRISED, EMOTION_SURPRISED);
        } else { // 前/左/右倾
            transition_to(STATE_CONFUSED, EMOTION_CONFUSED);
        }
        ESP_LOGI(TAG, "TILT dir=%.0f", msg->value);
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
    state_start_ticks = last_event_ticks;

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
