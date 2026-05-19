#include "app_display.h"
#include "face_api.h"
#include "esp_log.h"
#include "esp_random.h"
#include <math.h>

static const char *TAG = "display";

// Micro-animation state
static float breath_phase = 0;
static int blink_state = 0;       // 0: waiting, 1: closing, 2: opening
static float blink_t = 0;
static int frames_until_next_blink = 180;

static float ease_in_out(float t) {
    return t < 0.5f ? 2 * t * t : 1 - powf(-2 * t + 2, 2) / 2;
}

void display_init(void) {
    face_init();
    ESP_LOGI(TAG, "Display initialized (face_system)");
}

void display_set_emotion(emotion_t emotion) {
    if (emotion >= EMOTION_COUNT) return;
    face_set_expression((expression_id_t)emotion);
}

void display_update(void) {
    // 1. Blink logic
    if (blink_state == 0) {
        frames_until_next_blink--;
        if (frames_until_next_blink <= 0) {
            blink_state = 1;
            blink_t = 0;
        }
    } else if (blink_state == 1) {
        blink_t += 0.25f;
        if (blink_t >= 1.0f) {
            blink_t = 1.0f;
            blink_state = 2;
        }
    } else {
        blink_t -= 0.15f;
        if (blink_t <= 0) {
            blink_t = 0;
            blink_state = 0;
            frames_until_next_blink = 120 + (esp_random() % 240);
        }
    }

    // 2. Breath micro-animation (face roundness oscillation)
    breath_phase += 0.03f;
    float breath = sinf(breath_phase) * 0.03f + 1.0f;
    face_params_t fp = {.roundness = 0.5f * breath};
    face_set_component_instant(COMPONENT_FACE, &fp);

    // 3. Blink: close lids by adjusting eye top_lid_mid.dy
    if (blink_state != 0) {
        eye_params_t ep;
        const face_state_t *st = animator_get_state();
        float lid_close = ease_in_out(blink_t);

        // Left eye
        ep = st->eye[0];
        ep.top_lid_mid.dy = lid_close * 0.7f;
        face_set_component_instant(COMPONENT_EYE_LEFT, &ep);

        // Right eye
        ep = st->eye[1];
        ep.top_lid_mid.dy = lid_close * 0.7f;
        face_set_component_instant(COMPONENT_EYE_RIGHT, &ep);
    }

    // 4. Micro-saccades (small iris movements)
    static float sac_phase = 0;
    sac_phase += 0.08f;
    float micro_x = sinf(sac_phase * 1.3f) * 0.05f;
    float micro_y = cosf(sac_phase * 0.9f) * 0.05f;

    const face_state_t *st = animator_get_state();
    eye_params_t ep_l = st->eye[0];
    eye_params_t ep_r = st->eye[1];
    ep_l.iris_center.dx += micro_x;
    ep_l.iris_center.dy += micro_y;
    ep_r.iris_center.dx += micro_x;
    ep_r.iris_center.dy += micro_y;
    face_set_component_instant(COMPONENT_EYE_LEFT, &ep_l);
    face_set_component_instant(COMPONENT_EYE_RIGHT, &ep_r);

    // 5. LVGL tick + render
    face_animator_tick();
    face_render_frame();
}
