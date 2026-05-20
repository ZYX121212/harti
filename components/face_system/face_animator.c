#include "face_animator.h"
#include "lvgl.h"
#include <string.h>

/* ── Component lerp functions ────────────────────────────── */

static void face_params_lerp(const face_params_t *a, const face_params_t *b,
                             float t, face_params_t *out) {
    out->roundness = a->roundness + (b->roundness - a->roundness) * t;
}

static void brow_params_lerp(const brow_params_t *a, const brow_params_t *b,
                             float t, brow_params_t *out) {
    out->inner.dx = a->inner.dx + (b->inner.dx - a->inner.dx) * t;
    out->inner.dy = a->inner.dy + (b->inner.dy - a->inner.dy) * t;
    out->arch.dx = a->arch.dx + (b->arch.dx - a->arch.dx) * t;
    out->arch.dy = a->arch.dy + (b->arch.dy - a->arch.dy) * t;
    out->tail.dx = a->tail.dx + (b->tail.dx - a->tail.dx) * t;
    out->tail.dy = a->tail.dy + (b->tail.dy - a->tail.dy) * t;
    out->thickness = a->thickness + (b->thickness - a->thickness) * t;
}

static void eye_params_lerp(const eye_params_t *a, const eye_params_t *b,
                            float t, eye_params_t *out) {
    out->inner_corner.dx = a->inner_corner.dx + (b->inner_corner.dx - a->inner_corner.dx) * t;
    out->inner_corner.dy = a->inner_corner.dy + (b->inner_corner.dy - a->inner_corner.dy) * t;
    out->outer_corner.dx = a->outer_corner.dx + (b->outer_corner.dx - a->outer_corner.dx) * t;
    out->outer_corner.dy = a->outer_corner.dy + (b->outer_corner.dy - a->outer_corner.dy) * t;
    out->top_lid_mid.dx = a->top_lid_mid.dx + (b->top_lid_mid.dx - a->top_lid_mid.dx) * t;
    out->top_lid_mid.dy = a->top_lid_mid.dy + (b->top_lid_mid.dy - a->top_lid_mid.dy) * t;
    out->bot_lid_mid.dx = a->bot_lid_mid.dx + (b->bot_lid_mid.dx - a->bot_lid_mid.dx) * t;
    out->bot_lid_mid.dy = a->bot_lid_mid.dy + (b->bot_lid_mid.dy - a->bot_lid_mid.dy) * t;
    out->iris_center.dx = a->iris_center.dx + (b->iris_center.dx - a->iris_center.dx) * t;
    out->iris_center.dy = a->iris_center.dy + (b->iris_center.dy - a->iris_center.dy) * t;
    out->pupil_scale = a->pupil_scale + (b->pupil_scale - a->pupil_scale) * t;
    out->shine_intensity = a->shine_intensity + (b->shine_intensity - a->shine_intensity) * t;
}

static void mouth_params_lerp(const mouth_params_t *a, const mouth_params_t *b,
                              float t, mouth_params_t *out) {
    out->left_corner.dx = a->left_corner.dx + (b->left_corner.dx - a->left_corner.dx) * t;
    out->left_corner.dy = a->left_corner.dy + (b->left_corner.dy - a->left_corner.dy) * t;
    out->right_corner.dx = a->right_corner.dx + (b->right_corner.dx - a->right_corner.dx) * t;
    out->right_corner.dy = a->right_corner.dy + (b->right_corner.dy - a->right_corner.dy) * t;
    out->upper_lip_mid.dx = a->upper_lip_mid.dx + (b->upper_lip_mid.dx - a->upper_lip_mid.dx) * t;
    out->upper_lip_mid.dy = a->upper_lip_mid.dy + (b->upper_lip_mid.dy - a->upper_lip_mid.dy) * t;
    out->lower_lip_mid.dx = a->lower_lip_mid.dx + (b->lower_lip_mid.dx - a->lower_lip_mid.dx) * t;
    out->lower_lip_mid.dy = a->lower_lip_mid.dy + (b->lower_lip_mid.dy - a->lower_lip_mid.dy) * t;
    out->openness = a->openness + (b->openness - a->openness) * t;
}

static void decor_params_lerp(const decor_params_t *a, const decor_params_t *b,
                              float t, decor_params_t *out) {
    out->blush = a->blush + (b->blush - a->blush) * t;
    out->tears = a->tears + (b->tears - a->tears) * t;
    out->stars = a->stars + (b->stars - a->stars) * t;
    out->sweat = a->sweat + (b->sweat - a->sweat) * t;
    out->sparkle = a->sparkle + (b->sparkle - a->sparkle) * t;
}

typedef void (*lerp_func_t)(const void *a, const void *b, float t, void *out);

static lerp_func_t lerp_funcs[COMPONENT_COUNT] = {
    [COMPONENT_FACE]        = (lerp_func_t)face_params_lerp,
    [COMPONENT_BROW_LEFT]   = (lerp_func_t)brow_params_lerp,
    [COMPONENT_BROW_RIGHT]  = (lerp_func_t)brow_params_lerp,
    [COMPONENT_EYE_LEFT]    = (lerp_func_t)eye_params_lerp,
    [COMPONENT_EYE_RIGHT]   = (lerp_func_t)eye_params_lerp,
    [COMPONENT_MOUTH]       = (lerp_func_t)mouth_params_lerp,
    [COMPONENT_DECOR]       = (lerp_func_t)decor_params_lerp,
};

static const size_t param_sizes[COMPONENT_COUNT] = {
    [COMPONENT_FACE]        = sizeof(face_params_t),
    [COMPONENT_BROW_LEFT]   = sizeof(brow_params_t),
    [COMPONENT_BROW_RIGHT]  = sizeof(brow_params_t),
    [COMPONENT_EYE_LEFT]    = sizeof(eye_params_t),
    [COMPONENT_EYE_RIGHT]   = sizeof(eye_params_t),
    [COMPONENT_MOUTH]       = sizeof(mouth_params_t),
    [COMPONENT_DECOR]       = sizeof(decor_params_t),
};

/* Returns pointer to component params within face_state_t */
static void *get_component_ptr(face_state_t *st, face_component_t comp) {
    switch (comp) {
        case COMPONENT_FACE:        return &st->face;
        case COMPONENT_BROW_LEFT:   return &st->brow[0];
        case COMPONENT_BROW_RIGHT:  return &st->brow[1];
        case COMPONENT_EYE_LEFT:    return &st->eye[0];
        case COMPONENT_EYE_RIGHT:   return &st->eye[1];
        case COMPONENT_MOUTH:       return &st->mouth;
        case COMPONENT_DECOR:       return &st->decor;
        default: return NULL;
    }
}

/* ── Animation state ─────────────────────────────────────── */

static face_state_t current_state;
static lv_anim_t anims[COMPONENT_COUNT];
static bool anim_active[COMPONENT_COUNT];

// "From" states for each component (snapshot at animation start)
static uint8_t from_states[COMPONENT_COUNT][sizeof(eye_params_t)]; // max size

/* ── LVGL path callback bridge ────────────────────────────── */

static lv_anim_path_cb_t path_map(anim_path_type_t type) {
    switch (type) {
        case PATH_LINEAR:       return lv_anim_path_linear;
        case PATH_EASE_OUT:     return lv_anim_path_ease_out;
        case PATH_EASE_IN:      return lv_anim_path_ease_in;
        case PATH_EASE_IN_OUT:  return lv_anim_path_ease_in_out;
        case PATH_OVERSPEED:    return lv_anim_path_overshoot;
        default:                return lv_anim_path_ease_out;
    }
}

/* ── Per-animation callback: lerp from→target based on progress ─ */

typedef struct {
    face_component_t comp;
    uint8_t target[sizeof(eye_params_t)];
} anim_user_data_t;

static anim_user_data_t anim_data[COMPONENT_COUNT];

static void anim_exec_cb(void *var, int32_t value) {
    int idx = -1;
    for (int i = 0; i < COMPONENT_COUNT; i++) {
        if (var == get_component_ptr(&current_state, (face_component_t)i)) {
            idx = i; break;
        }
    }
    if (idx < 0) return;
    float t = value / 1024.0f;
    if (t < 0) t = 0;
    if (t > 1) t = 1;

    face_component_t comp = (face_component_t)idx;
    lerp_funcs[comp](from_states[comp], anim_data[comp].target, t,
                     get_component_ptr(&current_state, comp));
}

static void anim_ready_cb(lv_anim_t *a) {
    for (int i = 0; i < COMPONENT_COUNT; i++) {
        if (&anims[i] == a) {
            anim_active[i] = false;
            break;
        }
    }
}

/* ── Public API ───────────────────────────────────────────── */

void animator_init(void) {
    current_state = FACE_STATE_NEUTRAL;
    memset(anim_active, 0, sizeof(anim_active));
    memset(anims, 0, sizeof(anims));
}

void animator_tick(void) {
    lv_timer_handler();
}

void animator_set_expression(expression_id_t id) {
    if (id >= EXPRESSION_COUNT) return;
    const expression_def_t *expr = &EXPRESSION_DEFS[id];

    for (int i = 0; i < COMPONENT_COUNT; i++) {
        if (anim_active[i]) {
            lv_anim_del(get_component_ptr(&current_state, (face_component_t)i), anim_exec_cb);
            anim_active[i] = false;
        }

        size_t sz = param_sizes[i];
        void *src = get_component_ptr(&current_state, (face_component_t)i);
        memcpy(from_states[i], src, sz);

        const void *target = get_component_ptr((face_state_t *)&expr->target, (face_component_t)i);
        memcpy(anim_data[i].target, target, sz);
        anim_data[i].comp = (face_component_t)i;

        lv_anim_init(&anims[i]);
        lv_anim_set_var(&anims[i], src);
        lv_anim_set_values(&anims[i], 0, 1024);
        lv_anim_set_time(&anims[i], expr->timing[i].duration_ms);
        lv_anim_set_delay(&anims[i], expr->timing[i].delay_ms);
        lv_anim_set_path_cb(&anims[i], path_map(expr->timing[i].path_type));
        lv_anim_set_exec_cb(&anims[i], anim_exec_cb);
        lv_anim_set_ready_cb(&anims[i], anim_ready_cb);
        lv_anim_start(&anims[i]);
        anim_active[i] = true;
    }
}

void animator_set_component(face_component_t comp,
                            const void *target_params,
                            uint32_t duration_ms,
                            uint32_t delay_ms,
                            anim_path_type_t path_type) {
    if (comp >= COMPONENT_COUNT) return;
    int i = comp;

    if (anim_active[i]) {
        lv_anim_del(get_component_ptr(&current_state, comp), anim_exec_cb);
        anim_active[i] = false;
    }

    size_t sz = param_sizes[i];
    void *src = get_component_ptr(&current_state, comp);
    memcpy(from_states[i], src, sz);
    memcpy(anim_data[i].target, target_params, sz);

    lv_anim_init(&anims[i]);
    lv_anim_set_var(&anims[i], src);
    lv_anim_set_values(&anims[i], 0, 1024);
    lv_anim_set_time(&anims[i], duration_ms);
    lv_anim_set_delay(&anims[i], delay_ms);
    lv_anim_set_path_cb(&anims[i], path_map(path_type));
    lv_anim_set_exec_cb(&anims[i], anim_exec_cb);
    lv_anim_set_ready_cb(&anims[i], anim_ready_cb);
    lv_anim_start(&anims[i]);
    anim_active[i] = true;
}

void animator_set_component_instant(face_component_t comp, const void *target_params) {
    if (comp >= COMPONENT_COUNT) return;
    int i = comp;
    if (anim_active[i]) {
        lv_anim_del(get_component_ptr(&current_state, comp), anim_exec_cb);
        anim_active[i] = false;
    }
    void *dst = get_component_ptr(&current_state, comp);
    memcpy(dst, target_params, param_sizes[i]);
}

const face_state_t *animator_get_state(void) {
    return &current_state;
}

void animator_cancel_all(void) {
    for (int i = 0; i < COMPONENT_COUNT; i++) {
        if (anim_active[i]) {
            lv_anim_del(get_component_ptr(&current_state, (face_component_t)i), anim_exec_cb);
            anim_active[i] = false;
        }
    }
}
