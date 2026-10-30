/*
 * Lethal Kernel - VGA text console / framebuffer bitmap console
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_CONSOLE_H
#define LETHAL_CONSOLE_H

#include <stdint.h>
#include <stddef.h>
#include "../drivers/serial/serial.h"
#include "../drivers/video/framebuffer.h"

#ifndef CONSOLE_FONT_SCALE_X
#define CONSOLE_FONT_SCALE_X 1
#endif
#ifndef CONSOLE_FONT_SCALE_Y
#define CONSOLE_FONT_SCALE_Y 2
#endif

#define CONSOLE_TRANSPARENT 0xFF000000u

enum vga_color {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,
    VGA_COLOR_WHITE         = 15,
};

void console_init(void);
void console_clear(void);
void console_set_color(enum vga_color fg, enum vga_color bg);
void console_putc(char c);
void console_print(const char *str);
void console_print_dec(uint64_t value);
void console_print_hex(uint64_t value);
void console_ok(const char *msg);
void console_info(const char *msg);
void console_success(const char *msg);
void console_separator(void);
void console_print_header(void);

int console_init_fb(void);

void console_ui_init(const struct framebuffer_info *info);
void console_ui_tick(void);
void console_scroll(int lines);

int  console_cell_w(void);
int  console_cell_h(void);
void console_draw_glyph(int x, int y, uint32_t cp, uint32_t fg, uint32_t bg);
void console_draw_text(int x, int y, const char *utf8, uint32_t fg, uint32_t bg);
int  console_text_width(const char *utf8);

const uint8_t *console_get_glyph(uint32_t cp);

/* Used by font.c to alpha-blend against the backbuffer. */
uint32_t console_read_pixel(int x, int y);

void     console_write_pixel(int x, int y, uint32_t color);
int      console_pixel_width(void);
int      console_pixel_height(void);
#endif /* LETHAL_CONSOLE_H */