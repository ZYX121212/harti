#include "face_model.h"
#include <string.h>

/* ── Neutral component states ────────────────────────────── */

static const brow_params_t BROW_NEUTRAL = {
    .inner = {0, 0}, .arch = {0, 0}, .tail = {0, 0},
    .thickness = 1.0f,
    .taper = 0.6f,
};

static const eye_params_t EYE_NEUTRAL = {
    .inner_corner = {0, 0}, .outer_corner = {0, 0},
    .top_lid_mid = {0, -0.06f}, .bot_lid_mid = {0, 0.08f},
    .iris_center = {0, -0.06f},
    .position = {0, 0},
    .pupil_scale = 0.68f, .shine_intensity = 1.0f,
    .iris_detail = 0.5f, .eyelash = 0.6f,
};

static const mouth_params_t MOUTH_NEUTRAL = {
    .left_corner = {0.08f, -0.04f}, .right_corner = {-0.08f, -0.04f},
    .upper_lip_mid = {0, -0.08f}, .lower_lip_mid = {0, 0.02f},
    .openness = 0.0f, .cupid_depth = 0.4f,
};

static const decor_params_t DECOR_NEUTRAL = {
    .blush = 0.0f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0,
    .prop_count = 0,
};

const face_state_t FACE_STATE_NEUTRAL = {
    .face = {.roundness = 0.66f},
    .brow = {BROW_NEUTRAL, BROW_NEUTRAL},
    .eye  = {EYE_NEUTRAL, EYE_NEUTRAL},
    .mouth = MOUTH_NEUTRAL,
    .decor = DECOR_NEUTRAL,
};

/* ── Expression presets (13 emotions) ────────────────────── */

const expression_def_t EXPRESSION_DEFS[] = {
    // [0] NEUTRAL
    {
        .name = "NEUTRAL",
        .target = {
            .face = {.roundness = 0.66f},
            .brow = {BROW_NEUTRAL, BROW_NEUTRAL},
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0}, .bot_lid_mid = {0, 0},
                 .iris_center = {0, 0},
                 .position = {0, 0},
                 .pupil_scale = 0.6f, .shine_intensity = 0.88f,
                 .iris_detail = 0.5f, .eyelash = 0.6f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0}, .bot_lid_mid = {0, 0},
                 .iris_center = {0, 0},
                 .position = {0, 0},
                 .pupil_scale = 0.6f, .shine_intensity = 0.88f,
                 .iris_detail = 0.5f, .eyelash = 0.6f},
            },
            .mouth = {
                .left_corner = {0, 0}, .right_corner = {0, 0},
                .upper_lip_mid = {0, 0}, .lower_lip_mid = {0, 0},
                .openness = 0.0f, .cupid_depth = 0.4f,
            },
            .decor = DECOR_NEUTRAL,
        },
        .timing = {
            [COMPONENT_FACE]       = {150, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {150, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {150, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {150, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {150, 0, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {150, 50, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {150, 100, PATH_EASE_OUT},
        },
    },
    // [1] HAPPY
    {
        .name = "HAPPY",
        .target = {
            .face = {.roundness = 0.6f},
            .brow = {
                {.inner = {0, -0.1f}, .arch = {0, -0.45f}, .tail = {0.1f, -0.3f}, .thickness = 1.0f},
                {.inner = {0, -0.1f}, .arch = {0, -0.45f}, .tail = {-0.1f, -0.3f}, .thickness = 1.0f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.10f}, .bot_lid_mid = {0, 0.12f},
                 .iris_center = {0, -0.1f},
                 .position = {0, 0},
                 .pupil_scale = 0.65f, .shine_intensity = 0.9f,
                 .iris_detail = 0.6f, .eyelash = 0.5f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.10f}, .bot_lid_mid = {0, 0.12f},
                 .iris_center = {0, -0.1f},
                 .position = {0, 0},
                 .pupil_scale = 0.65f, .shine_intensity = 0.9f,
                 .iris_detail = 0.6f, .eyelash = 0.5f},
            },
            .mouth = {
                .left_corner = {0.2f, 0}, .right_corner = {-0.2f, 0},
                .upper_lip_mid = {0, -0.15f}, .lower_lip_mid = {0, 0.05f},
                .openness = 0.0f, .cupid_depth = 0.7f, .tooth_show = 0.6f,
            },
            .decor = {.blush = 0.0f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {125, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {125, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {125, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {100, 30, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {100, 30, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {175, 120, PATH_OVERSPEED},
            [COMPONENT_DECOR]      = {150, 80, PATH_EASE_OUT},
        },
    },
    // [2] SAD
    {
        .name = "SAD",
        .target = {
            .face = {.roundness = 0.4f},
            .brow = {
                {.inner = {0, -0.1f}, .arch = {0.05f, 0}, .tail = {0.1f, 0.35f}, .thickness = 1.0f},
                {.inner = {0, -0.1f}, .arch = {-0.05f, 0}, .tail = {-0.1f, 0.35f}, .thickness = 1.0f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.18f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.3f},
                 .position = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.5f,
                 .iris_detail = 0.3f, .eyelash = 0.7f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.18f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.3f},
                 .position = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.5f,
                 .iris_detail = 0.3f, .eyelash = 0.7f},
            },
            .mouth = {
                .left_corner = {0.05f, 0.15f}, .right_corner = {-0.05f, 0.15f},
                .upper_lip_mid = {0, 0.1f}, .lower_lip_mid = {0, 0.15f},
                .openness = 0.1f, .cupid_depth = 0.1f,
            },
            .decor = {.blush = 0.0f, .tears = 0.6f, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {200, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {200, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {200, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {175, 50, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_RIGHT]  = {175, 50, PATH_EASE_IN_OUT},
            [COMPONENT_MOUTH]      = {250, 150, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {200, 200, PATH_EASE_IN},
        },
    },
    // [3] SURPRISED
    {
        .name = "SURPRISED",
        .target = {
            .face = {.roundness = 0.65f},
            .brow = {
                {.inner = {0, -0.5f}, .arch = {0, -0.7f}, .tail = {0, -0.55f}, .thickness = 0.9f},
                {.inner = {0, -0.5f}, .arch = {0, -0.7f}, .tail = {0, -0.55f}, .thickness = 0.9f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.15f}, .bot_lid_mid = {0, 0.15f},
                 .iris_center = {0, 0},
                 .position = {0, 0},
                 .pupil_scale = 0.45f, .shine_intensity = 1.0f,
                 .iris_detail = 0.8f, .eyelash = 0.3f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.15f}, .bot_lid_mid = {0, 0.15f},
                 .iris_center = {0, 0},
                 .position = {0, 0},
                 .pupil_scale = 0.45f, .shine_intensity = 1.0f,
                 .iris_detail = 0.8f, .eyelash = 0.3f},
            },
            .mouth = {
                .left_corner = {0, 0.1f}, .right_corner = {0, 0.1f},
                .upper_lip_mid = {0, -0.1f}, .lower_lip_mid = {0, 0.25f},
                .openness = 0.5f, .cupid_depth = 0.3f,
            },
            .decor = {.blush = 0.0f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {100, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {100, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {100, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {100, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {100, 0, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {100, 80, PATH_OVERSPEED},
            [COMPONENT_DECOR]      = {100, 50, PATH_EASE_OUT},
        },
    },
    // [4] SLEEPY
    {
        .name = "SLEEPY",
        .target = {
            .face = {.roundness = 0.5f},
            .brow = {
                {.inner = {0, 0.05f}, .arch = {0, 0.15f}, .tail = {0, 0.1f}, .thickness = 1.0f},
                {.inner = {0, 0.05f}, .arch = {0, 0.15f}, .tail = {0, 0.1f}, .thickness = 1.0f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.3f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.2f},
                 .position = {0, 0},
                 .pupil_scale = 0.5f, .shine_intensity = 0.3f,
                 .iris_detail = 0.2f, .eyelash = 0.8f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.3f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.2f},
                 .position = {0, 0},
                 .pupil_scale = 0.5f, .shine_intensity = 0.3f,
                 .iris_detail = 0.2f, .eyelash = 0.8f},
            },
            .mouth = {
                .left_corner = {0, 0.05f}, .right_corner = {0, 0.05f},
                .upper_lip_mid = {0, 0.05f}, .lower_lip_mid = {0, 0.0f},
                .openness = 0.2f, .cupid_depth = 0.3f,
            },
            .decor = {.blush = 0.0f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {300, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {300, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {300, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {250, 30, PATH_EASE_IN},
            [COMPONENT_EYE_RIGHT]  = {250, 30, PATH_EASE_IN},
            [COMPONENT_MOUTH]      = {300, 100, PATH_EASE_IN_OUT},
            [COMPONENT_DECOR]      = {250, 50, PATH_EASE_IN_OUT},
        },
    },
    // [5] ANGRY
    {
        .name = "ANGRY",
        .target = {
            .face = {.roundness = 0.4f},
            .brow = {
                {.inner = {0.05f, -0.15f}, .arch = {0, -0.3f}, .tail = {-0.1f, 0.25f}, .thickness = 1.3f},
                {.inner = {-0.05f, -0.15f}, .arch = {0, -0.3f}, .tail = {0.1f, 0.25f}, .thickness = 1.3f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.05f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0},
                 .position = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.7f,
                 .iris_detail = 0.6f, .eyelash = 0.65f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.05f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0},
                 .position = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.7f,
                 .iris_detail = 0.6f, .eyelash = 0.65f},
            },
            .mouth = {
                .left_corner = {0.1f, 0.1f}, .right_corner = {-0.1f, 0.1f},
                .upper_lip_mid = {0, 0.05f}, .lower_lip_mid = {0, 0.1f},
                .openness = 0.0f, .cupid_depth = 0.5f,
            },
            .decor = {.blush = 0.0f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {100, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {100, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {100, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {100, 20, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {100, 20, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {125, 80, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {100, 0, PATH_EASE_OUT},
        },
    },
    // [6] BORED
    {
        .name = "BORED",
        .target = {
            .face = {.roundness = 0.5f},
            .brow = {
                {.inner = {0, 0}, .arch = {0, 0.15f}, .tail = {0, 0.1f}, .thickness = 0.9f},
                {.inner = {0, 0}, .arch = {0, 0.15f}, .tail = {0, 0.1f}, .thickness = 0.9f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.2f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, -0.2f},
                 .position = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.5f,
                 .iris_detail = 0.3f, .eyelash = 0.7f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.2f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, -0.2f},
                 .position = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.5f,
                 .iris_detail = 0.3f, .eyelash = 0.7f},
            },
            .mouth = {
                .left_corner = {0.05f, 0.05f}, .right_corner = {-0.05f, 0.05f},
                .upper_lip_mid = {0, 0.05f}, .lower_lip_mid = {0, 0.02f},
                .openness = 0.05f, .cupid_depth = 0.35f,
            },
            .decor = {.blush = 0.0f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {250, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {250, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {250, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {225, 40, PATH_EASE_IN},
            [COMPONENT_EYE_RIGHT]  = {225, 40, PATH_EASE_IN},
            [COMPONENT_MOUTH]      = {250, 100, PATH_EASE_IN_OUT},
            [COMPONENT_DECOR]      = {200, 0, PATH_EASE_OUT},
        },
    },
    // [7] EXCITED
    {
        .name = "EXCITED",
        .target = {
            .face = {.roundness = 0.7f},
            .brow = {
                {.inner = {0, -0.25f}, .arch = {0, -0.5f}, .tail = {0.05f, -0.35f}, .thickness = 0.85f},
                {.inner = {0, -0.25f}, .arch = {0, -0.5f}, .tail = {-0.05f, -0.35f}, .thickness = 0.85f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.12f}, .bot_lid_mid = {0, 0.1f},
                 .iris_center = {0, -0.1f},
                 .position = {0, 0},
                 .pupil_scale = 0.45f, .shine_intensity = 1.0f,
                 .iris_detail = 0.8f, .eyelash = 0.4f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.12f}, .bot_lid_mid = {0, 0.1f},
                 .iris_center = {0, -0.1f},
                 .position = {0, 0},
                 .pupil_scale = 0.45f, .shine_intensity = 1.0f,
                 .iris_detail = 0.8f, .eyelash = 0.4f},
            },
            .mouth = {
                .left_corner = {0.25f, -0.1f}, .right_corner = {-0.25f, -0.1f},
                .upper_lip_mid = {0, -0.2f}, .lower_lip_mid = {0, 0.1f},
                .openness = 0.3f, .cupid_depth = 0.65f, .tooth_show = 0.5f,
            },
            .decor = {.blush = 0.0f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {100, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {100, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {100, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {100, 20, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {100, 20, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {100, 60, PATH_OVERSPEED},
            [COMPONENT_DECOR]      = {100, 30, PATH_EASE_OUT},
        },
    },
    // [8] CONFUSED
    {
        .name = "CONFUSED",
        .target = {
            .face = {.roundness = 0.5f},
            .brow = {
                {.inner = {0, -0.15f}, .arch = {0, -0.25f}, .tail = {0, 0}, .thickness = 1.0f},
                {.inner = {0, 0}, .arch = {0, 0.1f}, .tail = {0, 0.2f}, .thickness = 1.0f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.12f}, .bot_lid_mid = {0, 0},
                 .iris_center = {0.28f, -0.22f},
                 .position = {0, 0},
                 .pupil_scale = 0.48f, .shine_intensity = 0.55f,
                 .iris_detail = 0.4f, .eyelash = 0.6f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.06f}, .bot_lid_mid = {0, 0.1f},
                 .iris_center = {0.28f, -0.22f},
                 .position = {0, 0},
                 .pupil_scale = 0.48f, .shine_intensity = 0.55f,
                 .iris_detail = 0.4f, .eyelash = 0.6f},
            },
            .mouth = {
                .left_corner = {0.22f, 0.02f}, .right_corner = {0.0f, 0.08f},
                .upper_lip_mid = {0.05f, 0.05f}, .lower_lip_mid = {0, 0.02f},
                .openness = 0.08f, .cupid_depth = 0.35f,
            },
            .decor = {.blush = 0.0f, .tears = 0, .stars = 0, .sweat = 0.35f, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {150, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {150, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {150, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {125, 30, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {125, 30, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {175, 120, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {150, 50, PATH_EASE_OUT},
        },
    },
    // [9] CONTENT
    {
        .name = "CONTENT",
        .target = {
            .face = {.roundness = 0.6f},
            .brow = {
                {.inner = {0, -0.1f}, .arch = {0, -0.3f}, .tail = {0, -0.15f}, .thickness = 0.9f},
                {.inner = {0, -0.1f}, .arch = {0, -0.3f}, .tail = {0, -0.15f}, .thickness = 0.9f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.05f}, .bot_lid_mid = {0, 0.10f},
                 .iris_center = {0, 0},
                 .position = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.4f,
                 .iris_detail = 0.3f, .eyelash = 0.8f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.05f}, .bot_lid_mid = {0, 0.10f},
                 .iris_center = {0, 0},
                 .position = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.4f,
                 .iris_detail = 0.3f, .eyelash = 0.8f},
            },
            .mouth = {
                .left_corner = {0.15f, -0.05f}, .right_corner = {-0.15f, -0.05f},
                .upper_lip_mid = {0, -0.1f}, .lower_lip_mid = {0, 0.02f},
                .openness = 0.0f, .cupid_depth = 0.5f,
            },
            .decor = {.blush = 0.0f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {250, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {250, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {250, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {225, 50, PATH_EASE_IN},
            [COMPONENT_EYE_RIGHT]  = {225, 50, PATH_EASE_IN},
            [COMPONENT_MOUTH]      = {250, 100, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {225, 150, PATH_EASE_IN_OUT},
        },
    },
    // [10] COLD
    {
        .name = "COLD",
        .target = {
            .face = {.roundness = 0.4f},
            .brow = {
                {.inner = {0, 0.05f}, .arch = {0, 0.2f}, .tail = {0, 0.1f}, .thickness = 1.1f},
                {.inner = {0, 0.05f}, .arch = {0, 0.2f}, .tail = {0, 0.1f}, .thickness = 1.1f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.2f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.28f},
                 .position = {0, 0},
                 .pupil_scale = 0.4f, .shine_intensity = 0.3f,
                 .iris_detail = 0.2f, .eyelash = 0.7f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.2f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.28f},
                 .position = {0, 0},
                 .pupil_scale = 0.4f, .shine_intensity = 0.3f,
                 .iris_detail = 0.2f, .eyelash = 0.7f},
            },
            .mouth = {
                .left_corner = {0, 0.05f}, .right_corner = {0, 0.05f},
                .upper_lip_mid = {0, 0.02f}, .lower_lip_mid = {0, 0.08f},
                .openness = 0.0f, .cupid_depth = 0.4f,
            },
            .decor = {.blush = 0.0f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {200, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {200, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {200, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {175, 40, PATH_EASE_IN},
            [COMPONENT_EYE_RIGHT]  = {175, 40, PATH_EASE_IN},
            [COMPONENT_MOUTH]      = {200, 100, PATH_EASE_IN_OUT},
            [COMPONENT_DECOR]      = {175, 60, PATH_EASE_IN_OUT},
        },
    },
    // [11] WARM
    {
        .name = "WARM",
        .target = {
            .face = {.roundness = 0.65f},
            .brow = {
                {.inner = {0, -0.15f}, .arch = {0, -0.35f}, .tail = {0, -0.2f}, .thickness = 0.9f},
                {.inner = {0, -0.15f}, .arch = {0, -0.35f}, .tail = {0, -0.2f}, .thickness = 0.9f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.14f}, .bot_lid_mid = {0, 0.1f},
                 .iris_center = {0, -0.15f},
                 .position = {0, 0},
                 .pupil_scale = 0.65f, .shine_intensity = 0.85f,
                 .iris_detail = 0.7f, .eyelash = 0.5f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.14f}, .bot_lid_mid = {0, 0.1f},
                 .iris_center = {0, -0.15f},
                 .position = {0, 0},
                 .pupil_scale = 0.65f, .shine_intensity = 0.85f,
                 .iris_detail = 0.7f, .eyelash = 0.5f},
            },
            .mouth = {
                .left_corner = {0.18f, -0.08f}, .right_corner = {-0.18f, -0.08f},
                .upper_lip_mid = {0, -0.15f}, .lower_lip_mid = {0, 0.02f},
                .openness = 0.05f, .cupid_depth = 0.6f,
            },
            .decor = {.blush = 0.0f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {175, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {150, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {150, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {140, 30, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {140, 30, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {175, 80, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {150, 50, PATH_EASE_OUT},
        },
    },
    // [12] HEART_EYES
    {
        .name = "HEART_EYES",
        .target = {
            .face = {.roundness = 0.7f},
            .brow = {
                {.inner = {0, -0.2f}, .arch = {0, -0.45f}, .tail = {0.05f, -0.3f}, .thickness = 0.85f},
                {.inner = {0, -0.2f}, .arch = {0, -0.45f}, .tail = {-0.05f, -0.3f}, .thickness = 0.85f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.05f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0},
                 .position = {0, 0},
                 .pupil_scale = 0.0f, .shine_intensity = 0.9f,
                 .iris_detail = 0.6f, .eyelash = 0.45f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.05f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0},
                 .position = {0, 0},
                 .pupil_scale = 0.0f, .shine_intensity = 0.9f,
                 .iris_detail = 0.6f, .eyelash = 0.45f},
            },
            .mouth = {
                .left_corner = {0.2f, -0.1f}, .right_corner = {-0.2f, -0.1f},
                .upper_lip_mid = {0, -0.2f}, .lower_lip_mid = {0, 0.05f},
                .openness = 0.15f, .cupid_depth = 0.7f,
            },
            .decor = {.blush = 0.0f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {125, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {100, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {100, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {100, 20, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {100, 20, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {140, 60, PATH_OVERSPEED},
            [COMPONENT_DECOR]      = {125, 30, PATH_EASE_OUT},
        },
    },
    // [13] THINKING
    {
        .name = "THINKING",
        .target = {
            .face = {.roundness = 0.58f},
            .brow = {
                {.inner = {0, -0.22f}, .arch = {0, -0.42f}, .tail = {0, -0.05f}, .thickness = 0.88f},
                {.inner = {0, 0.10f}, .arch = {0, 0.15f}, .tail = {0, 0.22f}, .thickness = 1.0f},
            },
            .eye = {
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.11f}, .bot_lid_mid = {0, 0},
                 .iris_center = {0.24f, -0.33f},
                 .position = {0, 0},
                 .pupil_scale = 0.52f, .shine_intensity = 0.6f,
                 .iris_detail = 0.4f, .eyelash = 0.6f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.18f}, .bot_lid_mid = {0, 0.06f},
                 .iris_center = {0.24f, -0.33f},
                 .position = {0, 0},
                 .pupil_scale = 0.45f, .shine_intensity = 0.5f,
                 .iris_detail = 0.35f, .eyelash = 0.65f},
            },
            .mouth = {
                .left_corner = {0.22f, 0.06f}, .right_corner = {-0.02f, 0.0f},
                .upper_lip_mid = {0.06f, 0.02f}, .lower_lip_mid = {0, 0.02f},
                .openness = 0.06f, .cupid_depth = 0.38f,
            },
            .decor = {.blush = 0.0f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {200, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {180, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {180, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {160, 30, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {160, 30, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {200, 80, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {100, 0, PATH_EASE_OUT},
        },
    },
};

const uint8_t EXPRESSION_COUNT = sizeof(EXPRESSION_DEFS) / sizeof(EXPRESSION_DEFS[0]);
