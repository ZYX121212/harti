idf_component_register(
    SRCS "main.c" "app_display.c" "app_sensors.c"
    INCLUDE_DIRS "."
    REQUIRES driver expressive_eyes gc9a01 harti_imu harti_temp
)
