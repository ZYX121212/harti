idf_component_register(
    SRCS "face_model.c" "face_animator.c" "face_renderer.c" "face_palette.c" "face_micro.c" "face_prop.c" "face_vivid.c" "face_temperament.c" "face_seq.c"
         "sprites/sprite_registry.c"
         "sprites/sprite_classic.c" "sprites/sprite_lineart.c"
         "sprites/sprite_cat.c" "sprites/sprite_pixel.c" "sprites/sprite_robot.c" "sprites/sprite_vector.c" "sprites/sprite_nova.c" "sprites/sprite_pig.c" "sprites/sprite_chibi.c"
    INCLUDE_DIRS "." "sprites"
    REQUIRES lvgl gc9a01
)
