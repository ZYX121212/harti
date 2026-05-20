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
    PAL_COUNT
} palette_index_t;

static const uint16_t PALETTE_WHITE[PAL_COUNT] = {
    [PAL_BG]      = RGB565(255, 255, 255),
    [PAL_BG_EDGE] = RGB565(235, 235, 235),
    [PAL_SCLERA]  = RGB565(255, 255, 255),
    [PAL_IRIS]    = RGB565(40, 40, 40),
    [PAL_PUPIL]   = RGB565(0, 0, 0),
    [PAL_BLUSH]   = RGB565(255, 160, 160),
    [PAL_TEAR]    = RGB565(180, 210, 255),
    [PAL_SHINE]   = RGB565(255, 255, 255),
    [PAL_STAR]    = RGB565(255, 220, 0),
    [PAL_SKIN]    = RGB565(255, 240, 225),
    [PAL_BROW]    = RGB565(60, 50, 45),
    [PAL_MOUTH]   = RGB565(200, 120, 120),
    [PAL_TONGUE]  = RGB565(255, 100, 100),
};

static const uint16_t PALETTE_BLACK[PAL_COUNT] = {
    [PAL_BG]      = RGB565(0, 0, 0),
    [PAL_BG_EDGE] = RGB565(0, 0, 0),
    [PAL_SCLERA]  = RGB565(58, 58, 62),
    [PAL_IRIS]    = RGB565(82, 80, 85),
    [PAL_PUPIL]   = RGB565(4, 4, 6),
    [PAL_BLUSH]   = RGB565(30, 18, 18),
    [PAL_TEAR]    = RGB565(30, 40, 55),
    [PAL_SHINE]   = RGB565(255, 255, 255),
    [PAL_STAR]    = RGB565(255, 255, 100),
    [PAL_SKIN]    = RGB565(20, 18, 20),
    [PAL_BROW]    = RGB565(50, 48, 50),
    [PAL_MOUTH]   = RGB565(40, 30, 30),
    [PAL_TONGUE]  = RGB565(60, 20, 20),
};

static const uint16_t PALETTE_CAT[PAL_COUNT] = {
    [PAL_BG]      = RGB565(30, 28, 36),
    [PAL_BG_EDGE] = RGB565(18, 16, 24),
    [PAL_SCLERA]  = RGB565(58, 58, 62),
    [PAL_IRIS]    = RGB565(255, 200, 60),
    [PAL_PUPIL]   = RGB565(20, 18, 20),
    [PAL_BLUSH]   = RGB565(255, 140, 140),
    [PAL_TEAR]    = RGB565(160, 210, 255),
    [PAL_SHINE]   = RGB565(255, 245, 200),
    [PAL_STAR]    = RGB565(255, 230, 130),
    [PAL_SKIN]    = RGB565(60, 55, 70),
    [PAL_BROW]    = RGB565(100, 95, 90),
    [PAL_MOUTH]   = RGB565(200, 160, 160),
    [PAL_TONGUE]  = RGB565(240, 120, 120),
};

static const uint16_t PALETTE_PIXEL[PAL_COUNT] = {
    [PAL_BG]      = RGB565(255, 255, 255),
    [PAL_BG_EDGE] = RGB565(200, 200, 200),
    [PAL_SCLERA]  = RGB565(255, 255, 255),
    [PAL_IRIS]    = RGB565(0, 180, 60),
    [PAL_PUPIL]   = RGB565(0, 0, 0),
    [PAL_BLUSH]   = RGB565(255, 100, 100),
    [PAL_TEAR]    = RGB565(100, 180, 255),
    [PAL_SHINE]   = RGB565(255, 255, 255),
    [PAL_STAR]    = RGB565(255, 255, 0),
    [PAL_SKIN]    = RGB565(255, 220, 180),
    [PAL_BROW]    = RGB565(80, 50, 30),
    [PAL_MOUTH]   = RGB565(60, 30, 20),
    [PAL_TONGUE]  = RGB565(255, 80, 100),
};

static const uint16_t PALETTE_ROBOT[PAL_COUNT] = {
    [PAL_BG]      = RGB565(40, 42, 48),
    [PAL_BG_EDGE] = RGB565(25, 27, 32),
    [PAL_SCLERA]  = RGB565(70, 75, 85),
    [PAL_IRIS]    = RGB565(60, 200, 255),
    [PAL_PUPIL]   = RGB565(10, 30, 60),
    [PAL_BLUSH]   = RGB565(255, 80, 60),
    [PAL_TEAR]    = RGB565(100, 150, 255),
    [PAL_SHINE]   = RGB565(180, 230, 255),
    [PAL_STAR]    = RGB565(255, 220, 60),
    [PAL_SKIN]    = RGB565(70, 75, 85),
    [PAL_BROW]    = RGB565(50, 55, 60),
    [PAL_MOUTH]   = RGB565(180, 190, 200),
    [PAL_TONGUE]  = RGB565(255, 60, 50),
};

#endif
