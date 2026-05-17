#include "app_sensors.h"
#include "harti_imu.h"
#include "harti_temp.h"
#include "driver/i2c_master.h"
#include "driver/touch_pad.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "sensors";

// Pin 配置 (可根据硬件调整)
#define I2C_SCL_IO  GPIO_NUM_15
#define I2C_SDA_IO  GPIO_NUM_16
#define TOUCH_PAD   TOUCH_PAD_NUM0  // GPIO0 电容触摸

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

static void process_imu(const imu_data_t *data) {
    float ax = data->accel[0], ay = data->accel[1], az = data->accel[2];
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    float dynamic_mag = fabsf(mag - 1.0f);

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

    // ── 摇晃: 持续高幅值 > 300ms ──
    if (dynamic_mag > SHAKE_G_THRESHOLD) {
        shake_frames++;
        if (shake_frames == 30) { // 30 帧 = 300ms
            sensor_event_msg_t msg = { .type = EVT_SHAKE, .value = dynamic_mag };
            xQueueSend(event_queue, &msg, 0);
            ESP_LOGI(TAG, "SHAKE detected, mag=%.2f", dynamic_mag);
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
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &bus));

    // IMU 初始化
    ESP_ERROR_CHECK(imu_init(bus));

    // 触摸初始化
    touch_pad_init();
    touch_pad_config(TOUCH_PAD);
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    touch_pad_filter_start(10);

    // 温度初始化
    ESP_ERROR_CHECK(temp_init());

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
