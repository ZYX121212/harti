#include "app_sensors.h"
#include "harti_imu.h"
#include "harti_temp.h"
#include "driver/i2c_master.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include "driver/touch_pad.h"
#pragma GCC diagnostic pop
#include "esp_log.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "sensors";

// Pin 配置 (与 HARDWARE.md 一致)
#define I2C_SCL_IO  GPIO_NUM_9
#define I2C_SDA_IO  GPIO_NUM_8
#define TOUCH_PAD   TOUCH_PAD_NUM0  // GPIO1 电容触摸 (ESP32-S3: TOUCH0=GPIO1)

#define SENSOR_TASK_STACK  2048
#define SENSOR_TASK_PRIO   3
#define IMU_SAMPLE_PERIOD_MS  10  // 100Hz

static QueueHandle_t event_queue;

// ── IMU 事件检测 ─────────────────────────────────────

#define SHAKE_G_THRESHOLD   2.5f
#define TAP_G_THRESHOLD     1.5f

// 滑动窗口: 50帧 @ 100Hz = 500ms
static float accel_history[50][3];
static int   accel_history_idx = 0;
static int   accel_history_count = 0;

static bool  tap_armed = true;
static int   tap_count = 0;
static int   tap_quiet_frames = 0;

static int   shake_frames = 0;

static int   flip_z_down_frames = 0;
static bool  flip_armed = true;

// ── Twist 检测状态 (陀螺仪 Z 轴) ──
static int   twist_frames = 0;
static bool  twist_armed = true;
static int   twist_cooldown = 0;

// ── Tilt 检测状态 ──
static int   tilt_frames = 0;
static int   tilt_last_dir = -1;  // 0前 1左 2右 3后, -1=无
static bool  tilt_armed = true;

// 全局倾斜角度 (供 display 任务读取, 实现眼睛跟踪)
float g_tilt_pitch = 0.0f;
float g_tilt_roll  = 0.0f;

static void process_imu(const imu_data_t *data) {
    float ax = data->accel[0], ay = data->accel[1], az = data->accel[2];
    float gz = data->gyro[2];
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    float dynamic_mag = fabsf(mag - 1.0f);

    // ── 计算倾斜角度 (pitch/roll) ──
    // pitch: 绕 X 轴旋转 (前倾为正)
    // roll:  绕 Y 轴旋转 (右倾为正)
    g_tilt_pitch = atan2f(ax, sqrtf(ay * ay + az * az));
    g_tilt_roll  = atan2f(ay, az);

    // 存入滑动窗口
    accel_history[accel_history_idx][0] = ax;
    accel_history[accel_history_idx][1] = ay;
    accel_history[accel_history_idx][2] = az;
    accel_history_idx = (accel_history_idx + 1) % 50;
    if (accel_history_count < 50) accel_history_count++;

    // ── 轻拍: 短脉冲 < 100ms ──
    if (dynamic_mag > TAP_G_THRESHOLD && tap_armed) {
        int above_count = 0;
        for (int i = 0; i < accel_history_count && i < 10; i++) {
            int idx = (accel_history_idx - 1 - i + 50) % 50;
            float m = sqrtf(accel_history[idx][0] * accel_history[idx][0] +
                           accel_history[idx][1] * accel_history[idx][1] +
                           accel_history[idx][2] * accel_history[idx][2]);
            if (fabsf(m - 1.0f) > TAP_G_THRESHOLD) above_count++;
        }
        if (above_count <= 5) { // ≤ 5帧 = ≤ 50ms
            tap_count++;
            tap_armed = false;
            tap_quiet_frames = 0;
            sensor_event_msg_t msg = { .type = EVT_TAP, .value = tap_count };
            xQueueSend(event_queue, &msg, 0);
            ESP_LOGI(TAG, "TAP detected, count=%d", tap_count);
        }
    }

    if (!tap_armed) {
        tap_quiet_frames++;
        if (tap_quiet_frames > 30) { // 300ms 后允许下一组 tap
            tap_armed = true;
            tap_count = 0;
        }
    }

    // ── 摇晃: 持续高幅值 > 300ms, 含方向区分 ──
    if (dynamic_mag > SHAKE_G_THRESHOLD) {
        shake_frames++;
        if (shake_frames == 30) { // 30 帧 = 300ms
            // 回溯 30 帧计算 X/Y 轴方差, 判定方向
            float sum_x = 0, sum_y = 0, sum_x2 = 0, sum_y2 = 0;
            int n = (accel_history_count < 30) ? accel_history_count : 30;
            for (int i = 0; i < n; i++) {
                int idx = (accel_history_idx - 1 - i + 50) % 50;
                float ax = accel_history[idx][0];
                float ay = accel_history[idx][1];
                sum_x += ax;
                sum_y += ay;
                sum_x2 += ax * ax;
                sum_y2 += ay * ay;
            }
            float var_x = (sum_x2 - sum_x * sum_x / n) / n;
            float var_y = (sum_y2 - sum_y * sum_y / n) / n;
            float dir_val = 0.0f; // 默认全向
            if (var_x > var_y * 2.5f)      dir_val = 1.0f; // 水平摇
            else if (var_y > var_x * 2.5f) dir_val = 2.0f; // 垂直摇

            sensor_event_msg_t msg = { .type = EVT_SHAKE, .value = dir_val };
            xQueueSend(event_queue, &msg, 0);
            ESP_LOGI(TAG, "SHAKE detected, dir=%.0f (var_x=%.4f var_y=%.4f)",
                     dir_val, var_x, var_y);
        }
    } else {
        shake_frames = 0;
    }

    // ── 翻转: Z 轴持续反向 > 500ms ──
    if (az < -0.7f) {
        flip_z_down_frames++;
        if (flip_z_down_frames >= 50 && flip_armed) { // 50 帧 = 500ms
            sensor_event_msg_t msg = { .type = EVT_FLIP, .value = 0 };
            xQueueSend(event_queue, &msg, 0);
            flip_armed = false;
            ESP_LOGI(TAG, "FLIP detected");
        }
    } else {
        if (flip_z_down_frames > 0 && flip_z_down_frames < 50) {
            flip_armed = true; // 短暂翻转恢复后重新武装
        }
        flip_z_down_frames = 0;
    }

    // ── 旋转检测 (Twist): 陀螺仪 Z 轴 > 100°/s 持续 > 100ms ──
    if (twist_cooldown > 0) {
        twist_cooldown--;
    }
    if (fabsf(gz) > 100.0f && twist_armed) {
        twist_frames++;
        if (twist_frames >= 10 && twist_cooldown == 0) { // 10 帧 = 100ms
            float dir = (gz > 0) ? 1.0f : -1.0f;
            sensor_event_msg_t msg = { .type = EVT_TWIST, .value = dir };
            xQueueSend(event_queue, &msg, 0);
            twist_armed = false;
            twist_cooldown = 50; // 500ms 冷却
            ESP_LOGI(TAG, "TWIST detected, dir=%.0f gz=%.1f", dir, gz);
        }
    } else {
        twist_frames = 0;
        if (fabsf(gz) < 30.0f && twist_cooldown == 0) {
            twist_armed = true;
        }
    }

    // ── 倾斜检测 (Tilt): pitch/roll > 25° 持续 > 250ms ──
    float abs_pitch = fabsf(g_tilt_pitch);
    float abs_roll  = fabsf(g_tilt_roll);
    int current_dir = -1;

    if (abs_pitch > 0.436f && abs_pitch > abs_roll) {       // > 25° = PI/7.2
        current_dir = (g_tilt_pitch > 0) ? 0 : 3;            // 0=前倾, 3=后倾
    } else if (abs_roll > 0.436f) {
        current_dir = (g_tilt_roll > 0) ? 2 : 1;             // 2=右倾, 1=左倾
    }

    if (current_dir >= 0 && current_dir == tilt_last_dir && tilt_armed) {
        tilt_frames++;
        if (tilt_frames >= 25) { // 25 帧 = 250ms
            sensor_event_msg_t msg = { .type = EVT_TILT, .value = (float)current_dir };
            xQueueSend(event_queue, &msg, 0);
            tilt_armed = false;
            ESP_LOGI(TAG, "TILT detected, dir=%d", current_dir);
        }
    } else {
        tilt_frames = 0;
        if (current_dir < 0) {
            tilt_armed = true; // 回到水平重新武装
        }
    }
    tilt_last_dir = current_dir;
}

// ── 触摸检测 ─────────────────────────────────────────

static bool touch_active = false;
static int  touch_debounce = 0;

static void process_touch(void) {
    uint32_t touch_val;
    if (touch_pad_read_raw_data(TOUCH_PAD, &touch_val) != ESP_OK) return;

    // 阈值: 触摸时电容值下降, 低于 600 表示被触摸 (需根据外壳调试)
    bool touched = (touch_val < 600);

    if (touched && !touch_active) {
        touch_debounce++;
        if (touch_debounce >= 5) { // 5 帧去抖 (约 100ms @ 每2个IMU周期)
            touch_active = true;
            sensor_event_msg_t msg = { .type = EVT_TOUCH_HEAD, .value = 0 };
            xQueueSend(event_queue, &msg, 0);
            ESP_LOGI(TAG, "TOUCH_HEAD");
        }
    } else if (!touched && touch_active) {
        touch_debounce = 0;
        touch_active = false;
        sensor_event_msg_t msg = { .type = EVT_TOUCH_RELEASE, .value = 0 };
        xQueueSend(event_queue, &msg, 0);
        ESP_LOGI(TAG, "TOUCH_RELEASE");
    } else if (!touched) {
        touch_debounce = 0;
    }
}

// ── 温度检测 ─────────────────────────────────────────

static float last_temp = 25.0f;
static int   warm_up_frames = 0;
static int   temp_sample_counter = 0;

static void process_temp(void) {
    temp_sample_counter++;
    if (temp_sample_counter < 100) return; // 每 100 个 IMU 周期 ≈ 1 秒
    temp_sample_counter = 0;

    float temp = temp_read_celsius();

    // 捂热: 每秒升温 > 0.4°C, 连续 5 秒
    float delta = temp - last_temp;
    if (delta > 0.4f) {
        warm_up_frames++;
        if (warm_up_frames >= 5) {
            sensor_event_msg_t msg = { .type = EVT_WARM_UP, .value = temp };
            xQueueSend(event_queue, &msg, 0);
            warm_up_frames = 0;
            ESP_LOGI(TAG, "WARM_UP temp=%.1f", temp);
        }
    } else {
        warm_up_frames = 0;
    }

    // 低温
    if (temp < 15.0f) {
        sensor_event_msg_t msg = { .type = EVT_COLD_DOWN, .value = temp };
        xQueueSend(event_queue, &msg, 0);
        ESP_LOGI(TAG, "COLD_DOWN temp=%.1f", temp);
    }

    last_temp = temp;
}

// ── 传感器任务入口 ───────────────────────────────────

static void sensor_task(void *arg) {
    ESP_LOGI(TAG, "Sensor task starting...");

    // I2C 总线初始化
    i2c_master_bus_config_t i2c_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_SCL_IO,
        .sda_io_num = I2C_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    esp_err_t err = i2c_new_master_bus(&i2c_cfg, &bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C bus init failed: %s, sensors disabled", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    // IMU 初始化
    err = imu_init(bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "IMU init failed: %s, sensors disabled", esp_err_to_name(err));
        i2c_del_master_bus(bus);
        vTaskDelete(NULL);
        return;
    }

    // 触摸初始化
    touch_pad_init();
    touch_pad_config(TOUCH_PAD);
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    touch_pad_filter_enable();

    // 温度初始化
    err = temp_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Temp init failed: %s, continuing without temp", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "All sensors initialized");

    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        imu_data_t imu_data;
        if (imu_read(&imu_data) == ESP_OK) {
            process_imu(&imu_data);
        }

        // 触摸: 每 2 个 IMU 周期读一次 (≈50Hz)
        static int touch_div = 0;
        touch_div++;
        if (touch_div >= 2) {
            touch_div = 0;
            process_touch();
        }

        // 温度: 内部自行节流至 1Hz
        process_temp();

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(IMU_SAMPLE_PERIOD_MS));
    }
}

QueueHandle_t sensors_start(void) {
    event_queue = xQueueCreate(10, sizeof(sensor_event_msg_t));
    xTaskCreate(sensor_task, "sensor", SENSOR_TASK_STACK, NULL, SENSOR_TASK_PRIO, NULL);
    return event_queue;
}
