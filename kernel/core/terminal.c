/*
 * Lethal Kernel - Graphical Terminal (movable window, TrueType font)
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */

#include "terminal.h"
#include "console.h"
#include "font.h"
#include "../drivers/serial/serial.h"
#include "../drivers/input/mouse.h"
#include "../drivers/input/keyboard.h"
#include "../drivers/video/framebuffer.h"
#include "../memory/heap.h"

#include <string.h>

#define PROMPT        "root@lethal: "
#define LINE_MAX      256
#define MAX_LINES     64
#define KEY_ESC       0x1B

#define WIN_DEFAULT_W 640
#define WIN_DEFAULT_H 400
#define TITLE_H       32
#define PAD           10

#define CW     (console_cell_w())
#define CH     (console_cell_h())
#define LINE_H (CH + 4)

static int tfb_ok;

static int32_t win_x, win_y, win_w, win_h;
static int dragging = 0;
static int32_t drag_off_x, drag_off_y;
static int32_t last_cx = -1, last_cy = -1;
static int last_buttons = 0;

static char lines[MAX_LINES][LINE_MAX];
static int line_count = 0;

static char input_buf[LINE_MAX];
static int input_len = 0;
static int caret_on = 0;

static uint32_t *saved_bg = NULL;

/* Cursor */
#define CURSOR_W 11
#define CURSOR_H 16

static uint32_t cursor_bg[CURSOR_W * CURSOR_H];
static int cursor_saved = 0;
static int32_t cursor_x = 0, cursor_y = 0;

static const char *const arrow[CURSOR_H] = {
    "X          ",
    "XX         ",
    "X.X        ",
    "X..X       ",
    "X...X      ",
    "X....X     ",
    "X.....X    ",
    "X......X   ",
    "X.......X  ",
    "X........X ",
    "X.....XXXXX",
    "X..X..X    ",
    "X.X X..X   ",
    "XX  X..X   ",
    "X    X..X  ",
    "     XXX   ",
};

/* -------- Low-level drawing on the global framebuffer -------- */



static void fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c)
{
    fb_fill_rect((int)x, (int)y, (int)w, (int)h, c);
}

/* -------- Text drawing (TrueType via font.c) -------- */


static void draw_text(int32_t x, int32_t y_baseline, const char *s, uint32_t color, int32_t max_x)
{
    if (!font_is_ready()) return;

    int ascent = font_ascent();

    while (*s) {
        uint32_t cp = 0;
        const uint8_t *p = (const uint8_t *)s;

        if (p[0] < 0x80) { cp = p[0]; s += 1; }
        else if ((p[0] & 0xE0) == 0xC0) { cp = p[0] & 0x1F; s += 1; continue; }
        else if ((p[0] & 0xF0) == 0xE0) { cp = p[0] & 0x0F; s += 1; continue; }
        else if ((p[0] & 0xF8) == 0xF0) { cp = p[0] & 0x07; s += 1; continue; }
        else { s += 1; continue; }

        int adv = font_advance(cp);
        if (x + adv > max_x) break;

        font_draw_codepoint((int)x, (int)y_baseline - ascent, cp, color);
        x += adv;
    }
}

static int text_width(const char *s)
{
    if (!font_is_ready()) return 0;
    int w = 0;
    while (*s) {
        const uint8_t *p = (const uint8_t *)s;
        uint32_t cp;
        if (p[0] < 0x80) { cp = p[0]; s += 1; }
        else if ((p[0] & 0xE0) == 0xC0) { s++; continue; }
        else if ((p[0] & 0xF0) == 0xE0) { s++; continue; }
        else if ((p[0] & 0xF8) == 0xF0) { s++; continue; }
        else { s++; continue; }
        w += font_advance(cp);
    }
    return w;
}

/* -------- Cursor overlay -------- */

static void cursor_restore_bg(void)
{
    if (!cursor_saved) return;

    int32_t w = CURSOR_W;
    int32_t h = CURSOR_H;
    if (cursor_x + w > (int32_t)fb_width())  w = (int32_t)fb_width()  - cursor_x;
    if (cursor_y + h > (int32_t)fb_height()) h = (int32_t)fb_height() - cursor_y;

    if (w > 0 && h > 0) {
        for (int32_t j = 0; j < h; j++) {
            for (int32_t i = 0; i < w; i++) {
                fb_putpixel((int)(cursor_x + i), (int)(cursor_y + j),
                            cursor_bg[j * CURSOR_W + i]);
            }
        }
    }
    cursor_saved = 0;
}

static void draw_cursor(void)
{
    int32_t x = mouse_get_x();
    int32_t y = mouse_get_y();

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= (int32_t)fb_width())  x = (int32_t)fb_width()  - 1;
    if (y >= (int32_t)fb_height()) y = (int32_t)fb_height() - 1;

    cursor_restore_bg();

    int32_t w = CURSOR_W;
    int32_t h = CURSOR_H;
    if (x + w > (int32_t)fb_width())  w = (int32_t)fb_width()  - x;
    if (y + h > (int32_t)fb_height()) h = (int32_t)fb_height() - y;
    if (w <= 0 || h <= 0) return;

    for (int32_t j = 0; j < h; j++) {
        for (int32_t i = 0; i < w; i++) {
            cursor_bg[j * CURSOR_W + i] = fb_getpixel((int)(x + i), (int)(y + j));
        }
    }

    cursor_saved = 1;
    cursor_x = x;
    cursor_y = y;

    uint32_t white = 0x00FFFFFF;
    uint32_t black = 0x00000000;

    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            char c = arrow[j][i];
            if (c == 'X') fb_putpixel((int)(x + i), (int)(y + j), black);
            else if (c == '.') fb_putpixel((int)(x + i), (int)(y + j), white);
        }
    }
}

/* -------- Window background save/restore -------- */

static void save_background(void)
{
    if (!saved_bg) return;

    for (int32_t j = 0; j < win_h; j++) {
        for (int32_t i = 0; i < win_w; i++) {
            saved_bg[j * win_w + i] = fb_getpixel((int)(win_x + i), (int)(win_y + j));
        }
    }
}

static void restore_background(void)
{
    if (!saved_bg) return;

    for (int32_t j = 0; j < win_h; j++) {
        for (int32_t i = 0; i < win_w; i++) {
            fb_putpixel((int)(win_x + i), (int)(win_y + j), saved_bg[j * win_w + i]);
        }
    }
}

/* -------- Layout -------- */

static int visible_lines(void)
{
    return (win_h - TITLE_H - 2 * PAD) / LINE_H;
}

static int max_input_cols(void)
{
    int cols = (win_w - 2 * PAD) / CW - (int)strlen(PROMPT) - 1;
    if (cols > LINE_MAX - 1) cols = LINE_MAX - 1;
    return cols < 1 ? 1 : cols;
}

static void prompt_pos(int32_t *px, int32_t *py_baseline)
{
    int vis = visible_lines();
    int n_show = line_count < vis - 1 ? line_count : vis - 1;
    if (n_show < 0) n_show = 0;

    int prompt_w = text_width(PROMPT);
    int input_w  = 0;
    for (int i = 0; i < input_len; i++) {
        input_w += font_advance((unsigned char)input_buf[i]);
    }

    *px = win_x + PAD + prompt_w + input_w;
    *py_baseline = win_y + TITLE_H + PAD + n_show * LINE_H + font_ascent();
}

static void draw_caret(void)
{
    int32_t cx, cy_baseline;
    prompt_pos(&cx, &cy_baseline);
    int top = cy_baseline - font_ascent();

    fill_rect(cx, top, 2, CH,
              caret_on ? 0x00C8E6C8 : 0x00161820);
}

/* -------- Window drawing -------- */

static void draw_window(void)
{
    uint32_t bg      = 0x00161820;
    uint32_t border  = 0x0046506E;
    uint32_t title   = 0x002D3246;
    uint32_t text    = 0x00DCDCDC;
    uint32_t title_t = 0x00C8D2F0;

    int32_t max_x = win_x + win_w - PAD;

    fill_rect(win_x, win_y, win_w, win_h, bg);
    fill_rect(win_x, win_y, win_w, 2, border);
    fill_rect(win_x, win_y + win_h - 2, win_w, 2, border);
    fill_rect(win_x, win_y, 2, win_h, border);
    fill_rect(win_x + win_w - 2, win_y, 2, win_h, border);

    fill_rect(win_x + 2, win_y + 2, win_w - 4, TITLE_H - 2, title);

    int32_t dot_y = win_y + 2 + (TITLE_H - 2 - 10) / 2;
    fill_rect(win_x + PAD,      dot_y, 10, 10, 0x00EB5F5A);
    fill_rect(win_x + PAD + 16, dot_y, 10, 10, 0x00F0BE46);
    fill_rect(win_x + PAD + 32, dot_y, 10, 10, 0x005AC864);

    int title_baseline = win_y + 2 + (TITLE_H - 2) / 2 + font_ascent() / 2 - 2;
    draw_text(win_x + PAD + 56, title_baseline,
              "Lethal Terminal", title_t, max_x);

    int vis = visible_lines();
    int n_show = line_count < vis - 1 ? line_count : vis - 1;
    if (n_show < 0) n_show = 0;

    int start = line_count - n_show;
    int32_t ty_baseline = win_y + TITLE_H + PAD + font_ascent();

    for (int i = 0; i < n_show; i++) {
        draw_text(win_x + PAD, ty_baseline, lines[start + i], text, max_x);
        ty_baseline += LINE_H;
    }

    draw_text(win_x + PAD, ty_baseline, PROMPT, 0x0078E696, max_x);

    int32_t px = win_x + PAD + text_width(PROMPT);
    input_buf[input_len] = '\0';

    draw_text(px, ty_baseline, input_buf, 0x00EBEBEB, max_x);

    draw_caret();
}

static void refresh_window(void)
{
    cursor_restore_bg();
    draw_window();
    draw_cursor();
}

/* -------- Public API -------- */

void terminal_set_fb(const struct framebuffer_info *info)
{
    if (!info || info->addr == 0) return;

    if (fb_width() == 0 || fb_height() == 0) {
        serial_print("[term] framebuffer not ready\n");
        return;
    }

    tfb_ok = 1;
    serial_print("[term] fb=");
    serial_print_dec(fb_width());
    serial_print("x");
    serial_print_dec(fb_height());
    serial_print(" font=ttf\n");
}

static void add_line(const char *s)
{
    if (line_count < MAX_LINES) {
        strncpy(lines[line_count], s, LINE_MAX - 1);
        lines[line_count][LINE_MAX - 1] = '\0';
        line_count++;
    } else {
        for (int i = 1; i < MAX_LINES; i++)
            memcpy(lines[i - 1], lines[i], LINE_MAX);

        strncpy(lines[MAX_LINES - 1], s, LINE_MAX - 1);
        lines[MAX_LINES - 1][LINE_MAX - 1] = '\0';
    }
}

void terminal_run(void)
{
    if (!tfb_ok) return;

    win_w = WIN_DEFAULT_W;
    win_h = WIN_DEFAULT_H;

    if (win_w > (int32_t)fb_width())  win_w = (int32_t)fb_width();
    if (win_h > (int32_t)fb_height()) win_h = (int32_t)fb_height();

    win_x = ((int32_t)fb_width()  - win_w) / 2;
    win_y = ((int32_t)fb_height() - win_h) / 2;

    line_count = 0;
    input_len = 0;
    input_buf[0] = '\0';
    caret_on = 1;

    dragging = 0;
    last_cx = -1;
    last_cy = -1;
    last_buttons = 0;
    cursor_saved = 0;

    saved_bg = (uint32_t *)kmalloc((uint64_t)win_w * win_h * sizeof(uint32_t));

    add_line("Lethal Kernel terminal v1.0 (TrueType)");
    add_line("Type 'help' for commands, or 'exit' (or ESC) to quit.");
    add_line("");

    save_background();
    draw_window();
    draw_cursor();

    keyboard_flush();

    uint32_t blink = 0;

    for (;;) {
        mouse_poll();

        int32_t cx = mouse_get_x();
        int32_t cy = mouse_get_y();
        int btns = mouse_get_buttons();

        int need_cursor = 0;

        if (cx != last_cx || cy != last_cy) {
            last_cx = cx;
            last_cy = cy;
            need_cursor = 1;
        }

        if ((btns & 1) && !(last_buttons & 1)) {
            if (cx >= win_x && cx < win_x + win_w &&
                cy >= win_y && cy < win_y + TITLE_H) {
                dragging = 1;
                drag_off_x = cx - win_x;
                drag_off_y = cy - win_y;
            }
        }

        if (!(btns & 1)) dragging = 0;
        last_buttons = btns;

        if (dragging) {
            int32_t nx = cx - drag_off_x;
            int32_t ny = cy - drag_off_y;

            if (nx < 0) nx = 0;
            if (ny < 0) ny = 0;
            if (nx + win_w > (int32_t)fb_width())  nx = (int32_t)fb_width()  - win_w;
            if (ny + win_h > (int32_t)fb_height()) ny = (int32_t)fb_height() - win_h;

            if (nx != win_x || ny != win_y) {
                cursor_restore_bg();
                restore_background();
                win_x = nx;
                win_y = ny;
                save_background();
                draw_window();
                need_cursor = 1;
            }
        }

        if (need_cursor) draw_cursor();

        if ((++blink % 200) == 0) {
            caret_on = !caret_on;
            cursor_restore_bg();
            draw_caret();
            draw_cursor();
        }

        char c;
        if (!keyboard_getchar(&c)) {
            for (volatile int i = 0; i < 1000; i++) __asm__ volatile("pause");
            continue;
        }

        if ((uint8_t)c == KEY_ESC) break;

        if (c == '\n' || c == '\r') {
            char full[LINE_MAX + 16];
            int n = 0;
            while (PROMPT[n]) { full[n] = PROMPT[n]; n++; }
            for (int i = 0; i < input_len; i++) full[n++] = input_buf[i];
            full[n] = '\0';

            add_line(full);
            input_buf[input_len] = '\0';

            if (strcmp(input_buf, "exit") == 0) break;

            if (strcmp(input_buf, "help") == 0) {
                add_line("  help         - display available commands");
                add_line("  exit         - close the terminal");
                add_line("  clear        - clear the terminal output");
                add_line("  echo <text>  - print text");
            } else if (strcmp(input_buf, "clear") == 0) {
                line_count = 0;
            } else if (strncmp(input_buf, "echo ", 5) == 0) {
                add_line(input_buf + 5);
            } else if (input_len > 0) {
                char err[LINE_MAX + 32];
                strcpy(err, "Unknown command: ");
                strcat(err, input_buf);
                add_line(err);
            }

            input_len = 0;
            caret_on = 1;
            refresh_window();
            continue;
        }

        if (c == '\b' || (uint8_t)c == 0x7F) {
            if (input_len > 0) {
                input_len--;
                caret_on = 1;
                refresh_window();
            }
            continue;
        }

        if (c >= 32 && c < 127 && input_len < max_input_cols()) {
            input_buf[input_len++] = c;
            caret_on = 1;
            refresh_window();
        }
    }

    cursor_restore_bg();
    restore_background();

    if (saved_bg) {
        kfree(saved_bg);
        saved_bg = NULL;
    }

    keyboard_flush();
}