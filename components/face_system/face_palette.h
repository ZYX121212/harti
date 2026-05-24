#ifndef FACE_PALETTE_H
#define FACE_PALETTE_H

#include <stdint.h>

#define RGB565(r,g,b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

typedef enum {
    PAL_BG = 0,
    PAL_BG_EDGE,
    PAL_SCLERA,
    PAL_IRIS,
    PAL_PUPIL,
    PAL_BLUSH,
    PAL_TEAR,
    PAL_SHINE,
    PAL_STAR,
    PAL_SKIN,
    PAL_BROW,
    PAL_MOUTH,
    PAL_TONGUE,
    PAL_LIMBAL,
    PAL_LID,
    PAL_COUNT
} palette_index_t;

/* ── All themes: black background + white expressions ──────────── */

extern const uint16_t PALETTE_WHITE[PAL_COUNT];
extern const uint16_t PALETTE_BLACK[PAL_COUNT];
extern const uint16_t PALETTE_CAT[PAL_COUNT];
extern const uint16_t PALETTE_PIXEL[PAL_COUNT];
extern const uint16_t PALETTE_ROBOT[PAL_COUNT];
extern const uint16_t PALETTE_VECTOR[PAL_COUNT];
extern const uint16_t PALETTE_LINEART[PAL_COUNT];
extern const uint16_t PALETTE_NOVA[PAL_COUNT];
extern const uint16_t PALETTE_PIG[PAL_COUNT];
extern const uint16_t PALETTE_CHIBI[PAL_COUNT];

#endif
