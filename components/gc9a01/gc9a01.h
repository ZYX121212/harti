#ifndef GC9A01_H
#define GC9A01_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GC9A01_WIDTH  240
#define GC9A01_HEIGHT 240

// RGB565 颜色定义
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_GRAY    0x8410

void gc9a01_init(void);
void gc9a01_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void gc9a01_send_pixels(const uint16_t *pixels, size_t len);
void gc9a01_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void gc9a01_fill_screen(uint16_t color);
void gc9a01_set_backlight(uint8_t level); // 0-100

#ifdef __cplusplus
}
#endif

#endif
