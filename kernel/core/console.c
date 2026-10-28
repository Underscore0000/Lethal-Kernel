/*
 * Lethal Kernel - VGA text console / framebuffer bitmap console
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 *

 */
#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "../../arch/x86_64/io.h"
#include "../drivers/serial/serial.h"
#include "../drivers/video/framebuffer.h"
#include "../drivers/input/mouse.h"


#ifndef CONSOLE_SW_CURSOR
#define CONSOLE_SW_CURSOR 0
#endif

void console_scroll(int lines);

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
            if (--utf8_need == 0) {
                *cp = utf8_cp;
                return 1;
            }
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


static const uint8_t font8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* ! */
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, /* " */
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, /* # */
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, /* $ */
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, /* % */
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, /* & */
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, /* ' */
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, /* ( */
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, /* ) */
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, /* * */
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, /* + */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, /* , */
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, /* - */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, /* . */
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, /* / */
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, /* 0 */
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, /* 1 */
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, /* 2 */
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, /* 3 */
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, /* 4 */
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, /* 5 */
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, /* 6 */
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, /* 7 */
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, /* 8 */
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, /* 9 */
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, /* : */
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, /* ; */
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, /* < */
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, /* = */
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, /* > */
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, /* ? */
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, /* @ */
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, /* A */
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, /* B */
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, /* C */
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, /* D */
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, /* E */
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, /* F */
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, /* G */
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, /* H */
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* I */
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, /* J */
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, /* K */
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, /* L */
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, /* M */
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, /* N */
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, /* O */
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, /* P */
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, /* Q */
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, /* R */
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, /* S */
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* T */
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, /* U */
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, /* V */
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, /* W */
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, /* X */
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, /* Y */
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, /* Z */
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, /* [ */
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, /* \ */
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, /* ] */
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, /* ^ */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, /* _ */
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, /* ` */
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, /* a */
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, /* b */
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, /* c */
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00}, /* d */
    {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00}, /* e */
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00}, /* f */
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, /* g */
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, /* h */
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, /* i */
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, /* j */
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, /* k */
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* l */
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, /* m */
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, /* n */
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, /* o */
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, /* p */
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, /* q */
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, /* r */
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, /* s */
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, /* t */
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, /* u */
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, /* v */
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, /* w */
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, /* x */
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, /* y */
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, /* z */
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, /* { */
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, /* | */
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, /* } */
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, /* ~ */
};


static const uint8_t g_tee[8]    = {0x18,0x18,0x18,0xF8,0xF8,0x18,0x18,0x18}; /* ├ */
static const uint8_t g_corner[8] = {0x18,0x18,0x18,0xF8,0xF8,0x00,0x00,0x00}; /* └ */
static const uint8_t g_hline[8]  = {0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00}; /* ─ */
static const uint8_t g_vline[8]  = {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18}; /* │ */
static const uint8_t g_heart[8]  = {0x00,0x66,0xFF,0xFF,0xFF,0x7E,0x3C,0x18}; /* ❤ */
static const uint8_t g_bullet[8] = {0x00,0x00,0x3C,0x7E,0x7E,0x3C,0x00,0x00}; /* • */

static const uint8_t *glyph_for(uint32_t cp)
{
    if (cp >= 32 && cp <= 126) return font8x8[cp - 32];

    switch (cp) {
    case 0x251C: return g_tee;
    case 0x2514: return g_corner;
    case 0x2500: return g_hline;
    case 0x2502: return g_vline;
    case 0x2764: return g_heart;
    case 0x2022: return g_bullet;

    case 0xE0: case 0xE1: return font8x8['a' - 32];
    case 0xE8: case 0xE9: return font8x8['e' - 32];
    case 0xEC: case 0xED: return font8x8['i' - 32];
    case 0xF2: case 0xF3: return font8x8['o' - 32];
    case 0xF9: case 0xFA: return font8x8['u' - 32];
    case 0xC0: return font8x8['A' - 32];
    case 0xC8: case 0xC9: return font8x8['E' - 32];
    default:   return font8x8['?' - 32];
    }
}


#define FB_MARGIN_X 8
#define FB_CELL_W   (8 * CONSOLE_FONT_SCALE_X)
#define FB_CELL_H   (8 * CONSOLE_FONT_SCALE_Y)

static int fb_console_active = 0;

static int fb_cursor_x = FB_MARGIN_X;
static int fb_cursor_y = 0;

static uint32_t fb_fg_color = 0x00D0D0D0;
static uint32_t fb_bg_color = 0x00000000;

static enum vga_color cur_fg = VGA_COLOR_LIGHT_GREY;
static enum vga_color cur_bg = VGA_COLOR_BLACK;

static int32_t conui_last_cx = -1;
static int32_t conui_last_cy = -1;
static int     conui_last_wheel = 0;
static int     fb_scroll_offset = 0;

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


static void fill_clipped(int x, int y, int w, int h, uint32_t color)
{
    int sw = (int)fb_width();
    int sh = (int)fb_height();
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > sw) w = sw - x;
    if (y + h > sh) h = sh - y;
    if (w <= 0 || h <= 0) return;
    fb_fill_rect(x, y, w, h, color);
}

static void draw_cell(int x, int y, uint32_t cp, uint32_t fg, uint32_t bg)
{
    const uint8_t *g = glyph_for(cp);

    if (bg != CONSOLE_TRANSPARENT)
        fill_clipped(x, y, FB_CELL_W, FB_CELL_H, bg);

    for (int row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        int col = 0;
        while (col < 8) {
            if (bits & (1u << col)) {
                int start = col;
                while (col < 8 && (bits & (1u << col))) col++;
                fill_clipped(x + start * CONSOLE_FONT_SCALE_X,
                             y + row * CONSOLE_FONT_SCALE_Y,
                             (col - start) * CONSOLE_FONT_SCALE_X,
                             CONSOLE_FONT_SCALE_Y, fg);
            } else {
                col++;
            }
        }
    }
}

static void fb_newline(void)
{
    fb_cursor_x = FB_MARGIN_X;
    fb_cursor_y += FB_CELL_H;

    while (fb_cursor_y + FB_CELL_H > (int)fb_height()) {
        fb_scroll_up(FB_CELL_H, fb_bg_color);
        fb_cursor_y -= FB_CELL_H;
    }
}

static void fb_putcp(uint32_t cp)
{
    if (!fb_console_active) return;

    if (cp == '\n') { fb_newline(); return; }
    if (cp == '\r') { fb_cursor_x = FB_MARGIN_X; return; }
    if (cp == '\b') {
        if (fb_cursor_x - FB_CELL_W >= FB_MARGIN_X) {
            fb_cursor_x -= FB_CELL_W;
            fill_clipped(fb_cursor_x, fb_cursor_y, FB_CELL_W, FB_CELL_H, fb_bg_color);
        }
        return;
    }
    if (cp == '\t') {
        for (int i = 0; i < 4; i++) fb_putcp(' ');
        return;
    }

    draw_cell(fb_cursor_x, fb_cursor_y, cp, fb_fg_color, fb_bg_color);
    fb_cursor_x += FB_CELL_W;

    if (fb_cursor_x + FB_CELL_W > (int)fb_width() - FB_MARGIN_X)
        fb_newline();
}


int console_cell_w(void) { return FB_CELL_W; }
int console_cell_h(void) { return FB_CELL_H; }

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
        x += FB_CELL_W;
    }
}


const uint8_t *console_get_glyph(uint32_t cp)
{
    return glyph_for(cp);
}

int console_text_width(const char *str)
{
    int n = 0;
    if (!str) return 0;
    while (*str) { utf8_next(&str); n++; }
    return n * FB_CELL_W;
}


#if CONSOLE_SW_CURSOR
static void draw_sw_cursor(int32_t cx, int32_t cy)
{
    static const int arrow[][2] = {
        {0,0}, {0,12}, {3,9}, {6,14}, {8,13}, {5,8}, {10,8}
    };
    static const int n = sizeof(arrow) / sizeof(arrow[0]);

    uint32_t white = vga_to_rgb(VGA_COLOR_WHITE);
    uint32_t black = vga_to_rgb(VGA_COLOR_BLACK);

    for (int i = 0; i < n - 1; i++) {
        int x0 = arrow[i][0], y0 = arrow[i][1];
        int x1 = arrow[i+1][0], y1 = arrow[i+1][1];
        int dx = x1 - x0, dy = y1 - y0;
        int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
        int steps = adx > ady ? adx : ady;
        if (steps < 1) steps = 1;
        for (int s = 0; s <= steps; s++) {
            int px = cx + x0 + (dx * s) / steps;
            int py = cy + y0 + (dy * s) / steps;
            if (px < 0 || py < 0 || px >= (int)fb_width() || py >= (int)fb_height())
                continue;
            fb_putpixel(px, py, black);
        }
    }
    for (int yy = 1; yy < 12; yy++) {
        for (int xx = 1; xx < 8; xx++) {
            if (xx + yy < 12 && xx < yy) {
                int px = cx + xx, py = cy + yy;
                if (px < 0 || py < 0 || px >= (int)fb_width() || py >= (int)fb_height())
                    continue;
                fb_putpixel(px, py, white);
            }
        }
    }
}
#endif

void console_ui_init(const struct framebuffer_info *info)
{
    if (!info || info->addr == 0) return;
    conui_last_cx = -1;
    conui_last_cy = -1;
    conui_last_wheel = mouse_get_wheel();
    serial_print("[console-ui] init\n");
}

void console_ui_tick(void)
{
    if (!fb_console_active) return;

    mouse_poll();

    int w = mouse_get_wheel();
    if (w != conui_last_wheel) {
        int delta = w - conui_last_wheel;
        conui_last_wheel = w;
        console_scroll(-delta);
    }

#if CONSOLE_SW_CURSOR
    int32_t cx = mouse_get_x();
    int32_t cy = mouse_get_y();
    if (cx != conui_last_cx || cy != conui_last_cy) {
        draw_sw_cursor(cx, cy);
        conui_last_cx = cx;
        conui_last_cy = cy;
    }
#endif
}

void console_scroll(int lines)
{
    fb_scroll_offset += lines;
    if (fb_scroll_offset < 0) fb_scroll_offset = 0;
}


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
        fb_clear(fb_bg_color);
        fb_cursor_x = FB_MARGIN_X;
        fb_cursor_y = 0;
        fb_scroll_offset = 0;
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
    serial_print("[console] init framebuffer console (font bitmap integrato)\n");

    if (fb_width() == 0 || fb_height() == 0) {
        serial_print("[console] framebuffer non disponibile\n");
        return -1;
    }

    fb_cursor_x = FB_MARGIN_X;
    fb_cursor_y = 0;
    fb_console_active = 1;
    fb_clear(fb_bg_color);

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
        fb_cursor_x = FB_MARGIN_X;
        int y = fb_cursor_y + FB_CELL_H / 2;
        int w = (int)fb_width() - 2 * FB_MARGIN_X;
        fill_clipped(FB_MARGIN_X, y, w, 1, vga_to_rgb(VGA_COLOR_DARK_GREY));
        fb_newline();
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