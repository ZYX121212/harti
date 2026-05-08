idf_component_register(
    SRCS "harti_imu.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_i2c
)
