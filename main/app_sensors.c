#include "app_sensors.h"
#include "harti_config.h"
#include "harti_imu.h"
#include "harti_temp.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "sensors";

// Pin config
#define I2C_SCL_IO  GPIO_NUM_9
#define I2C_SDA_IO  GPIO_NUM_8

static QueueHandle_t event_queue;

// ── IMU event detection ────────────────────────────────────

// Sliding window: 50 frames @ 100Hz = 500ms
static float accel_history[50][3];
static int   accel_history_idx = 0;
static int   accel_history_count = 0;

static bool  tap_armed = true;
static int   tap_count = 0;
static int   tap_quiet_frames = 0;

static int   shake_frames = 0;

static int   flip_z_down_frames = 0;
static bool  flip_armed = true;

// Twist detection state (gyro Z-axis)
static int   twist_frames = 0;
static bool  twist_armed = true;
static int   twist_cooldown = 0;

// Tilt detection state
static int   tilt_frames = 0;
static int   tilt_last_dir = -1;
static bool  tilt_armed = true;

// Global tilt angles (read by display task for eye tracking)
float g_tilt_pitch = 0.0f;
float g_tilt_roll  = 0.0f;

static void process_imu(const imu_data_t *data) {
    float ax = data->accel[0], ay = data->accel[1], az = data->accel[2];
    float gz = data->gyro[2];
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    float dynamic_mag = fabsf(mag - 1.0f);

    // ── Tilt angles ──
    g_tilt_pitch = atan2f(ax, sqrtf(ay * ay + az * az));
    g_tilt_roll  = atan2f(ay, az);

    // Store in sliding window
    accel_history[accel_history_idx][0] = ax;
    accel_history[accel_history_idx][1] = ay;
    accel_history[accel_history_idx][2] = az;
    accel_history_idx = (accel_history_idx + 1) % 50;
    if (accel_history_count < 50) accel_history_count++;

    // ── Tap detection (< 100ms pulse) ──
    if (dynamic_mag > TAP_G_THRESHOLD && tap_armed) {
        int above_count = 0;
        for (int i = 0; i < accel_history_count && i < 10; i++) {
            int idx = (accel_history_idx - 1 - i + 50) % 50;
            float m = sqrtf(accel_history[idx][0] * accel_history[idx][0] +
                           accel_history[idx][1] * accel_history[idx][1] +
                           accel_history[idx][2] * accel_history[idx][2]);
            if (fabsf(m - 1.0f) > TAP_G_THRESHOLD) above_count++;
        }
        if (above_count <= TAP_MAX_FRAMES) {
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
        if (tap_quiet_frames > TAP_QUIET_FRAMES) {
            tap_armed = true;
            tap_count = 0;
        }
    }

    // ── Shake detection (> 300ms sustained high magnitude, with direction) ──
    if (dynamic_mag > SHAKE_G_THRESHOLD) {
        shake_frames++;
        if (shake_frames == SHAKE_MIN_FRAMES) {
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
            float dir_val = 0.0f;
            if (var_x > var_y * 2.5f)      dir_val = 1.0f;
            else if (var_y > var_x * 2.5f) dir_val = 2.0f;

            sensor_event_msg_t msg = { .type = EVT_SHAKE, .value = dir_val };
            xQueueSend(event_queue, &msg, 0);
            ESP_LOGI(TAG, "SHAKE detected, dir=%.0f (var_x=%.4f var_y=%.4f)",
                     dir_val, var_x, var_y);
        }
    } else {
        shake_frames = 0;
    }

    // ── Flip detection (Z-axis sustained reversal > 500ms) ──
    if (az < FLIP_Z_THRESH) {
        flip_z_down_frames++;
        if (flip_z_down_frames >= FLIP_MIN_FRAMES && flip_armed) {
            sensor_event_msg_t msg = { .type = EVT_FLIP, .value = 0 };
            xQueueSend(event_queue, &msg, 0);
            flip_armed = false;
            ESP_LOGI(TAG, "FLIP detected");
        }
    } else {
        if (flip_z_down_frames > 0 && flip_z_down_frames < FLIP_MIN_FRAMES) {
            flip_armed = true;
        }
        flip_z_down_frames = 0;
    }

    // ── Twist detection (gyro Z > 100°/s sustained > 100ms) ──
    if (twist_cooldown > 0) {
        twist_cooldown--;
    }
    if (fabsf(gz) > TWIST_GYRO_THRESH && twist_armed) {
        twist_frames++;
        if (twist_frames >= TWIST_MIN_FRAMES && twist_cooldown == 0) {
            float dir = (gz > 0) ? 1.0f : -1.0f;
            sensor_event_msg_t msg = { .type = EVT_TWIST, .value = dir };
            xQueueSend(event_queue, &msg, 0);
            twist_armed = false;
            twist_cooldown = TWIST_COOLDOWN_FRAMES;
            ESP_LOGI(TAG, "TWIST detected, dir=%.0f gz=%.1f", dir, gz);
        }
    } else {
        twist_frames = 0;
        if (fabsf(gz) < 30.0f && twist_cooldown == 0) {
            twist_armed = true;
        }
    }

    // ── Tilt detection (pitch/roll > 25° sustained > 250ms) ──
    float abs_pitch = fabsf(g_tilt_pitch);
    float abs_roll  = fabsf(g_tilt_roll);
    int current_dir = -1;

    if (abs_pitch > TILT_ANGLE_THRESH && abs_pitch > abs_roll) {
        current_dir = (g_tilt_pitch > 0) ? 0 : 3;
    } else if (abs_roll > TILT_ANGLE_THRESH) {
        current_dir = (g_tilt_roll > 0) ? 2 : 1;
    }

    if (current_dir >= 0 && current_dir == tilt_last_dir && tilt_armed) {
        tilt_frames++;
        if (tilt_frames >= TILT_MIN_FRAMES) {
            sensor_event_msg_t msg = { .type = EVT_TILT, .value = (float)current_dir };
            xQueueSend(event_queue, &msg, 0);
            tilt_armed = false;
            ESP_LOGI(TAG, "TILT detected, dir=%d", current_dir);
        }
    } else {
        tilt_frames = 0;
        if (current_dir < 0) {
            tilt_armed = true;
        }
    }
    tilt_last_dir = current_dir;
}

// ── Temperature detection ─────────────────────────────────

static float last_temp = 25.0f;
static int   warm_up_frames = 0;
static int   temp_sample_counter = 0;

static void process_temp(void) {
    temp_sample_counter++;
    if (temp_sample_counter < TEMP_SAMPLE_INTERVAL) return;
    temp_sample_counter = 0;

    float temp = temp_read_celsius();

    // Warm-up: temperature rising > 0.4°C/s sustained 5s
    float delta = temp - last_temp;
    if (delta > WARM_UP_DELTA_PER_SEC) {
        warm_up_frames++;
        if (warm_up_frames >= WARM_UP_DURATION_SEC) {
            sensor_event_msg_t msg = { .type = EVT_WARM_UP, .value = temp };
            xQueueSend(event_queue, &msg, 0);
            warm_up_frames = 0;
            ESP_LOGI(TAG, "WARM_UP temp=%.1f", temp);
        }
    } else {
        warm_up_frames = 0;
    }

    // Cold
    if (temp < COLD_TEMP_THRESH) {
        sensor_event_msg_t msg = { .type = EVT_COLD_DOWN, .value = temp };
        xQueueSend(event_queue, &msg, 0);
        ESP_LOGI(TAG, "COLD_DOWN temp=%.1f", temp);
    }

    last_temp = temp;
}

// ── Sensor task entry ─────────────────────────────────────

static void sensor_task(void *arg) {
    ESP_LOGI(TAG, "Sensor task starting...");

    // I2C bus init
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

    // IMU init
    err = imu_init(bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "IMU init failed: %s, sensors disabled", esp_err_to_name(err));
        i2c_del_master_bus(bus);
        vTaskDelete(NULL);
        return;
    }

    // Temperature init
    err = temp_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Temp init failed: %s, continuing without temp", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "All sensors initialized");

    TickType_t last_wake = xTaskGetTickCount();
    int print_counter = 0;
    while (1) {
        imu_data_t imu_data;
        if (imu_read(&imu_data) == ESP_OK) {
            process_imu(&imu_data);

            // UART debug output: every 50 cycles (~0.5s)
            print_counter++;
            if (print_counter >= 50) {
                print_counter = 0;
                printf("[IMU] accel: x=%.3fg y=%.3fg z=%.3fg | gyro: x=%.1fdps y=%.1fdps z=%.1fdps | pitch=%.1f° roll=%.1f°\n",
                       imu_data.accel[0], imu_data.accel[1], imu_data.accel[2],
                       imu_data.gyro[0], imu_data.gyro[1], imu_data.gyro[2],
                       g_tilt_pitch * 57.3f, g_tilt_roll * 57.3f);
            }
        }

        // Temperature (internally throttled to 1Hz)
        process_temp();

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(IMU_SAMPLE_PERIOD_MS));
    }
}

QueueHandle_t sensors_start(void) {
    event_queue = xQueueCreate(10, sizeof(sensor_event_msg_t));
    xTaskCreate(sensor_task, "sensor", SENSOR_TASK_STACK, NULL, SENSOR_TASK_PRIO, NULL);
    return event_queue;
}
