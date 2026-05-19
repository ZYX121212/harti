idf_component_register(
    SRCS "face_model.c" "face_animator.c" "face_renderer.c" "sprite_classic.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl gc9a01
)
