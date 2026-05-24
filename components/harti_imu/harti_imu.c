#include "harti_imu.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "harti_imu";

#define MPU6050_ADDR          0x68
#define REG_WHO_AM_I          0x75
#define REG_PWR_MGMT_1        0x6B
#define REG_SMPLRT_DIV        0x19
#define REG_CONFIG            0x1A
#define REG_GYRO_CONFIG       0x1B
#define REG_ACCEL_CONFIG      0x1C
#define REG_ACCEL_XOUT_H      0x3B

#define MPU6050_WHO_AM_I_VAL  0x68
#define MPU6500_WHO_AM_I_VAL  0x70

// LSB per g for ±8g accel range (AFS_SEL=2)
#define ACCEL_LSB_PER_G       4096.0f
// LSB per dps for ±500dps gyro range (FS_SEL=1)
#define GYRO_LSB_PER_DPS      65.5f

static i2c_master_dev_handle_t dev_handle;
static bool imu_initialized = false;

static esp_err_t write_reg(uint8_t reg, uint8_t value) {
    uint8_t tx_buf[2] = { reg, value };
    return i2c_master_transmit(dev_handle, tx_buf, 2, pdMS_TO_TICKS(100));
}

esp_err_t imu_init(i2c_master_bus_handle_t bus) {
    if (imu_initialized) return ESP_OK;

    // 自动探测 I2C 地址: 先试 0x68 (AD0=GND), 再试 0x69 (AD0=VCC)
    uint8_t addrs[] = { 0x68, 0x69 };
    uint8_t who = 0;
    esp_err_t err = ESP_FAIL;

    for (int i = 0; i < sizeof(addrs); i++) {
        err = i2c_master_bus_add_device(bus, &(i2c_device_config_t) {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addrs[i],
            .scl_speed_hz = 400000,
        }, &dev_handle);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "failed to add device at 0x%02x", addrs[i]);
            continue;
        }

        err = i2c_master_transmit_receive(dev_handle, (uint8_t[]){ REG_WHO_AM_I }, 1, &who, 1, pdMS_TO_TICKS(100));
        if (err == ESP_OK && (who == MPU6050_WHO_AM_I_VAL || who == MPU6500_WHO_AM_I_VAL)) {
            const char *model = (who == MPU6500_WHO_AM_I_VAL) ? "MPU6500" : "MPU6050";
            ESP_LOGI(TAG, "%s found at 0x%02x, WHO_AM_I=0x%02x", model, addrs[i], who);
            break;
        }

        ESP_LOGW(TAG, "no MPU6050/MPU6500 at 0x%02x (err=%d who=0x%02x)", addrs[i], err, who);
        i2c_master_bus_rm_device(dev_handle);
        dev_handle = NULL;
        err = ESP_ERR_NOT_FOUND;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050/MPU6500 not found on any I2C address");
        return err;
    }

    // Wake up (clear sleep bit)
    ESP_RETURN_ON_ERROR(write_reg(REG_PWR_MGMT_1, 0x00), TAG, "PWR_MGMT_1 write failed");

    // Sample rate: 100Hz (1kHz DLPF output / (1 + 9) = 100Hz)
    ESP_RETURN_ON_ERROR(write_reg(REG_SMPLRT_DIV, 9), TAG, "SMPLRT_DIV write failed");

    // CONFIG: DLPF_CFG=1 (~184Hz accel bandwidth, ~188Hz gyro bandwidth)
    ESP_RETURN_ON_ERROR(write_reg(REG_CONFIG, 0x01), TAG, "CONFIG write failed");

    // Gyro: ±500dps (FS_SEL=1)
    ESP_RETURN_ON_ERROR(write_reg(REG_GYRO_CONFIG, 0x08), TAG, "GYRO_CONFIG write failed");

    // Accel: ±8g (AFS_SEL=2)
    ESP_RETURN_ON_ERROR(write_reg(REG_ACCEL_CONFIG, 0x10), TAG, "ACCEL_CONFIG write failed");

    ESP_LOGI(TAG, "MPU6050 initialized (accel ±8g, gyro ±500dps, ODR 100Hz)");
    imu_initialized = true;
    return ESP_OK;
}

esp_err_t imu_read(imu_data_t *data) {
    if (data == NULL) return ESP_ERR_INVALID_ARG;
    uint8_t rx_buf[14];
    esp_err_t err = i2c_master_transmit_receive(
        dev_handle,
        (uint8_t[]){ REG_ACCEL_XOUT_H }, 1,
        rx_buf, 14, pdMS_TO_TICKS(100)
    );
    if (err != ESP_OK) {
        return err;
    }

    // MPU6050 auto-increments from ACCEL_XOUT_H:
    // [0:1] ACCEL_X, [2:3] ACCEL_Y, [4:5] ACCEL_Z,
    // [6:7] TEMP (skip),
    // [8:9] GYRO_X, [10:11] GYRO_Y, [12:13] GYRO_Z
    // All values big-endian

    int16_t raw;
    raw = (int16_t)((uint16_t)rx_buf[0] << 8 | rx_buf[1]);
    data->accel[0] = raw / ACCEL_LSB_PER_G;

    raw = (int16_t)((uint16_t)rx_buf[2] << 8 | rx_buf[3]);
    data->accel[1] = raw / ACCEL_LSB_PER_G;

    raw = (int16_t)((uint16_t)rx_buf[4] << 8 | rx_buf[5]);
    data->accel[2] = raw / ACCEL_LSB_PER_G;

    raw = (int16_t)((uint16_t)rx_buf[8] << 8 | rx_buf[9]);
    data->gyro[0] = raw / GYRO_LSB_PER_DPS;

    raw = (int16_t)((uint16_t)rx_buf[10] << 8 | rx_buf[11]);
    data->gyro[1] = raw / GYRO_LSB_PER_DPS;

    raw = (int16_t)((uint16_t)rx_buf[12] << 8 | rx_buf[13]);
    data->gyro[2] = raw / GYRO_LSB_PER_DPS;

    return ESP_OK;
}
