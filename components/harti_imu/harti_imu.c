#include "harti_imu.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "harti_imu";

#define QMI8658_ADDR         0x6B
#define REG_WHO_AM_I         0x00
#define REG_CTRL1            0x02
#define REG_CTRL2            0x03
#define REG_CTRL5            0x05
#define REG_CTRL6            0x06
#define REG_ACCEL_X_L        0x35

#define QMI8658_WHO_AM_I_VAL 0x05

// LSB per g for ±8g accel range
#define ACCEL_LSB_PER_G      4096.0f
// LSB per dps for ±512dps gyro range
#define GYRO_LSB_PER_DPS     16.0f

static i2c_master_dev_handle_t dev_handle;
static bool imu_initialized = false;

static esp_err_t write_reg(uint8_t reg, uint8_t value) {
    uint8_t tx_buf[2] = { reg, value };
    return i2c_master_transmit(dev_handle, tx_buf, 2, -1);
}

esp_err_t imu_init(i2c_master_bus_handle_t bus) {
    if (imu_initialized) return ESP_OK;
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(bus, &(i2c_device_config_t) {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = QMI8658_ADDR,
            .scl_speed_hz = 400000,
        }, &dev_handle),
        TAG, "failed to add I2C device"
    );

    // Verify WHO_AM_I
    uint8_t who;
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit_receive(dev_handle, (uint8_t[]){ REG_WHO_AM_I }, 1, &who, 1, -1),
        TAG, "WHO_AM_I read failed"
    );
    if (who != QMI8658_WHO_AM_I_VAL) {
        ESP_LOGE(TAG, "unexpected WHO_AM_I: 0x%02x (expected 0x%02x)", who, QMI8658_WHO_AM_I_VAL);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "WHO_AM_I: 0x%02x OK", who);

    // CTRL2: accel ±8g (0b010 << 4), gyro ±512dps (0b010 << 0)
    ESP_RETURN_ON_ERROR(write_reg(REG_CTRL2, (2 << 4) | (2 << 0)), TAG, "CTRL2 write failed");

    // CTRL5: ODR 100Hz for both accel and gyro (0b1001 = 100Hz)
    //  - high nibble: accel ODR
    //  - low nibble: gyro ODR
    // 100Hz = ODR[3:0] = 0x09
    ESP_RETURN_ON_ERROR(write_reg(REG_CTRL5, (9 << 4) | 9), TAG, "CTRL5 write failed");

    // CTRL6: LPF off (raw data)
    ESP_RETURN_ON_ERROR(write_reg(REG_CTRL6, 0x00), TAG, "CTRL6 write failed");

    // CTRL1: enable accel (bit 0) and gyro (bit 1)
    ESP_RETURN_ON_ERROR(write_reg(REG_CTRL1, 0x03), TAG, "CTRL1 write failed");

    ESP_LOGI(TAG, "QMI8658 initialized (accel ±8g, gyro ±512dps, ODR 100Hz)");
    imu_initialized = true;
    return ESP_OK;
}

esp_err_t imu_read(imu_data_t *data) {
    if (data == NULL) return ESP_ERR_INVALID_ARG;
    uint8_t rx_buf[12];
    esp_err_t err = i2c_master_transmit_receive(
        dev_handle,
        (uint8_t[]){ REG_ACCEL_X_L }, 1,
        rx_buf, 12, -1
    );
    if (err != ESP_OK) {
        return err;
    }

    // Sequential read starting at ACCEL_X_L gives:
    // [0] ACCEL_X_L, [1] ACCEL_X_H,
    // [2] ACCEL_Y_L, [3] ACCEL_Y_H,
    // [4] ACCEL_Z_L, [5] ACCEL_Z_H,
    // [6] GYRO_X_L,  [7] GYRO_X_H,
    // [8] GYRO_Y_L,  [9] GYRO_Y_H,
    // [10] GYRO_Z_L, [11] GYRO_Z_H

    int16_t raw;
    raw = (int16_t)((uint16_t)rx_buf[1] << 8 | rx_buf[0]);
    data->accel[0] = raw / ACCEL_LSB_PER_G;

    raw = (int16_t)((uint16_t)rx_buf[3] << 8 | rx_buf[2]);
    data->accel[1] = raw / ACCEL_LSB_PER_G;

    raw = (int16_t)((uint16_t)rx_buf[5] << 8 | rx_buf[4]);
    data->accel[2] = raw / ACCEL_LSB_PER_G;

    raw = (int16_t)((uint16_t)rx_buf[7] << 8 | rx_buf[6]);
    data->gyro[0] = raw / GYRO_LSB_PER_DPS;

    raw = (int16_t)((uint16_t)rx_buf[9] << 8 | rx_buf[8]);
    data->gyro[1] = raw / GYRO_LSB_PER_DPS;

    raw = (int16_t)((uint16_t)rx_buf[11] << 8 | rx_buf[10]);
    data->gyro[2] = raw / GYRO_LSB_PER_DPS;

    return ESP_OK;
}
