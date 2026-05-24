#include "app_display.h"
#include "face_api.h"
#include "esp_log.h"
#include "esp_random.h"
#include <math.h>

static const char *TAG = "display";

// 来自 app_sensors.c 的全局倾斜角度
extern float g_tilt_pitch;
extern float g_tilt_roll;

// Micro-animation state
static float breath_phase = 0;
static float face_roundness_base = 0.66f;
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

// Tap shake (set by behavior task via app_behavior.c)
volatile int g_tap_shake_frames = 0;

// Idle quirk system
static int quirk_timer = 300;          // frames until next quirk (~5-15s @20fps)
static int quirk_state = 0;            // 0=idle, 1=active, 2=recovering
static float quirk_t = 0;
static int quirk_type = 0;             // 0=widen, 1=look-L, 2=look-R, 3=wink-L, 4=wink-R
static float quirk_eye_dy = 0;         // temporary lid offset
static float quirk_iris_dx = 0;        // temporary iris offset

static float ease_in_out(float t) {
    return t < 0.5f ? 2 * t * t : 1 - powf(-2 * t + 2, 2) / 2;
}

static float ease_out_bounce(float t) {
    if (t < 0.6f) return 2.5f * t * t;
    t -= 0.6f;
    return 0.9f + 0.1f * sinf(t * 8.0f);
}

void display_init(void) {
    face_init();
    ESP_LOGI(TAG, "Display initialized (face_system)");
}

void display_set_emotion(emotion_t emotion) {
    if (emotion >= EMOTION_COUNT) return;
    face_roundness_base = EXPRESSION_DEFS[emotion].target.face.roundness;
    if (face_roundness_base < 0.58f) face_roundness_base = 0.58f;
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
        blink_t += 0.40f;
        if (blink_t >= 1.0f) {
            blink_t = 1.0f;
            blink_state = 2;
        }
    } else {
        blink_t -= 0.25f;
        if (blink_t <= 0) {
            blink_t = 0;
            blink_state = 0;
            wink_type = 0;
            frames_until_next_blink = 120 + (esp_random() % 240);
        }
    }

    // 2. Breath micro-animation (face roundness oscillation)
    breath_phase += 0.03f;
    float breath = sinf(breath_phase) * 0.025f;
    face_params_t fp = {.roundness = face_roundness_base + breath};
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
        saccade_amplitude = 0.04f + (float)(esp_random() % 60) * 0.001f;
        saccade_timer = 60 + (esp_random() % 120);
    }
    float micro_x = sinf(sac_phase * 1.3f) * saccade_amplitude;
    float micro_y = cosf(sac_phase * 0.9f) * saccade_amplitude;
    // Occasional large saccade (1 in 150 frames)
    if (esp_random() % 150 == 0) {
        micro_x += (float)((int)(esp_random() % 100) - 50) * 0.002f;
        micro_y += (float)((int)(esp_random() % 100) - 50) * 0.002f;
    }

    const face_state_t *st = animator_get_state();
    eye_params_t ep_l = st->eye[0];
    eye_params_t ep_r = st->eye[1];

    // saccade 微动
    ep_l.iris_center.dx += micro_x;
    ep_l.iris_center.dy += micro_y;
    ep_r.iris_center.dx += micro_x;
    ep_r.iris_center.dy += micro_y;

    // tilt 重力偏移: 眼睛跟随倾斜方向 (googly-eye 效果)
    float tilt_dx = g_tilt_roll * 0.55f;   // 左右倾 → 眼睛左右移
    float tilt_dy = -g_tilt_pitch * 0.4f;  // 前倾 → 眼睛上移
    if (tilt_dx > 0.25f)  tilt_dx = 0.25f;
    if (tilt_dx < -0.25f) tilt_dx = -0.25f;
    if (tilt_dy > 0.20f)  tilt_dy = 0.20f;
    if (tilt_dy < -0.20f) tilt_dy = -0.20f;
    ep_l.iris_center.dx += tilt_dx;
    ep_l.iris_center.dy += tilt_dy;
    ep_r.iris_center.dx += tilt_dx;
    ep_r.iris_center.dy += tilt_dy;

    // C6: Apply idle quirk effects to eyes
    if (quirk_state != 0) {
        float q_ease = ease_out_bounce(quirk_t);
        if (quirk_type == 3) {
            // Wink left eye only
            ep_l.top_lid_mid.dy += 0.45f * q_ease;
        } else if (quirk_type == 4) {
            // Wink right eye only
            ep_r.top_lid_mid.dy += 0.45f * q_ease;
        } else if (quirk_type == 0) {
            // Widen both eyes
            ep_l.top_lid_mid.dy -= 0.18f * q_ease;
            ep_r.top_lid_mid.dy -= 0.18f * q_ease;
        }
        // Look left/right iris offset
        ep_l.iris_center.dx += quirk_iris_dx;
        ep_r.iris_center.dx += quirk_iris_dx;
    }

    // C7: Tap shake — rapid iris oscillation
    if (g_tap_shake_frames > 0) {
        float shake_x = sinf((float)g_tap_shake_frames * 0.8f) * 0.06f;
        float shake_y = cosf((float)g_tap_shake_frames * 0.6f) * 0.04f;
        ep_l.iris_center.dx += shake_x;
        ep_l.iris_center.dy += shake_y;
        ep_r.iris_center.dx += shake_x;
        ep_r.iris_center.dy += shake_y;
        g_tap_shake_frames--;
    }

    face_set_component_instant(COMPONENT_EYE_LEFT, &ep_l);
    face_set_component_instant(COMPONENT_EYE_RIGHT, &ep_r);

    // C2: Brow micro-movement (independent left/right twitches)
    brow_phase_l += 0.05f;
    brow_phase_r += 0.07f;
    float brow_micro_l = sinf(brow_phase_l) * 0.022f;
    float brow_micro_r = sinf(brow_phase_r) * 0.022f;
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
            frames_until_mouth = 100 + (esp_random() % 200); // 2-5 seconds
        }
    }
    if (mouth_move_state != 0) {
        mouth_params_t mp = st->mouth;
        mp.openness += mouth_move_t * 0.08f;
        mp.lower_lip_mid.dy += mouth_move_t * 0.05f;
        face_set_component_instant(COMPONENT_MOUTH, &mp);
    }

    // C8: Idle quirk state machine (~every 5-15 seconds)
    if (quirk_state == 0) {
        quirk_timer--;
        if (quirk_timer <= 0) {
            quirk_state = 1;
            quirk_t = 0;
            quirk_type = esp_random() % 5; // 0=widen, 1=look-L, 2=look-R, 3=wink-L, 4=wink-R
            quirk_eye_dy = 0;
            quirk_iris_dx = 0;
        }
    } else if (quirk_state == 1) {
        quirk_t += 0.06f;
        if (quirk_t >= 1.0f) {
            quirk_t = 1.0f;
            quirk_state = 2; // start recovery
        }
        float ease_t = ease_out_bounce(quirk_t);
        // Compute quirk offsets
        if (quirk_type == 0) {
            // widen handled in eye block above via quirk_type check
        } else if (quirk_type == 1) {
            quirk_iris_dx = -0.12f * ease_t;
        } else if (quirk_type == 2) {
            quirk_iris_dx = 0.12f * ease_t;
        }
    } else {
        quirk_t -= 0.04f;
        if (quirk_t <= 0) {
            quirk_t = 0;
            quirk_state = 0;
            quirk_eye_dy = 0;
            quirk_iris_dx = 0;
            quirk_timer = 200 + (esp_random() % 400); // ~5–15s @ ~20fps
        }
    }

    // 5. LVGL tick + render
    face_animator_tick();
    face_render_frame();
}
