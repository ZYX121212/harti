#include "app_behavior.h"
#include "app_sensors.h"
#include "harti_config.h"
#include "face_api.h"
#include "face_temperament.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "behavior";

typedef enum {
    STATE_IDLE,
    STATE_HAPPY,
    STATE_CONTENT,
    STATE_SURPRISED,
    STATE_THINKING,
    STATE_CONFUSED,
    STATE_SAD,
    STATE_WARM,
    STATE_COLD,
    STATE_BORED,
    STATE_SLEEPY,
} behavior_state_t;

static behavior_state_t current_state = STATE_IDLE;
static TickType_t last_event_ticks;
static TickType_t state_start_ticks;
static expression_id_t prev_expression = 0; /* EMOTION_NEUTRAL */

// Helper: set emotion and update state
static void transition_to(behavior_state_t state, emotion_t emo) {
    current_state = state;
    state_start_ticks = xTaskGetTickCount();
    face_set_expression((expression_id_t)emo);
    face_temperament_notify_expression_change(prev_expression, (expression_id_t)emo);
    prev_expression = (expression_id_t)emo;
    if (emo == EMOTION_NEUTRAL) {
        face_prop_clear(300);
    }

    /* Expression-linked props — show on enter, hide on leave */
    switch (prev_expression) {
    case EMOTION_HAPPY:   face_prop_hide(PROP_MUSIC_NOTE, 100);   break;
    case EMOTION_CONTENT: face_prop_hide(PROP_TEACUP_STEAM, 100); break;
    case EMOTION_COLD:    face_prop_hide(PROP_SUNGLASSES, 100);   break;
    default: break;
    }
    switch (emo) {
    case EMOTION_HAPPY:   face_prop_show(PROP_MUSIC_NOTE, 0.7f, 0.5f, 0);    break;
    case EMOTION_CONTENT: face_prop_show(PROP_TEACUP_STEAM, 0.4f, 0.6f, 0);  break;
    case EMOTION_COLD:    face_prop_show(PROP_SUNGLASSES, 0.0f, 0.0f, 0);    break;
    default: break;
    }
}

// ── Idle check (called every second) ───────────────────────

static void check_idle(void) {
    TickType_t now = xTaskGetTickCount();
    int elapsed_s = (now - last_event_ticks) * portTICK_PERIOD_MS / 1000;
    int state_elapsed_s = (now - state_start_ticks) * portTICK_PERIOD_MS / 1000;

    // SURPRISED → THINKING
    if (current_state == STATE_SURPRISED && state_elapsed_s >= SURPRISE_TIMEOUT_SEC) {
        transition_to(STATE_THINKING, EMOTION_THINKING);
        ESP_LOGI(TAG, "SURPRISED → THINKING (%ds)", state_elapsed_s);
        return;
    }

    // HAPPY → CONTENT
    if (current_state == STATE_HAPPY && state_elapsed_s >= HAPPY_TIMEOUT_SEC) {
        transition_to(STATE_CONTENT, EMOTION_CONTENT);
        ESP_LOGI(TAG, "HAPPY → CONTENT (%ds)", state_elapsed_s);
        return;
    }

    // CONTENT → IDLE
    if (current_state == STATE_CONTENT && state_elapsed_s >= CONTENT_TIMEOUT_SEC) {
        transition_to(STATE_IDLE, EMOTION_NEUTRAL);
        ESP_LOGI(TAG, "CONTENT → IDLE (%ds)", state_elapsed_s);
        return;
    }

    // CONFUSED → IDLE
    if (current_state == STATE_CONFUSED && state_elapsed_s >= CONFUSED_TIMEOUT_SEC) {
        transition_to(STATE_IDLE, EMOTION_NEUTRAL);
        ESP_LOGI(TAG, "CONFUSED → IDLE (%ds)", state_elapsed_s);
        return;
    }

    // THINKING → IDLE
    if (current_state == STATE_THINKING && state_elapsed_s >= THINKING_TIMEOUT_SEC) {
        transition_to(STATE_IDLE, EMOTION_NEUTRAL);
        ESP_LOGI(TAG, "THINKING → IDLE (%ds)", state_elapsed_s);
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

// ── Sensor event handler ───────────────────────────────────

static void on_event(const sensor_event_msg_t *msg) {
    last_event_ticks = xTaskGetTickCount();

    // Any interaction wakes from BORED/SLEEPY
    bool was_idle = (current_state == STATE_BORED || current_state == STATE_SLEEPY);

    switch (msg->type) {

    case EVT_SHAKE: {
        face_prop_show(PROP_STAR_SMALL, 11.0f, 0.55f, 180);
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        if (msg->value >= 1.5f) {
            transition_to(STATE_SURPRISED, EMOTION_SURPRISED);
        } else if (msg->value >= 0.5f) {
            transition_to(STATE_HAPPY, EMOTION_HAPPY);
        } else {
            transition_to(STATE_SURPRISED, EMOTION_EXCITED);
        }
        break;
    }

    case EVT_TAP:
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        if (msg->value >= 2.0f) {
            transition_to(STATE_SAD, EMOTION_SAD);
        }
        break;

    case EVT_FLIP:
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        transition_to(STATE_SURPRISED, EMOTION_SURPRISED);
        break;

    case EVT_TWIST:
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        transition_to(STATE_SURPRISED, EMOTION_SURPRISED);
        ESP_LOGI(TAG, "TWIST → SURPRISED");
        break;

    case EVT_TILT:
        if (was_idle) { transition_to(STATE_IDLE, EMOTION_NEUTRAL); break; }
        if (msg->value >= 2.5f) {
            transition_to(STATE_SURPRISED, EMOTION_SURPRISED);
        } else {
            transition_to(STATE_CONFUSED, EMOTION_CONFUSED);
        }
        ESP_LOGI(TAG, "TILT dir=%.0f", msg->value);
        break;

    case EVT_WARM_UP:
        face_prop_show(PROP_TEACUP, 9.8f, 0.6f, 250);
        if (was_idle) { transition_to(STATE_WARM, EMOTION_WARM); break; }
        transition_to(STATE_WARM, EMOTION_WARM);
        break;

    case EVT_COLD_DOWN:
        face_prop_hide(PROP_TEACUP, 200);
        transition_to(STATE_COLD, EMOTION_COLD);
        break;

    case EVT_BLE_MEET:
        ESP_LOGI(TAG, "BLE MEET");
        break;

    case EVT_BLE_FRIEND:
        ESP_LOGI(TAG, "BLE FRIEND level=%.0f", msg->value);
        face_prop_show(PROP_HEART, 11.5f, 0.5f, 200);
        break;

    default: break;
    }
}

// ── Behavior task entry ────────────────────────────────────

static void behavior_task(void *arg) {
    QueueHandle_t queue = (QueueHandle_t)arg;
    ESP_LOGI(TAG, "Behavior task started");
    last_event_ticks = xTaskGetTickCount();
    state_start_ticks = last_event_ticks;

    /* Wait for initial NEUTRAL animation to finish (100ms delay + 150ms dur = 250ms),
       then show finger heart prop on the right cheek. */
    vTaskDelay(pdMS_TO_TICKS(350));
    face_prop_show(PROP_FINGER_HEART, 0.65f, 0.55f, 30000);

    sensor_event_msg_t msg;
    while (1) {
        // Block on events, 1s timeout for idle checking
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
