#include "face_palette.h"

/* ── All palettes: black background + white features ──────────── */

const uint16_t PALETTE_WHITE[PAL_COUNT] = {
    [PAL_BG]      = RGB565(0, 0, 0),
    [PAL_BG_EDGE] = RGB565(0, 0, 0),
    [PAL_SCLERA]  = RGB565(255, 255, 255),
    [PAL_IRIS]    = RGB565(255, 255, 255),
    [PAL_PUPIL]   = RGB565(255, 255, 255),
    [PAL_BLUSH]   = RGB565(255, 255, 255),
    [PAL_TEAR]    = RGB565(255, 255, 255),
    [PAL_SHINE]   = RGB565(255, 255, 255),
    [PAL_STAR]    = RGB565(255, 255, 255),
    [PAL_SKIN]    = RGB565(0, 0, 0),
    [PAL_BROW]    = RGB565(255, 255, 255),
    [PAL_MOUTH]   = RGB565(255, 255, 255),
    [PAL_TONGUE]  = RGB565(255, 255, 255),
    [PAL_LIMBAL]  = RGB565(0, 0, 0),
    [PAL_LID]     = RGB565(0, 0, 0),
};

const uint16_t PALETTE_BLACK[PAL_COUNT] = {
    [PAL_BG]      = RGB565(0, 0, 0),
    [PAL_BG_EDGE] = RGB565(0, 0, 0),
    [PAL_SCLERA]  = RGB565(255, 255, 255),
    [PAL_IRIS]    = RGB565(255, 255, 255),
    [PAL_PUPIL]   = RGB565(255, 255, 255),
    [PAL_BLUSH]   = RGB565(255, 255, 255),
    [PAL_TEAR]    = RGB565(255, 255, 255),
    [PAL_SHINE]   = RGB565(255, 255, 255),
    [PAL_STAR]    = RGB565(255, 255, 255),
    [PAL_SKIN]    = RGB565(0, 0, 0),
    [PAL_BROW]    = RGB565(255, 255, 255),
    [PAL_MOUTH]   = RGB565(255, 255, 255),
    [PAL_TONGUE]  = RGB565(255, 255, 255),
    [PAL_LIMBAL]  = RGB565(0, 0, 0),
    [PAL_LID]     = RGB565(0, 0, 0),
};

/* Cat: white bg + black line-art + pink accents */
const uint16_t PALETTE_CAT[PAL_COUNT] = {
    [PAL_BG]      = RGB565(255, 255, 255),  /* white background */
    [PAL_BG_EDGE] = RGB565(0, 0, 0),        /* black face/ear outline */
    [PAL_SCLERA]  = RGB565(0, 0, 0),        /* black eye outline */
    [PAL_IRIS]    = RGB565(0, 0, 0),        /* black iris ring */
    [PAL_PUPIL]   = RGB565(0, 0, 0),        /* black pupil */
    [PAL_BLUSH]   = RGB565(248, 183, 183),  /* #F8B7B7 blush */
    [PAL_TEAR]    = RGB565(135, 206, 235),  /* light blue tear */
    [PAL_SHINE]   = RGB565(255, 255, 255),  /* white catchlight */
    [PAL_STAR]    = RGB565(0, 0, 0),        /* black star */
    [PAL_SKIN]    = RGB565(255, 218, 185),  /* peach inner ear */
    [PAL_BROW]    = RGB565(0, 0, 0),        /* black brow */
    [PAL_MOUTH]   = RGB565(0, 0, 0),        /* black mouth line */
    [PAL_TONGUE]  = RGB565(255, 105, 180),  /* hot pink tongue */
    [PAL_LIMBAL]  = RGB565(0, 0, 0),
    [PAL_LID]     = RGB565(0, 0, 0),
};

const uint16_t PALETTE_PIXEL[PAL_COUNT] = {
    [PAL_BG]      = RGB565(0, 0, 0),
    [PAL_BG_EDGE] = RGB565(0, 0, 0),
    [PAL_SCLERA]  = RGB565(255, 255, 255),
    [PAL_IRIS]    = RGB565(255, 255, 255),
    [PAL_PUPIL]   = RGB565(255, 255, 255),
    [PAL_BLUSH]   = RGB565(255, 255, 255),
    [PAL_TEAR]    = RGB565(255, 255, 255),
    [PAL_SHINE]   = RGB565(255, 255, 255),
    [PAL_STAR]    = RGB565(255, 255, 255),
    [PAL_SKIN]    = RGB565(0, 0, 0),
    [PAL_BROW]    = RGB565(255, 255, 255),
    [PAL_MOUTH]   = RGB565(255, 255, 255),
    [PAL_TONGUE]  = RGB565(255, 255, 255),
    [PAL_LIMBAL]  = RGB565(0, 0, 0),
    [PAL_LID]     = RGB565(0, 0, 0),
};

const uint16_t PALETTE_ROBOT[PAL_COUNT] = {
    [PAL_BG]      = RGB565(0, 0, 0),
    [PAL_BG_EDGE] = RGB565(0, 0, 0),
    [PAL_SCLERA]  = RGB565(255, 255, 255),
    [PAL_IRIS]    = RGB565(255, 255, 255),
    [PAL_PUPIL]   = RGB565(255, 255, 255),
    [PAL_BLUSH]   = RGB565(255, 255, 255),
    [PAL_TEAR]    = RGB565(255, 255, 255),
    [PAL_SHINE]   = RGB565(255, 255, 255),
    [PAL_STAR]    = RGB565(255, 255, 255),
    [PAL_SKIN]    = RGB565(0, 0, 0),
    [PAL_BROW]    = RGB565(255, 255, 255),
    [PAL_MOUTH]   = RGB565(255, 255, 255),
    [PAL_TONGUE]  = RGB565(255, 255, 255),
    [PAL_LIMBAL]  = RGB565(0, 0, 0),
    [PAL_LID]     = RGB565(0, 0, 0),
};

const uint16_t PALETTE_VECTOR[PAL_COUNT] = {
    [PAL_BG]      = RGB565(0, 0, 0),
    [PAL_BG_EDGE] = RGB565(0, 0, 0),
    [PAL_SCLERA]  = RGB565(255, 255, 255),
    [PAL_IRIS]    = RGB565(255, 255, 255),
    [PAL_PUPIL]   = RGB565(0, 0, 0),        /* 瞳孔黑色，否则无法看方向 */
    [PAL_BLUSH]   = RGB565(255, 255, 255),
    [PAL_TEAR]    = RGB565(255, 255, 255),
    [PAL_SHINE]   = RGB565(255, 255, 255),
    [PAL_STAR]    = RGB565(255, 255, 255),
    [PAL_SKIN]    = RGB565(0, 0, 0),
    [PAL_BROW]    = RGB565(255, 255, 255),
    [PAL_MOUTH]   = RGB565(255, 255, 255),
    [PAL_TONGUE]  = RGB565(255, 255, 255),
    [PAL_LIMBAL]  = RGB565(0, 0, 0),
    [PAL_LID]     = RGB565(0, 0, 0),
};

const uint16_t PALETTE_LINEART[PAL_COUNT] = {
    [PAL_BG]      = RGB565(0, 0, 0),
    [PAL_BG_EDGE] = RGB565(0, 0, 0),
    [PAL_SCLERA]  = RGB565(255, 255, 255),
    [PAL_IRIS]    = RGB565(255, 255, 255),
    [PAL_PUPIL]   = RGB565(255, 255, 255),
    [PAL_BLUSH]   = RGB565(255, 255, 255),
    [PAL_TEAR]    = RGB565(255, 255, 255),
    [PAL_SHINE]   = RGB565(255, 255, 255),
    [PAL_STAR]    = RGB565(255, 255, 255),
    [PAL_SKIN]    = RGB565(0, 0, 0),
    [PAL_BROW]    = RGB565(255, 255, 255),
    [PAL_MOUTH]   = RGB565(255, 255, 255),
    [PAL_TONGUE]  = RGB565(255, 255, 255),
    [PAL_LIMBAL]  = RGB565(0, 0, 0),
    [PAL_LID]     = RGB565(0, 0, 0),
};

const uint16_t PALETTE_NOVA[PAL_COUNT] = {
    [PAL_BG]      = RGB565(0, 0, 0),
    [PAL_BG_EDGE] = RGB565(0, 0, 0),
    [PAL_SCLERA]  = RGB565(255, 255, 255),
    [PAL_IRIS]    = RGB565(255, 255, 255),
    [PAL_PUPIL]   = RGB565(0, 0, 0),        /* black pupils on white eyes */
    [PAL_BLUSH]   = RGB565(255, 255, 255),
    [PAL_TEAR]    = RGB565(255, 255, 255),
    [PAL_SHINE]   = RGB565(255, 255, 255),
    [PAL_STAR]    = RGB565(255, 255, 255),
    [PAL_SKIN]    = RGB565(255, 255, 255),  /* white filled head */
    [PAL_BROW]    = RGB565(0, 0, 0),        /* black brows (visible on white head) */
    [PAL_MOUTH]   = RGB565(0, 0, 0),        /* black mouth (visible on white head) */
    [PAL_TONGUE]  = RGB565(255, 255, 255),
    [PAL_LIMBAL]  = RGB565(0, 0, 0),
    [PAL_LID]     = RGB565(0, 0, 0),
};

/* Pig: pink skin + dark outlines + pale snout */
const uint16_t PALETTE_PIG[PAL_COUNT] = {
    [PAL_BG]      = RGB565(250, 200, 210),  /* soft pink background/skin */
    [PAL_BG_EDGE] = RGB565(185, 115, 125),  /* darker pink outline */
    [PAL_SCLERA]  = RGB565(255, 255, 255),  /* white */
    [PAL_IRIS]    = RGB565(60, 30, 30),     /* dark brown eye ring */
    [PAL_PUPIL]   = RGB565(0, 0, 0),        /* black pupil */
    [PAL_BLUSH]   = RGB565(250, 145, 155),  /* rosy pink blush */
    [PAL_TEAR]    = RGB565(255, 255, 255),  /* white catchlight */
    [PAL_SHINE]   = RGB565(255, 255, 255),  /* white catchlight */
    [PAL_STAR]    = RGB565(255, 255, 255),  /* white */
    [PAL_SKIN]    = RGB565(252, 220, 228),  /* light pink head fill */
    [PAL_BROW]    = RGB565(80, 40, 30),     /* dark brown brows */
    [PAL_MOUTH]   = RGB565(80, 40, 30),     /* dark brown mouth/nostrils */
    [PAL_TONGUE]  = RGB565(250, 225, 230),  /* pale pink snout */
    [PAL_LIMBAL]  = RGB565(0, 0, 0),
    [PAL_LID]     = RGB565(0, 0, 0),
};

/* Chibi: warm white face + honey brown eyes + peach blush + soft outlines */
const uint16_t PALETTE_CHIBI[PAL_COUNT] = {
    [PAL_BG]      = RGB565(255, 250, 245),  /* #FFFAF5 warm white face fill */
    [PAL_BG_EDGE] = RGB565(184, 160, 144),  /* #B8A090 soft grey-brown outline */
    [PAL_SCLERA]  = RGB565(255, 255, 255),  /* #FFFFFF pure white eye */
    [PAL_IRIS]    = RGB565(196, 136,  60),  /* #C4883C honey brown iris */
    [PAL_PUPIL]   = RGB565( 60,  36,  16),  /* #3C2410 dark brown pupil */
    [PAL_BLUSH]   = RGB565(255, 184, 176),  /* #FFB8B0 peach pink blush */
    [PAL_SHINE]   = RGB565(255, 255, 255),  /* #FFFFFF catchlight */
    [PAL_BROW]    = RGB565(107,  80,  64),  /* #6B5040 warm grey-brown brows */
    [PAL_MOUTH]   = RGB565(139, 107,  91),  /* #8B6B5B warm brown mouth */
    [PAL_TEAR]    = RGB565(160, 208, 240),  /* #A0D0F0 soft blue tear */
    [PAL_STAR]    = RGB565(255, 208,  96),  /* #FFD060 gold sparkle */
    [PAL_SKIN]    = RGB565(255, 250, 245),  /* same as PAL_BG */
    [PAL_TONGUE]  = RGB565(255, 184, 176),  /* same as PAL_BLUSH */
    [PAL_LIMBAL]  = RGB565( 42,  26,  10),  /* #2A1A0A dark limbal ring */
    [PAL_LID]     = RGB565(232, 224, 216),  /* #E8E0D8 light grey lid crease */
};
