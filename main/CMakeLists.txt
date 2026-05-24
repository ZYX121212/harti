idf_component_register(
    SRCS "main.c" "app_sensors.c" "app_behavior.c" "app_ble.c" "app_input.c"
    INCLUDE_DIRS "."
    REQUIRES driver gc9a01 harti_imu harti_temp face_system
)
