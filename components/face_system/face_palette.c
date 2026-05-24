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

/* Cat: black bg + white line-art (monochrome) */
const uint16_t PALETTE_CAT[PAL_COUNT] = {
    [PAL_BG]      = RGB565(0, 0, 0),
    [PAL_BG_EDGE] = RGB565(0, 0, 0),
    [PAL_SCLERA]  = RGB565(255, 255, 255),
    [PAL_IRIS]    = RGB565(255, 255, 255),
    [PAL_PUPIL]   = RGB565(0, 0, 0),        /* black pupil for direction */
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

const uint16_t PALETTE_PIXEL[PAL_COUNT] = {
    [PAL_BG]      = RGB565(0, 0, 0),
    [PAL_BG_EDGE] = RGB565(0, 0, 0),
    [PAL_SCLERA]  = RGB565(255, 255, 255),
    [PAL_IRIS]    = RGB565(255, 255, 255),
    [PAL_PUPIL]   = RGB565(0, 0, 0),        /* black pupil for direction */
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

/* Pig: monochrome */
const uint16_t PALETTE_PIG[PAL_COUNT] = {
    [PAL_BG]      = RGB565(0, 0, 0),
    [PAL_BG_EDGE] = RGB565(0, 0, 0),
    [PAL_SCLERA]  = RGB565(255, 255, 255),
    [PAL_IRIS]    = RGB565(255, 255, 255),
    [PAL_PUPIL]   = RGB565(0, 0, 0),
    [PAL_BLUSH]   = RGB565(0, 0, 0),
    [PAL_TEAR]    = RGB565(255, 255, 255),
    [PAL_SHINE]   = RGB565(255, 255, 255),
    [PAL_STAR]    = RGB565(255, 255, 255),
    [PAL_SKIN]    = RGB565(255, 255, 255),
    [PAL_BROW]    = RGB565(0, 0, 0),
    [PAL_MOUTH]   = RGB565(0, 0, 0),
    [PAL_TONGUE]  = RGB565(255, 255, 255),
    [PAL_LIMBAL]  = RGB565(0, 0, 0),
    [PAL_LID]     = RGB565(0, 0, 0),
};

/* Chibi: monochrome */
const uint16_t PALETTE_CHIBI[PAL_COUNT] = {
    [PAL_BG]      = RGB565(0, 0, 0),
    [PAL_BG_EDGE] = RGB565(0, 0, 0),
    [PAL_SCLERA]  = RGB565(255, 255, 255),
    [PAL_IRIS]    = RGB565(255, 255, 255),
    [PAL_PUPIL]   = RGB565(0, 0, 0),
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
