#ifndef HARTI_CONFIG_H
#define HARTI_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── Shared application types ────────────────────────────── */

// Maps 1:1 to expression_id_t in face_system
typedef enum {
    EMOTION_NEUTRAL = 0,
    EMOTION_HAPPY,
    EMOTION_SAD,
    EMOTION_SURPRISED,
    EMOTION_SLEEPY,
    EMOTION_ANGRY,
    EMOTION_BORED,
    EMOTION_EXCITED,
    EMOTION_CONFUSED,
    EMOTION_CONTENT,
    EMOTION_COLD,
    EMOTION_WARM,
    EMOTION_HEART_EYES,
    EMOTION_THINKING,
    EMOTION_COUNT
} emotion_t;

/* ── Task config ─────────────────────────────────────────── */

#define DISPLAY_TASK_STACK      4096
#define DISPLAY_TASK_PRIO       5

#define SENSOR_TASK_STACK       2048
#define SENSOR_TASK_PRIO        3

#define BEHAVIOR_TASK_STACK     2048
#define BEHAVIOR_TASK_PRIO      3

#define BUTTON_TASK_STACK       2048
#define BUTTON_TASK_PRIO        3

#define BLE_TASK_STACK          3072
#define BLE_TASK_PRIO           1

/* ── Sensor task timing ──────────────────────────────────── */

#define IMU_SAMPLE_PERIOD_MS    10      // 100Hz
#define TEMP_SAMPLE_INTERVAL    100     // 每 100 个 IMU 周期 ≈ 1s

/* ── IMU thresholds ──────────────────────────────────────── */

#define SHAKE_G_THRESHOLD       2.5f    // 加速度幅值 > 2.5g
#define TAP_G_THRESHOLD         1.5f    // 加速度幅值 > 1.5g
#define TAP_MAX_FRAMES          5       // ≤ 5 帧 = ≤ 50ms
#define TAP_QUIET_FRAMES        30      // 300ms 后允许下一组 tap
#define SHAKE_MIN_FRAMES        30      // 30 帧 = 300ms

#define FLIP_Z_THRESH           -0.7f   // Z 轴反向阈值
#define FLIP_MIN_FRAMES         50      // 50 帧 = 500ms

#define TWIST_GYRO_THRESH       100.0f  // 陀螺仪 Z 轴 > 100°/s
#define TWIST_MIN_FRAMES        10      // 10 帧 = 100ms
#define TWIST_COOLDOWN_FRAMES   50      // 500ms 冷却

#define TILT_ANGLE_THRESH       0.436f  // > 25° = PI/7.2
#define TILT_MIN_FRAMES         25      // 25 帧 = 250ms

/* ── Touch / button ──────────────────────────────────────── */

#define BUTTON_POLL_MS          30
#define BUTTON_PRESS_THRESH     400     // ADC < 400 = 按下
#define BUTTON_RELEASE_THRESH   800     // ADC > 800 = 释放
#define BUTTON_DEBOUNCE_CNT     3

/* ── Temperature thresholds ──────────────────────────────── */

#define WARM_UP_DELTA_PER_SEC   0.4f    // 每秒升温 > 0.4°C
#define WARM_UP_DURATION_SEC    5       // 连续 5 秒
#define COLD_TEMP_THRESH        15.0f   // 低温阈值

/* ── Behavior timing ─────────────────────────────────────── */

#define IDLE_BORED_SEC          10      // 10s 无互动 → BORED
#define IDLE_SLEEPY_SEC         30      // 30s 无互动 → SLEEPY
#define SURPRISE_TIMEOUT_SEC    3       // SURPRISED 后 3s → CONFUSED
#define CONFUSED_TIMEOUT_SEC    5       // CONFUSED 后 5s → IDLE
#define THINKING_TIMEOUT_SEC    4       // THINKING 后 4s → IDLE
#define HAPPY_TIMEOUT_SEC       4       // HAPPY 后 4s → CONTENT
#define CONTENT_TIMEOUT_SEC     5       // CONTENT 后 5s → IDLE

/* ── Display ─────────────────────────────────────────────── */

#define DISPLAY_FRAME_MS        50      // 20fps

#ifdef __cplusplus
}
#endif

#endif
