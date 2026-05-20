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
static int wink_type = 0;         // 0: both, 1: left only, 2: right only

// Brow micro-movement
static float brow_phase_l = 0;
static float brow_phase_r = 0;

// Idle mouth movement
static int mouth_move_state = 0;  // 0: idle, 1: opening, 2: closing
static float mouth_move_t = 0;
static int frames_until_mouth = 180;

// Saccade randomization
static float saccade_amplitude = 0.05f;
static int saccade_timer = 120;
static float sac_phase = 0;

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
    // 1. Blink logic with wink
    if (blink_state == 0) {
        frames_until_next_blink--;
        if (frames_until_next_blink <= 0) {
            blink_state = 1;
            blink_t = 0;
            // C1: 1 in 5 chance of wink (single eye)
            wink_type = (esp_random() % 5 == 0) ? (1 + (esp_random() % 2)) : 0;
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
            wink_type = 0;
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

        // C1: Winks — only close the target eye(s)
        float left_close = (wink_type == 2) ? 0 : lid_close;
        float right_close = (wink_type == 1) ? 0 : lid_close;

        // Left eye
        ep = st->eye[0];
        ep.top_lid_mid.dy = left_close * 0.7f;
        face_set_component_instant(COMPONENT_EYE_LEFT, &ep);

        // Right eye
        ep = st->eye[1];
        ep.top_lid_mid.dy = right_close * 0.7f;
        face_set_component_instant(COMPONENT_EYE_RIGHT, &ep);
    }

    // 4. Micro-saccades (small iris movements) with randomization
    sac_phase += 0.08f;
    // C4: Variable amplitude + occasional large jumps
    saccade_timer--;
    if (saccade_timer <= 0) {
        saccade_amplitude = 0.02f + (float)(esp_random() % 60) * 0.001f;
        saccade_timer = 90 + (esp_random() % 180);
    }
    float micro_x = sinf(sac_phase * 1.3f) * saccade_amplitude;
    float micro_y = cosf(sac_phase * 0.9f) * saccade_amplitude;
    // Occasional large saccade (1 in 300 frames)
    if (esp_random() % 300 == 0) {
        micro_x += (float)((int)(esp_random() % 100) - 50) * 0.002f;
        micro_y += (float)((int)(esp_random() % 100) - 50) * 0.002f;
    }

    const face_state_t *st = animator_get_state();
    eye_params_t ep_l = st->eye[0];
    eye_params_t ep_r = st->eye[1];
    ep_l.iris_center.dx += micro_x;
    ep_l.iris_center.dy += micro_y;
    ep_r.iris_center.dx += micro_x;
    ep_r.iris_center.dy += micro_y;
    face_set_component_instant(COMPONENT_EYE_LEFT, &ep_l);
    face_set_component_instant(COMPONENT_EYE_RIGHT, &ep_r);

    // C2: Brow micro-movement (independent left/right twitches)
    brow_phase_l += 0.05f;
    brow_phase_r += 0.07f;
    float brow_micro_l = sinf(brow_phase_l) * 0.012f;
    float brow_micro_r = sinf(brow_phase_r) * 0.012f;
    brow_params_t bp_l = st->brow[0];
    brow_params_t bp_r = st->brow[1];
    // Only apply if not in the middle of an expression transition
    // (we apply these as instant micro-adjustments on top of current state)
    bp_l.arch.dy += brow_micro_l;
    bp_r.arch.dy += brow_micro_r;
    face_set_component_instant(COMPONENT_BROW_LEFT, &bp_l);
    face_set_component_instant(COMPONENT_BROW_RIGHT, &bp_r);

    // C3: Idle mouth micro-movement
    frames_until_mouth--;
    if (frames_until_mouth <= 0 && mouth_move_state == 0) {
        mouth_move_state = 1;  // start opening
        mouth_move_t = 0;
    }
    if (mouth_move_state == 1) {
        mouth_move_t += 0.05f;
        if (mouth_move_t >= 1.0f) {
            mouth_move_t = 1.0f;
            mouth_move_state = 2;  // start closing
        }
    } else if (mouth_move_state == 2) {
        mouth_move_t -= 0.03f;
        if (mouth_move_t <= 0) {
            mouth_move_t = 0;
            mouth_move_state = 0;  // back to idle
            frames_until_mouth = 180 + (esp_random() % 300); // 3-8 seconds
        }
    }
    if (mouth_move_state != 0) {
        mouth_params_t mp = st->mouth;
        mp.openness += mouth_move_t * 0.08f;
        mp.lower_lip_mid.dy += mouth_move_t * 0.05f;
        face_set_component_instant(COMPONENT_MOUTH, &mp);
    }

    // 5. LVGL tick + render
    face_animator_tick();
    face_render_frame();
}
