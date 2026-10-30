/*
 * Lethal Kernel - VGA text console / framebuffer bitmap console
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "font.h"
#include "../../arch/x86_64/io.h"
#include "../drivers/serial/serial.h"
#include "../drivers/video/framebuffer.h"
#include "../drivers/input/mouse.h"
#include "../memory/heap.h"
#include <string.h>

#ifndef CONSOLE_SW_CURSOR
#define CONSOLE_SW_CURSOR 0
#endif

/* -------------------------------------------------------------------------
 * UTF-8
 * ------------------------------------------------------------------------- */
static uint32_t utf8_cp   = 0;
static int      utf8_need = 0;

static int utf8_feed(uint8_t b, uint32_t *cp)
{
    if (utf8_need > 0) {
        if ((b & 0xC0) == 0x80) {
            utf8_cp = (utf8_cp << 6) | (b & 0x3F);
            if (--utf8_need == 0) { *cp = utf8_cp; return 1; }
            return 0;
        }
        utf8_need = 0;
    }
    if (b < 0x80) { *cp = b; return 1; }
    if ((b & 0xE0) == 0xC0) { utf8_cp = b & 0x1F; utf8_need = 1; return 0; }
    if ((b & 0xF0) == 0xE0) { utf8_cp = b & 0x0F; utf8_need = 2; return 0; }
    if ((b & 0xF8) == 0xF0) { utf8_cp = b & 0x07; utf8_need = 3; return 0; }
    *cp = '?';
    return 1;
}

static uint32_t utf8_next(const char **pp)
{
    const uint8_t *p = (const uint8_t *)*pp;
    uint32_t cp;
    int extra;

    if (p[0] < 0x80)              { cp = p[0];        extra = 0; }
    else if ((p[0] & 0xE0) == 0xC0) { cp = p[0] & 0x1F; extra = 1; }
    else if ((p[0] & 0xF0) == 0xE0) { cp = p[0] & 0x0F; extra = 2; }
    else if ((p[0] & 0xF8) == 0xF0) { cp = p[0] & 0x07; extra = 3; }
    else { *pp += 1; return '?'; }

    p++;
    while (extra-- > 0) {
        if ((*p & 0xC0) != 0x80) { *pp = (const char *)p; return '?'; }
        cp = (cp << 6) | (*p & 0x3F);
        p++;
    }
    *pp = (const char *)p;
    return cp;
}

/* -------------------------------------------------------------------------
 * Bitmap 8x8 fallback
 * ------------------------------------------------------------------------- */
static const uint8_t font8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00},
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00},
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00},
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00},
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00},
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00},
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00},
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06},
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00},
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00},
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00},
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00},
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00},
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00},
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00},
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00},
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00},
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00},
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00},
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00},
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00},
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06},
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00},
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00},
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00},
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00},
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00},
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00},
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00},
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00},
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00},
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00},
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00},
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00},
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00},
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00},
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00},
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00},
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00},
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00},
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00},
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00},
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00},
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00},
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00},
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00},
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00},
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00},
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00},
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00},
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00},
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00},
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00},
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00},
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00},
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00},
    {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00},
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00},
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F},
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00},
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E},
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00},
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00},
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00},
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00},
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F},
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78},
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00},
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00},
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00},
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00},
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00},
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F},
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00},
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00},
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00},
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00},
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00},
};

static const uint8_t *glyph_for(uint32_t cp)
{
    if (cp >= 32 && cp <= 126) return font8x8[cp - 32];
    return font8x8['?' - 32];
}

/* -------------------------------------------------------------------------
 * VGA text mode
 * ------------------------------------------------------------------------- */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

static size_t   vga_row;
static size_t   vga_col;
static uint8_t  vga_color;
static volatile uint16_t *vga_buffer;

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg)
{
    return (uint8_t)fg | (uint8_t)(bg << 4);
}

static inline uint16_t vga_entry(char c, uint8_t color)
{
    return (uint16_t)(unsigned char)c | (uint16_t)color << 8;
}

static void vga_scroll(void)
{
    for (size_t row = 1; row < VGA_HEIGHT; row++)
        for (size_t col = 0; col < VGA_WIDTH; col++)
            vga_buffer[(row - 1) * VGA_WIDTH + col] = vga_buffer[row * VGA_WIDTH + col];

    for (size_t col = 0; col < VGA_WIDTH; col++)
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = vga_entry(' ', vga_color);

    vga_row = VGA_HEIGHT - 1;
}

static char vga_map_cp(uint32_t cp)
{
    if (cp < 0x80) return (char)cp;
    switch (cp) {
    case 0x251C: return (char)0xC3;
    case 0x2514: return (char)0xC0;
    case 0x2500: return (char)0xC4;
    case 0x2502: return (char)0xB3;
    case 0x2764: return (char)0x03;
    case 0x2022: return (char)0x07;
    default:     return '?';
    }
}

static void vga_putcp(uint32_t cp)
{
    if (cp == '\n') {
        vga_col = 0;
        if (++vga_row == VGA_HEIGHT) vga_scroll();
        return;
    }
    if (cp == '\r') { vga_col = 0; return; }
    if (cp == '\b') {
        if (vga_col > 0) {
            vga_col--;
            vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_color);
        }
        return;
    }
    vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(vga_map_cp(cp), vga_color);
    if (++vga_col == VGA_WIDTH) {
        vga_col = 0;
        if (++vga_row == VGA_HEIGHT) vga_scroll();
    }
}

/* -------------------------------------------------------------------------
 * Framebuffer backbuffer
 * ------------------------------------------------------------------------- */
static uint32_t *bb_buf;
static uint32_t  bb_w, bb_h, bb_pitch;

static void bb_alloc(void)
{
    if (bb_buf) return;
    bb_w = fb_width();
    bb_h = fb_height();
    bb_pitch = fb_pitch();
    size_t bytes = (size_t)bb_pitch * bb_h;
    bb_buf = (uint32_t *)kmalloc(bytes);
    if (bb_buf) memset(bb_buf, 0, bytes);
}

static void bb_flush(void)
{
    if (!bb_buf) return;
    uint32_t stride = bb_pitch / 4;
    for (uint32_t y = 0; y < bb_h; y++) {
        uint32_t *row = bb_buf + y * stride;
        for (uint32_t x = 0; x < bb_w; x++) {
            fb_putpixel((int)x, (int)y, row[x]);
        }
    }
}

static void bb_fill_rect(int x, int y, int w, int h, uint32_t color)
{
    if (!bb_buf) return;
    int sw = (int)bb_w, sh = (int)bb_h;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > sw) w = sw - x;
    if (y + h > sh) h = sh - y;
    if (w <= 0 || h <= 0) return;
    uint32_t stride = bb_pitch / 4;
    for (int yy = y; yy < y + h; yy++) {
        uint32_t *row = bb_buf + yy * stride;
        for (int xx = x; xx < x + w; xx++) row[xx] = color;
    }
}

uint32_t console_read_pixel(int x, int y)
{
    if (!bb_buf) return 0;
    if (x < 0 || y < 0 || x >= (int)bb_w || y >= (int)bb_h) return 0;
    return bb_buf[y * (bb_pitch / 4) + x];
}
void console_write_pixel(int x, int y, uint32_t color)
{
    if (!bb_buf) return;
    if (x < 0 || y < 0 || x >= (int)bb_w || y >= (int)bb_h) return;
    bb_buf[y * (bb_pitch / 4) + x] = color;
}

int console_pixel_width(void)  { return (int)bb_w; }
int console_pixel_height(void) { return (int)bb_h; }

/* -------------------------------------------------------------------------
 * Font-based cell rendering
 * ------------------------------------------------------------------------- */
static int fb_console_active = 0;

static int fb_cursor_x = 0;
static int fb_cursor_y = 0;

static uint32_t fb_fg_color = 0x00D0D0D0;
static uint32_t fb_bg_color = 0x00000000;

static enum vga_color cur_fg = VGA_COLOR_LIGHT_GREY;
static enum vga_color cur_bg = VGA_COLOR_BLACK;

static int32_t conui_last_wheel = 0;

static uint32_t vga_to_rgb(enum vga_color c)
{
    static const uint32_t palette[16] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
        0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
        0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
    };
    return palette[c & 0xF];
}


static int cell_h(void)
{
    if (font_is_ready()) return font_line_height();
    return 8 * CONSOLE_FONT_SCALE_Y;
}

int console_cell_w(void)
{
    if (font_is_ready()) return font_advance(' ');
    return 8 * CONSOLE_FONT_SCALE_X;
}

int console_cell_h(void) { return cell_h(); }

static void draw_cell(int x, int y, uint32_t cp, uint32_t fg, uint32_t bg)
{
    int cw = font_is_ready() ? font_advance(cp) : (8 * CONSOLE_FONT_SCALE_X);
    int ch = cell_h();
    if (cw < 1) cw = 1;

    if (bg != CONSOLE_TRANSPARENT) {
        bb_fill_rect(x, y, cw, ch, bg);
    }

    if (font_is_ready()) {
        int baseline = y + font_ascent();
        font_draw_codepoint(x, baseline, cp, fg);
        return;
    }

    /* fallback bitmap 8x8 scalato */
    const uint8_t *g = glyph_for(cp);
    int sx = cw / 8; if (sx < 1) sx = 1;
    int sy = ch / 8; if (sy < 1) sy = 1;
    uint32_t stride = bb_pitch / 4;
    for (int row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        for (int col = 0; col < 8; col++) {
            if (!(bits & (1u << col))) continue;
            for (int dy = 0; dy < sy; dy++) {
                int py = y + row * sy + dy;
                if (py < 0 || py >= (int)bb_h) continue;
                uint32_t *prow = bb_buf + py * stride;
                for (int dx = 0; dx < sx; dx++) {
                    int px = x + col * sx + dx;
                    if (px < 0 || px >= (int)bb_w) continue;
                    prow[px] = fg;
                }
            }
        }
    }
}


static void fb_newline(void)
{
    fb_cursor_x = 0;
    fb_cursor_y += cell_h();

    while (fb_cursor_y + cell_h() > (int)bb_h) {
        int stride = bb_pitch / 4;
        int line = cell_h();
        memmove(bb_buf, bb_buf + line * stride,
                (size_t)(bb_h - line) * bb_pitch);
        for (int yy = bb_h - line; yy < (int)bb_h; yy++) {
            uint32_t *row = bb_buf + yy * stride;
            for (uint32_t xx = 0; xx < bb_w; xx++) row[xx] = fb_bg_color;
        }
        fb_cursor_y -= line;
    }
}

static void fb_putcp(uint32_t cp)
{
    if (!fb_console_active) return;

    if (cp == '\n') { fb_newline(); return; }
    if (cp == '\r') { fb_cursor_x = 0; return; }
    if (cp == '\b') {
        int cw = font_is_ready() ? font_advance(' ') : (8 * CONSOLE_FONT_SCALE_X);
        if (cw < 1) cw = 1;
        if (fb_cursor_x - cw >= 0) {
            fb_cursor_x -= cw;
            bb_fill_rect(fb_cursor_x, fb_cursor_y, cw, cell_h(), fb_bg_color);
        }
        return;
    }
    if (cp == '\t') {
        for (int i = 0; i < 4; i++) fb_putcp(' ');
        return;
    }

    int adv = font_is_ready() ? font_advance(cp) : (8 * CONSOLE_FONT_SCALE_X);
    if (adv < 1) adv = 1;

    draw_cell(fb_cursor_x, fb_cursor_y, cp, fb_fg_color, fb_bg_color);
    fb_cursor_x += adv;

    if (fb_cursor_x + adv > (int)bb_w) fb_newline();
}

void console_draw_glyph(int x, int y, uint32_t cp, uint32_t fg, uint32_t bg)
{
    draw_cell(x, y, cp, fg, bg);
}

void console_draw_text(int x, int y, const char *str, uint32_t fg, uint32_t bg)
{
    if (!str) return;
    while (*str) {
        uint32_t cp = utf8_next(&str);
        draw_cell(x, y, cp, fg, bg);
        x += font_is_ready() ? font_advance(cp) : (8 * CONSOLE_FONT_SCALE_X);
    }
}

int console_text_width(const char *str)
{
    int w = 0;
    if (!str) return 0;
    while (*str) {
        uint32_t cp = utf8_next(&str);
        w += font_is_ready() ? font_advance(cp) : (8 * CONSOLE_FONT_SCALE_X);
    }
    return w;
}

/* -------------------------------------------------------------------------
 * UI tick / scroll
 * ------------------------------------------------------------------------- */
void console_ui_init(const struct framebuffer_info *info)
{
    if (!info || info->addr == 0) return;
    conui_last_wheel = mouse_get_wheel();
    serial_print("[console-ui] init\n");
}

void console_scroll(int lines)
{
    if (!fb_console_active || !bb_buf) return;
    if (lines == 0) return;

    int stride = bb_pitch / 4;
    int dy = lines * cell_h();
    if (dy <= -(int)bb_h || dy >= (int)bb_h) return;

    if (dy > 0) {
        memmove(bb_buf, bb_buf + dy * stride,
                (size_t)(bb_h - dy) * bb_pitch);
        for (int yy = bb_h - dy; yy < (int)bb_h; yy++) {
            uint32_t *row = bb_buf + yy * stride;
            for (uint32_t xx = 0; xx < bb_w; xx++) row[xx] = fb_bg_color;
        }
        fb_cursor_y -= dy;
        if (fb_cursor_y < 0) fb_cursor_y = 0;
    } else {
        int up = -dy;
        memmove(bb_buf + up * stride, bb_buf,
                (size_t)(bb_h - up) * bb_pitch);
        for (int yy = 0; yy < up; yy++) {
            uint32_t *row = bb_buf + yy * stride;
            for (uint32_t xx = 0; xx < bb_w; xx++) row[xx] = fb_bg_color;
        }
        fb_cursor_y += up;
        if (fb_cursor_y + cell_h() > (int)bb_h) fb_cursor_y = (int)bb_h - cell_h();
    }

    bb_flush();
}

void console_ui_tick(void)
{
    if (!fb_console_active) return;
    int w = mouse_get_wheel();
    if (w != conui_last_wheel) {
        int delta = w - conui_last_wheel;
        conui_last_wheel = w;
        console_scroll(-delta * 2);
    }
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */
void console_set_color(enum vga_color fg, enum vga_color bg)
{
    cur_fg = fg;
    cur_bg = bg;
    vga_color   = vga_entry_color(fg, bg);
    fb_fg_color = vga_to_rgb(fg);
    fb_bg_color = vga_to_rgb(bg);
}

void console_clear(void)
{
    if (fb_console_active) {
        if (bb_buf) {
            size_t words = ((size_t)bb_pitch * bb_h) / 4;
            for (size_t i = 0; i < words; i++) bb_buf[i] = fb_bg_color;
        }
        fb_cursor_x = 0;
        fb_cursor_y = 0;
        bb_flush();
    } else {
        for (size_t row = 0; row < VGA_HEIGHT; row++)
            for (size_t col = 0; col < VGA_WIDTH; col++)
                vga_buffer[row * VGA_WIDTH + col] = vga_entry(' ', vga_color);
        vga_row = 0;
        vga_col = 0;
    }
}

void console_init(void)
{
    if (fb_console_active) return;
    vga_buffer = VGA_MEMORY;
    vga_row = 0;
    vga_col = 0;
    vga_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_clear();
}

int console_init_fb(void)
{
    serial_print("[console] init framebuffer console\n");

    if (fb_width() == 0 || fb_height() == 0) {
        serial_print("[console] framebuffer non disponibile\n");
        return -1;
    }

    bb_alloc();
    if (!bb_buf) {
        serial_print("[console] backbuffer alloc failed\n");
        return -1;
    }

    fb_cursor_x = 0;
    fb_cursor_y = 0;
    fb_console_active = 1;
    console_clear();

    serial_print("[console] framebuffer console ready\n");
    return 0;
}

void console_putc(char c)
{
    uint32_t cp;
    if (!utf8_feed((uint8_t)c, &cp)) return;
    if (fb_console_active) fb_putcp(cp);
    else vga_putcp(cp);
}

void console_print(const char *str)
{
    while (*str) console_putc(*str++);
    if (fb_console_active) bb_flush();
}

static void format_dec(uint64_t value, char *buf, size_t buflen)
{
    char tmp[21];
    size_t i = 0;
    if (value == 0) tmp[i++] = '0';
    else {
        while (value > 0 && i < sizeof(tmp)) {
            tmp[i++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }
    size_t j = 0;
    while (i > 0 && j + 1 < buflen) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

static void format_hex(uint64_t value, char *buf, size_t buflen)
{
    if (buflen < 3) { if (buflen > 0) buf[0] = '\0'; return; }
    buf[0] = '0'; buf[1] = 'x';
    size_t max_digits = (buflen - 3 < 16) ? (buflen - 3) : 16;
    for (size_t i = 0; i < max_digits; i++) {
        uint8_t nibble = (uint8_t)((value >> ((max_digits - 1 - i) * 4)) & 0xF);
        buf[2 + i] = (char)(nibble < 10 ? ('0' + nibble) : ('A' + nibble - 10));
    }
    buf[2 + max_digits] = '\0';
}

void console_print_dec(uint64_t value)
{
    char buf[21];
    format_dec(value, buf, sizeof(buf));
    console_print(buf);
}

void console_print_hex(uint64_t value)
{
    char buf[19];
    format_hex(value, buf, sizeof(buf));
    console_print(buf);
}

void console_ok(const char *msg)
{
    console_set_color(VGA_COLOR_LIGHT_GREEN, cur_bg);
    console_print("[ OK ] ");
    console_set_color(VGA_COLOR_LIGHT_GREY, cur_bg);
    console_print(msg);
    console_print("\n");
}

void console_info(const char *msg)
{
    console_set_color(VGA_COLOR_LIGHT_CYAN, cur_bg);
    console_print("[ .. ] ");
    console_set_color(VGA_COLOR_LIGHT_GREY, cur_bg);
    console_print(msg);
    console_print("\n");
}

void console_success(const char *msg)
{
    console_set_color(VGA_COLOR_LIGHT_GREEN, cur_bg);
    console_print("[ OK ] ");
    console_set_color(VGA_COLOR_WHITE, cur_bg);
    console_print(msg);
    console_print("\n");
}

void console_separator(void)
{
    if (fb_console_active) {
        fb_cursor_x = 0;
        int y = fb_cursor_y + cell_h() / 2;
        int w = (int)bb_w;
        bb_fill_rect(0, y, w, 1, vga_to_rgb(VGA_COLOR_DARK_GREY));
        fb_newline();
        bb_flush();
        return;
    }
    console_set_color(VGA_COLOR_DARK_GREY, cur_bg);
    for (int i = 0; i < VGA_WIDTH; i++) console_putc('-');
    console_set_color(VGA_COLOR_LIGHT_GREY, cur_bg);
    console_putc('\n');
}

void console_print_header(void)
{
    console_clear();
    console_set_color(VGA_COLOR_LIGHT_CYAN, cur_bg);
    console_print("  Lethal Kernel v1.0\n");
    console_set_color(VGA_COLOR_LIGHT_GREY, cur_bg);
    console_separator();
    console_putc('\n');
}