#ifndef FACE_MODEL_H
#define FACE_MODEL_H

#include <stdint.h>
#include <stdbool.h>
#include "face_palette.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Keypoint (normalized -1.0..1.0) ─────────────────────── */

typedef struct {
    float dx;
    float dy;
} face_kpt_t;

/* ── Per-component parameter structs ─────────────────────── */

typedef struct {
    float roundness;
    float squash_x;    /* horizontal squash (-1.0..1.0), transient */
    float stretch_y;   /* vertical stretch (-1.0..1.0), transient */
} face_params_t;

typedef struct {
    face_kpt_t inner;
    face_kpt_t arch;
    face_kpt_t tail;
    float thickness;
    float taper;
} brow_params_t;

typedef struct {
    face_kpt_t inner_corner;
    face_kpt_t outer_corner;
    face_kpt_t top_lid_mid;
    face_kpt_t bot_lid_mid;
    face_kpt_t iris_center;
    float pupil_scale;
    float shine_intensity;
    float iris_detail;   /* 0..1, limbal ring + tertiary shine intensity */
    float eyelash;       /* 0..1, eyelash visibility */
} eye_params_t;

typedef struct {
    face_kpt_t left_corner;
    face_kpt_t right_corner;
    face_kpt_t upper_lip_mid;
    face_kpt_t lower_lip_mid;
    float openness;
    float cupid_depth;   /* 0..1, upper lip cupid's bow indentation */
    float tooth_show;    /* 0..1, teeth visibility when mouth open */
} mouth_params_t;

typedef struct {
    float blush;
    float tears;
    float stars;
    float sweat;
    float sparkle;
} decor_params_t;

/* ── Component enum ──────────────────────────────────────── */

typedef enum {
    COMPONENT_FACE = 0,
    COMPONENT_BROW_LEFT,
    COMPONENT_BROW_RIGHT,
    COMPONENT_EYE_LEFT,
    COMPONENT_EYE_RIGHT,
    COMPONENT_MOUTH,
    COMPONENT_DECOR,
    COMPONENT_COUNT
} face_component_t;

/* ── Full face state ─────────────────────────────────────── */

typedef struct {
    face_params_t face;
    brow_params_t brow[2];
    eye_params_t  eye[2];
    mouth_params_t mouth;
    decor_params_t decor;
} face_state_t;

/* ── Expression preset ───────────────────────────────────── */

typedef uint8_t expression_id_t;

typedef struct {
    expression_id_t expr_a;
    expression_id_t expr_b;
    float blend;          /* 0.0 = pure A, 1.0 = pure B */
} expression_blend_t;

typedef enum {
    PATH_LINEAR = 0,
    PATH_EASE_OUT,
    PATH_EASE_IN,
    PATH_EASE_IN_OUT,
    PATH_OVERSPEED,
} anim_path_type_t;

typedef struct {
    uint32_t duration_ms;
    uint32_t delay_ms;
    anim_path_type_t path_type;
} component_timing_t;

typedef struct {
    const char *name;
    face_state_t target;
    component_timing_t timing[COMPONENT_COUNT];
} expression_def_t;

/* ── Sprite set ──────────────────────────────────────────── */

typedef uint8_t sprite_id_t;
struct sprite_set_s;

typedef void (*sprite_draw_func_t)(int y, const face_state_t *st,
                                   const struct sprite_set_s *sp, uint16_t *buf);

typedef struct sprite_set_s {
    const char *name;
    float eye_radius;
    float eye_half_spacing;
    float mouth_y_center;
    float brow_y_offset;
    float blush_y_offset;
    sprite_draw_func_t draw_face;
    sprite_draw_func_t draw_blush;
    sprite_draw_func_t draw_mouth;
    sprite_draw_func_t draw_eye_left;
    sprite_draw_func_t draw_eye_right;
    sprite_draw_func_t draw_brow_left;
    sprite_draw_func_t draw_brow_right;
    sprite_draw_func_t draw_decor_overlay;
    const uint16_t *pal;
} sprite_set_t;

/* ── Extern declarations for built-in data ───────────────── */

extern const face_state_t FACE_STATE_NEUTRAL;
extern const expression_def_t EXPRESSION_DEFS[];
extern const uint8_t EXPRESSION_COUNT;
extern const sprite_set_t SPRITE_CLASSIC;

#ifdef __cplusplus
}
#endif

#endif
