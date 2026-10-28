
/*
 * Lethal Kernel - Graphical Terminal (movable window, bitmap font)
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */

#include "terminal.h"
#include "console.h"
#include "../drivers/serial/serial.h"
#include "../drivers/input/mouse.h"
#include "../drivers/input/keyboard.h"
#include "../memory/heap.h"

#include <string.h>

#define PROMPT        "root@lethal: "
#define LINE_MAX      256
#define MAX_LINES     64
#define KEY_ESC       0x1B

#define WIN_DEFAULT_W 640
#define WIN_DEFAULT_H 400
#define TITLE_H       28
#define PAD           10

#define CW     (console_cell_w())
#define CH     (console_cell_h())
#define LINE_H (CH + 4)

static uint8_t *tfb_base;
static uint32_t tfb_w, tfb_h, tfb_pitch, tfb_bpp;
static uint32_t rpos, rsz, gpos, gsz, bpos, bsz;
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

static uint8_t *saved_bg = NULL;

#define CURSOR_W 11
#define CURSOR_H 16

static uint8_t cursor_bg[CURSOR_W * CURSOR_H * 4];
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

static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r >> (8 - rsz)) << rpos) |
           ((uint32_t)(g >> (8 - gsz)) << gpos) |
           ((uint32_t)(b >> (8 - bsz)) << bpos);
}

static void put_px(int32_t x, int32_t y, uint32_t c)
{
    if (x < 0 || y < 0 ||
        x >= (int32_t)tfb_w || y >= (int32_t)tfb_h)
        return;

    uint8_t *p = tfb_base
        + (uint64_t)y * tfb_pitch
        + (uint64_t)x * (tfb_bpp / 8);

    if (tfb_bpp == 32) {
        *(volatile uint32_t *)p = c;
    } else if (tfb_bpp == 24) {
        p[0] = (uint8_t)c;
        p[1] = (uint8_t)(c >> 8);
        p[2] = (uint8_t)(c >> 16);
    } else {
        *(volatile uint16_t *)p = (uint16_t)c;
    }
}

static void fill_rect(int32_t x, int32_t y,
                      int32_t w, int32_t h, uint32_t c)
{
    if (x < 0) {
        w += x;
        x = 0;
    }

    if (y < 0) {
        h += y;
        y = 0;
    }

    if (x + w > (int32_t)tfb_w)
        w = (int32_t)tfb_w - x;

    if (y + h > (int32_t)tfb_h)
        h = (int32_t)tfb_h - y;

    if (w <= 0 || h <= 0)
        return;

    if (tfb_bpp == 32) {
        for (int32_t j = 0; j < h; j++) {
            volatile uint32_t *p = (volatile uint32_t *)
                (tfb_base
                 + (uint64_t)(y + j) * tfb_pitch
                 + (uint64_t)x * 4);

            for (int32_t i = 0; i < w; i++)
                p[i] = c;
        }
    } else {
        for (int32_t j = 0; j < h; j++) {
            for (int32_t i = 0; i < w; i++)
                put_px(x + i, y + j, c);
        }
    }
}

static void draw_glyph(int32_t x, int32_t y,
                       uint32_t cp, uint32_t color)
{
    const uint8_t *g = console_get_glyph(cp);

    for (int row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        int col = 0;

        while (col < 8) {
            if (bits & (1u << col)) {
                int start = col;

                while (col < 8 && (bits & (1u << col)))
                    col++;

                fill_rect(
                    x + start * CONSOLE_FONT_SCALE_X,
                    y + row * CONSOLE_FONT_SCALE_Y,
                    (col - start) * CONSOLE_FONT_SCALE_X,
                    CONSOLE_FONT_SCALE_Y,
                    color
                );
            } else {
                col++;
            }
        }
    }
}

static void draw_text(int32_t x, int32_t y,
                      const char *s, uint32_t color, int32_t max_x)
{
    while (*s) {
        uint8_t ch = (uint8_t)*s++;

        if ((ch & 0xC0) == 0x80)
            continue;

        if (x + CW > max_x)
            break;

        draw_glyph(x, y, ch < 0x80 ? ch : '?', color);
        x += CW;
    }
}

static void cursor_restore_bg(void)
{
    if (!cursor_saved)
        return;

    int32_t bytes = (int32_t)(tfb_bpp / 8);
    int32_t w = CURSOR_W;
    int32_t h = CURSOR_H;

    if (cursor_x + w > (int32_t)tfb_w)
        w = (int32_t)tfb_w - cursor_x;

    if (cursor_y + h > (int32_t)tfb_h)
        h = (int32_t)tfb_h - cursor_y;

    if (w > 0 && h > 0) {
        for (int32_t j = 0; j < h; j++) {
            uint8_t *dst = tfb_base
                + (uint64_t)(cursor_y + j) * tfb_pitch
                + (uint64_t)cursor_x * bytes;

            memcpy(
                dst,
                cursor_bg + (uint64_t)j * CURSOR_W * bytes,
                (size_t)w * bytes
            );
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

    if (x >= (int32_t)tfb_w)
        x = (int32_t)tfb_w - 1;

    if (y >= (int32_t)tfb_h)
        y = (int32_t)tfb_h - 1;

    cursor_restore_bg();

    int32_t bytes = (int32_t)(tfb_bpp / 8);
    int32_t w = CURSOR_W;
    int32_t h = CURSOR_H;

    if (x + w > (int32_t)tfb_w)
        w = (int32_t)tfb_w - x;

    if (y + h > (int32_t)tfb_h)
        h = (int32_t)tfb_h - y;

    if (w <= 0 || h <= 0)
        return;

    for (int32_t j = 0; j < h; j++) {
        uint8_t *src = tfb_base
            + (uint64_t)(y + j) * tfb_pitch
            + (uint64_t)x * bytes;

        memcpy(
            cursor_bg + (uint64_t)j * CURSOR_W * bytes,
            src,
            (size_t)w * bytes
        );
    }

    cursor_saved = 1;
    cursor_x = x;
    cursor_y = y;

    uint32_t white = rgb(255, 255, 255);
    uint32_t black = rgb(0, 0, 0);

    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            char c = arrow[j][i];

            if (c == 'X')
                put_px(x + i, y + j, black);
            else if (c == '.')
                put_px(x + i, y + j, white);
        }
    }
}

static void save_background(void)
{
    if (!saved_bg)
        return;

    int32_t bytes = (int32_t)(tfb_bpp / 8);

    for (int32_t j = 0; j < win_h; j++) {
        uint8_t *src = tfb_base
            + (uint64_t)(win_y + j) * tfb_pitch
            + (uint64_t)win_x * bytes;

        memcpy(
            saved_bg + (uint64_t)j * win_w * bytes,
            src,
            (size_t)win_w * bytes
        );
    }
}

static void restore_background(void)
{
    if (!saved_bg)
        return;

    int32_t bytes = (int32_t)(tfb_bpp / 8);

    for (int32_t j = 0; j < win_h; j++) {
        uint8_t *dst = tfb_base
            + (uint64_t)(win_y + j) * tfb_pitch
            + (uint64_t)win_x * bytes;

        memcpy(
            dst,
            saved_bg + (uint64_t)j * win_w * bytes,
            (size_t)win_w * bytes
        );
    }
}

static int visible_lines(void)
{
    return (win_h - TITLE_H - 2 * PAD) / LINE_H;
}

static int max_input_cols(void)
{
    int cols = (win_w - 2 * PAD) / CW
             - (int)strlen(PROMPT) - 1;

    if (cols > LINE_MAX - 1)
        cols = LINE_MAX - 1;

    return cols < 1 ? 1 : cols;
}

static void prompt_pos(int32_t *px, int32_t *py)
{
    int vis = visible_lines();
    int n_show = line_count < vis - 1
               ? line_count
               : vis - 1;

    if (n_show < 0)
        n_show = 0;

    *px = win_x + PAD
        + ((int32_t)strlen(PROMPT) + input_len) * CW;

    *py = win_y + TITLE_H + PAD + n_show * LINE_H;
}

static void draw_caret(void)
{
    int32_t cx, cy;

    prompt_pos(&cx, &cy);

    fill_rect(
        cx,
        cy,
        2,
        CH,
        caret_on ? rgb(200, 230, 200) : rgb(22, 24, 32)
    );
}

static void draw_window(void)
{
    uint32_t bg      = rgb(22, 24, 32);
    uint32_t border  = rgb(70, 80, 110);
    uint32_t title   = rgb(45, 50, 70);
    uint32_t text    = rgb(220, 220, 220);
    uint32_t title_t = rgb(200, 210, 240);

    int32_t max_x = win_x + win_w - PAD;

    fill_rect(win_x, win_y, win_w, win_h, bg);
    fill_rect(win_x, win_y, win_w, 2, border);
    fill_rect(win_x, win_y + win_h - 2, win_w, 2, border);
    fill_rect(win_x, win_y, 2, win_h, border);
    fill_rect(win_x + win_w - 2, win_y, 2, win_h, border);

    fill_rect(
        win_x + 2,
        win_y + 2,
        win_w - 4,
        TITLE_H - 2,
        title
    );

    int32_t dot_y = win_y + 2 + (TITLE_H - 2 - 10) / 2;

    fill_rect(win_x + PAD,      dot_y, 10, 10, rgb(235, 95, 90));
    fill_rect(win_x + PAD + 16, dot_y, 10, 10, rgb(240, 190, 70));
    fill_rect(win_x + PAD + 32, dot_y, 10, 10, rgb(90, 200, 100));

    draw_text(
        win_x + PAD + 56,
        win_y + 2 + (TITLE_H - 2 - CH) / 2,
        "Lethal Terminal",
        title_t,
        max_x
    );

    int vis = visible_lines();
    int n_show = line_count < vis - 1
               ? line_count
               : vis - 1;

    if (n_show < 0)
        n_show = 0;

    int start = line_count - n_show;
    int32_t ty = win_y + TITLE_H + PAD;

    for (int i = 0; i < n_show; i++) {
        draw_text(
            win_x + PAD,
            ty,
            lines[start + i],
            text,
            max_x
        );

        ty += LINE_H;
    }

    int32_t px = win_x + PAD;

    draw_text(
        px,
        ty,
        PROMPT,
        rgb(120, 230, 150),
        max_x
    );

    px += (int32_t)strlen(PROMPT) * CW;

    input_buf[input_len] = '\0';

    draw_text(
        px,
        ty,
        input_buf,
        rgb(235, 235, 235),
        max_x
    );

    draw_caret();
}

static void refresh_window(void)
{
    cursor_restore_bg();
    draw_window();
    draw_cursor();
}

void terminal_set_fb(const struct framebuffer_info *info)
{
    if (!info || info->addr == 0)
        return;

    tfb_base = (uint8_t *)(uintptr_t)info->addr;

    tfb_w     = info->width;
    tfb_h     = info->height;
    tfb_pitch = info->pitch;
    tfb_bpp   = info->bpp;

    rpos = info->red_pos;
    rsz  = info->red_mask;

    gpos = info->green_pos;
    gsz  = info->green_mask;

    bpos = info->blue_pos;
    bsz  = info->blue_mask;

    if (rsz == 0 || rsz > 8) {
        rpos = 16;
        rsz  = 8;
        gpos = 8;
        gsz  = 8;
        bpos = 0;
        bsz  = 8;
    }

    tfb_ok = 1;

    serial_print("[term] fb=");
    serial_print_dec(tfb_w);
    serial_print("x");
    serial_print_dec(tfb_h);
    serial_print(" font=bitmap\n");
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
    if (!tfb_ok)
        return;

    win_w = WIN_DEFAULT_W;
    win_h = WIN_DEFAULT_H;

    if (win_w > (int32_t)tfb_w)
        win_w = (int32_t)tfb_w;

    if (win_h > (int32_t)tfb_h)
        win_h = (int32_t)tfb_h;

    win_x = ((int32_t)tfb_w - win_w) / 2;
    win_y = ((int32_t)tfb_h - win_h) / 2;

    line_count = 0;
    input_len = 0;
    input_buf[0] = '\0';
    caret_on = 1;

    dragging = 0;
    last_cx = -1;
    last_cy = -1;
    last_buttons = 0;
    cursor_saved = 0;

    saved_bg = (uint8_t *)kmalloc(
        (uint64_t)win_w * win_h * (tfb_bpp / 8)
    );

    add_line("Lethal Kernel terminal v1.0");
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

        if (!(btns & 1))
            dragging = 0;

        last_buttons = btns;

        if (dragging) {
            int32_t nx = cx - drag_off_x;
            int32_t ny = cy - drag_off_y;

            if (nx < 0)
                nx = 0;

            if (ny < 0)
                ny = 0;

            if (nx + win_w > (int32_t)tfb_w)
                nx = (int32_t)tfb_w - win_w;

            if (ny + win_h > (int32_t)tfb_h)
                ny = (int32_t)tfb_h - win_h;

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

        if (need_cursor)
            draw_cursor();

        if ((++blink % 200) == 0) {
            caret_on = !caret_on;

            cursor_restore_bg();
            draw_caret();
            draw_cursor();
        }

        char c;

        if (!keyboard_getchar(&c)) {
            for (volatile int i = 0; i < 1000; i++)
                __asm__ volatile("pause");

            continue;
        }

        if ((uint8_t)c == KEY_ESC)
            break;

        if (c == '\n' || c == '\r') {
            char full[LINE_MAX + 16];
            int n = 0;

            while (PROMPT[n]) {
                full[n] = PROMPT[n];
                n++;
            }

            for (int i = 0; i < input_len; i++)
                full[n++] = input_buf[i];

            full[n] = '\0';

            add_line(full);

            input_buf[input_len] = '\0';

            if (strcmp(input_buf, "exit") == 0)
                break;

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

        if (c >= 32 && c < 127 &&
            input_len < max_input_cols()) {
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
