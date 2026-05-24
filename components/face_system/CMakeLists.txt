idf_component_register(
    SRCS "face_model.c" "face_animator.c" "face_renderer.c" "sprite_classic.c" "sprite_lineart.c"
         "sprites/sprite_cat.c" "sprites/sprite_pixel.c" "sprites/sprite_robot.c"
    INCLUDE_DIRS "." "sprites"
    REQUIRES lvgl gc9a01
)
