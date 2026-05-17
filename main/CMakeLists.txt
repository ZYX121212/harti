idf_component_register(
    SRCS "main.c" "app_display.c" "app_sensors.c" "app_behavior.c" "app_effects.c" "app_ble.c"
    INCLUDE_DIRS "."
    REQUIRES driver expressive_eyes gc9a01 harti_imu harti_temp
)
