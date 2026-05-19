#include "face_model.h"
#include <string.h>

/* ── Neutral component states ────────────────────────────── */

static const brow_params_t BROW_NEUTRAL = {
    .inner = {0, 0}, .arch = {0, 0}, .tail = {0, 0},
    .thickness = 1.0f,
};

static const eye_params_t EYE_NEUTRAL = {
    .inner_corner = {0, 0}, .outer_corner = {0, 0},
    .top_lid_mid = {0, 0}, .bot_lid_mid = {0, 0},
    .iris_center = {0, 0},
    .pupil_scale = 0.6f, .shine_intensity = 0.88f,
};

static const mouth_params_t MOUTH_NEUTRAL = {
    .left_corner = {0, 0}, .right_corner = {0, 0},
    .upper_lip_mid = {0, 0}, .lower_lip_mid = {0, 0},
    .openness = 0.0f,
};

static const face_params_t FACE_NEUTRAL = {
    .roundness = 0.5f,
};

static const decor_params_t DECOR_NEUTRAL = {0};

const face_state_t FACE_STATE_NEUTRAL = {
    .face = {.roundness = 0.5f},
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
            .face = {.roundness = 0.5f},
            .brow = {BROW_NEUTRAL, BROW_NEUTRAL},
            .eye = {EYE_NEUTRAL, EYE_NEUTRAL},
            .mouth = MOUTH_NEUTRAL,
            .decor = DECOR_NEUTRAL,
        },
        .timing = {
            [COMPONENT_FACE]       = {300, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {300, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {300, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {300, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {300, 0, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {300, 50, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {300, 100, PATH_EASE_OUT},
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
                 .top_lid_mid = {0, -0.08f}, .bot_lid_mid = {0, 0.12f},
                 .iris_center = {0, -0.1f},
                 .pupil_scale = 0.65f, .shine_intensity = 0.9f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.08f}, .bot_lid_mid = {0, 0.12f},
                 .iris_center = {0, -0.1f},
                 .pupil_scale = 0.65f, .shine_intensity = 0.9f},
            },
            .mouth = {
                .left_corner = {0.2f, 0}, .right_corner = {-0.2f, 0},
                .upper_lip_mid = {0, -0.15f}, .lower_lip_mid = {0, 0.05f},
                .openness = 0.0f,
            },
            .decor = {.blush = 0.55f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {250, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {250, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {250, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {200, 30, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {200, 30, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {350, 120, PATH_OVERSPEED},
            [COMPONENT_DECOR]      = {300, 80, PATH_EASE_OUT},
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
                 .pupil_scale = 0.55f, .shine_intensity = 0.5f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.18f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.3f},
                 .pupil_scale = 0.55f, .shine_intensity = 0.5f},
            },
            .mouth = {
                .left_corner = {0.05f, 0.15f}, .right_corner = {-0.05f, 0.15f},
                .upper_lip_mid = {0, 0.1f}, .lower_lip_mid = {0, 0.15f},
                .openness = 0.1f,
            },
            .decor = {.blush = 0.2f, .tears = 0.6f, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {400, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {400, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {400, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {350, 50, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_RIGHT]  = {350, 50, PATH_EASE_IN_OUT},
            [COMPONENT_MOUTH]      = {500, 150, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {400, 200, PATH_EASE_IN},
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
                 .pupil_scale = 0.45f, .shine_intensity = 1.0f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.15f}, .bot_lid_mid = {0, 0.15f},
                 .iris_center = {0, 0},
                 .pupil_scale = 0.45f, .shine_intensity = 1.0f},
            },
            .mouth = {
                .left_corner = {0, 0.1f}, .right_corner = {0, 0.1f},
                .upper_lip_mid = {0, -0.1f}, .lower_lip_mid = {0, 0.25f},
                .openness = 0.5f,
            },
            .decor = {.blush = 0.15f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0.2f},
        },
        .timing = {
            [COMPONENT_FACE]       = {150, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {120, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {120, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {120, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {120, 0, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {180, 80, PATH_OVERSPEED},
            [COMPONENT_DECOR]      = {200, 50, PATH_EASE_OUT},
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
                 .pupil_scale = 0.5f, .shine_intensity = 0.3f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.3f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.2f},
                 .pupil_scale = 0.5f, .shine_intensity = 0.3f},
            },
            .mouth = {
                .left_corner = {0, 0.05f}, .right_corner = {0, 0.05f},
                .upper_lip_mid = {0, 0.05f}, .lower_lip_mid = {0, 0.0f},
                .openness = 0.2f,
            },
            .decor = {.blush = 0.1f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {600, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {600, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {600, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {500, 30, PATH_EASE_IN},
            [COMPONENT_EYE_RIGHT]  = {500, 30, PATH_EASE_IN},
            [COMPONENT_MOUTH]      = {600, 100, PATH_EASE_IN_OUT},
            [COMPONENT_DECOR]      = {500, 50, PATH_EASE_IN_OUT},
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
                 .pupil_scale = 0.55f, .shine_intensity = 0.7f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.05f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.7f},
            },
            .mouth = {
                .left_corner = {0.1f, 0.1f}, .right_corner = {-0.1f, 0.1f},
                .upper_lip_mid = {0, 0.05f}, .lower_lip_mid = {0, 0.1f},
                .openness = 0.0f,
            },
            .decor = {.blush = 0, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {200, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {200, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {200, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {180, 20, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {180, 20, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {250, 80, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {200, 0, PATH_EASE_OUT},
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
                 .pupil_scale = 0.55f, .shine_intensity = 0.5f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.2f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, -0.2f},
                 .pupil_scale = 0.55f, .shine_intensity = 0.5f},
            },
            .mouth = {
                .left_corner = {0.05f, 0.05f}, .right_corner = {-0.05f, 0.05f},
                .upper_lip_mid = {0, 0.05f}, .lower_lip_mid = {0, 0.02f},
                .openness = 0.05f,
            },
            .decor = {.blush = 0, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {500, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {500, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {500, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {450, 40, PATH_EASE_IN},
            [COMPONENT_EYE_RIGHT]  = {450, 40, PATH_EASE_IN},
            [COMPONENT_MOUTH]      = {500, 100, PATH_EASE_IN_OUT},
            [COMPONENT_DECOR]      = {400, 0, PATH_EASE_OUT},
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
                 .top_lid_mid = {0, -0.1f}, .bot_lid_mid = {0, 0.1f},
                 .iris_center = {0, -0.1f},
                 .pupil_scale = 0.7f, .shine_intensity = 1.0f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.1f}, .bot_lid_mid = {0, 0.1f},
                 .iris_center = {0, -0.1f},
                 .pupil_scale = 0.7f, .shine_intensity = 1.0f},
            },
            .mouth = {
                .left_corner = {0.25f, -0.1f}, .right_corner = {-0.25f, -0.1f},
                .upper_lip_mid = {0, -0.2f}, .lower_lip_mid = {0, 0.1f},
                .openness = 0.3f,
            },
            .decor = {.blush = 0.4f, .tears = 0, .stars = 0.65f, .sweat = 0, .sparkle = 0.3f},
        },
        .timing = {
            [COMPONENT_FACE]       = {180, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {150, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {150, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {150, 20, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {150, 20, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {200, 60, PATH_OVERSPEED},
            [COMPONENT_DECOR]      = {200, 30, PATH_EASE_OUT},
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
                 .top_lid_mid = {0, 0.1f}, .bot_lid_mid = {0, 0},
                 .iris_center = {0.3f, 0.1f},
                 .pupil_scale = 0.5f, .shine_intensity = 0.6f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.05f}, .bot_lid_mid = {0, 0.08f},
                 .iris_center = {0.3f, 0.1f},
                 .pupil_scale = 0.5f, .shine_intensity = 0.6f},
            },
            .mouth = {
                .left_corner = {0, 0.05f}, .right_corner = {-0.15f, 0.05f},
                .upper_lip_mid = {0, 0.05f}, .lower_lip_mid = {0, 0.02f},
                .openness = 0.1f,
            },
            .decor = {.blush = 0.1f, .tears = 0, .stars = 0, .sweat = 0.2f, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {300, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {300, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {300, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {250, 30, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {250, 30, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {350, 120, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {300, 50, PATH_EASE_OUT},
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
                 .top_lid_mid = {0, 0.28f}, .bot_lid_mid = {0, 0.02f},
                 .iris_center = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.4f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.28f}, .bot_lid_mid = {0, 0.02f},
                 .iris_center = {0, 0},
                 .pupil_scale = 0.55f, .shine_intensity = 0.4f},
            },
            .mouth = {
                .left_corner = {0.15f, -0.05f}, .right_corner = {-0.15f, -0.05f},
                .upper_lip_mid = {0, -0.1f}, .lower_lip_mid = {0, 0.02f},
                .openness = 0.0f,
            },
            .decor = {.blush = 0.75f, .tears = 0, .stars = 0.2f, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {500, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {500, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {500, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {450, 50, PATH_EASE_IN},
            [COMPONENT_EYE_RIGHT]  = {450, 50, PATH_EASE_IN},
            [COMPONENT_MOUTH]      = {500, 100, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {450, 150, PATH_EASE_IN_OUT},
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
                 .iris_center = {0, 0.2f},
                 .pupil_scale = 0.4f, .shine_intensity = 0.3f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.2f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0.2f},
                 .pupil_scale = 0.4f, .shine_intensity = 0.3f},
            },
            .mouth = {
                .left_corner = {0, 0.05f}, .right_corner = {0, 0.05f},
                .upper_lip_mid = {0, 0.02f}, .lower_lip_mid = {0, 0.08f},
                .openness = 0.0f,
            },
            .decor = {.blush = 0.3f, .tears = 0, .stars = 0, .sweat = 0, .sparkle = 0},
        },
        .timing = {
            [COMPONENT_FACE]       = {400, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_LEFT]  = {400, 0, PATH_EASE_IN_OUT},
            [COMPONENT_BROW_RIGHT] = {400, 0, PATH_EASE_IN_OUT},
            [COMPONENT_EYE_LEFT]   = {350, 40, PATH_EASE_IN},
            [COMPONENT_EYE_RIGHT]  = {350, 40, PATH_EASE_IN},
            [COMPONENT_MOUTH]      = {400, 100, PATH_EASE_IN_OUT},
            [COMPONENT_DECOR]      = {350, 60, PATH_EASE_IN_OUT},
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
                 .top_lid_mid = {0, -0.03f}, .bot_lid_mid = {0, 0.1f},
                 .iris_center = {0, -0.15f},
                 .pupil_scale = 0.65f, .shine_intensity = 0.85f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, -0.03f}, .bot_lid_mid = {0, 0.1f},
                 .iris_center = {0, -0.15f},
                 .pupil_scale = 0.65f, .shine_intensity = 0.85f},
            },
            .mouth = {
                .left_corner = {0.18f, -0.08f}, .right_corner = {-0.18f, -0.08f},
                .upper_lip_mid = {0, -0.15f}, .lower_lip_mid = {0, 0.02f},
                .openness = 0.05f,
            },
            .decor = {.blush = 0.5f, .tears = 0, .stars = 0.3f, .sweat = 0, .sparkle = 0.1f},
        },
        .timing = {
            [COMPONENT_FACE]       = {350, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {300, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {300, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {280, 30, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {280, 30, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {350, 80, PATH_EASE_OUT},
            [COMPONENT_DECOR]      = {300, 50, PATH_EASE_OUT},
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
                 .pupil_scale = 0.0f, .shine_intensity = 0.9f},
                {.inner_corner = {0, 0}, .outer_corner = {0, 0},
                 .top_lid_mid = {0, 0.05f}, .bot_lid_mid = {0, 0.05f},
                 .iris_center = {0, 0},
                 .pupil_scale = 0.0f, .shine_intensity = 0.9f},
            },
            .mouth = {
                .left_corner = {0.2f, -0.1f}, .right_corner = {-0.2f, -0.1f},
                .upper_lip_mid = {0, -0.2f}, .lower_lip_mid = {0, 0.05f},
                .openness = 0.15f,
            },
            .decor = {.blush = 0.65f, .tears = 0, .stars = 0.9f, .sweat = 0, .sparkle = 0.5f},
        },
        .timing = {
            [COMPONENT_FACE]       = {250, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_LEFT]  = {200, 0, PATH_EASE_OUT},
            [COMPONENT_BROW_RIGHT] = {200, 0, PATH_EASE_OUT},
            [COMPONENT_EYE_LEFT]   = {200, 20, PATH_EASE_OUT},
            [COMPONENT_EYE_RIGHT]  = {200, 20, PATH_EASE_OUT},
            [COMPONENT_MOUTH]      = {280, 60, PATH_OVERSPEED},
            [COMPONENT_DECOR]      = {250, 30, PATH_EASE_OUT},
        },
    },
};

const uint8_t EXPRESSION_COUNT = sizeof(EXPRESSION_DEFS) / sizeof(EXPRESSION_DEFS[0]);

/* ── Classic sprite set (defined in sprite_classic.c) ────── */
extern const sprite_set_t SPRITE_CLASSIC;
