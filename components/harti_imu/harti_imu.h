#ifndef HARTI_IMU_H
#define HARTI_IMU_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float accel[3];  // x/y/z in g
    float gyro[3];   // x/y/z in dps
} imu_data_t;

esp_err_t imu_init(i2c_master_bus_handle_t bus);
esp_err_t imu_read(imu_data_t *data);

#ifdef __cplusplus
}
#endif

#endif
