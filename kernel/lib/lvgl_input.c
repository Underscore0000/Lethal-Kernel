/*
 * Lethal Kernel - LVGL input driver (mouse + cursore custom)
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include "../../third_party/lvgl/lvgl.h"
#include "../drivers/serial/serial.h"
#include "../drivers/input/mouse.h"
#include "../drivers/video/framebuffer.h"

static lv_indev_t *mouse_indev = NULL;
static lv_obj_t   *cursor_obj  = NULL;


#define CUR_W 20
#define CUR_H 32
#define SS    4                
#define BORDER_D2 25           


typedef struct { int x, y; } ipt_t;
static const ipt_t arrow[] = {
    {0, 0}, {0, 26}, {6, 20}, {10, 30}, {15, 28}, {11, 18}, {18, 18}
};
#define ARROW_N ((int)(sizeof(arrow) / sizeof(arrow[0])))


#define OFF 1

static uint32_t       cur_pixels[CUR_W * CUR_H];
static lv_image_dsc_t cur_dsc;


static int poly_inside(int px, int py)
{
    int inside = 0;
    for (int i = 0, j = ARROW_N - 1; i < ARROW_N; j = i++) {
        int xi = (arrow[i].x + OFF) * SS, yi = (arrow[i].y + OFF) * SS;
        int xj = (arrow[j].x + OFF) * SS, yj = (arrow[j].y + OFF) * SS;
        if ((yi > py) != (yj > py)) {
            int64_t lhs = (int64_t)(px - xi) * (yj - yi);
            int64_t rhs = (int64_t)(xj - xi) * (py - yi);
            if ((yj - yi) > 0 ? (lhs < rhs) : (lhs > rhs))
                inside = !inside;
        }
    }
    return inside;
}


static int64_t poly_dist2(int px, int py)
{
    int64_t best = (int64_t)1 << 60;
    for (int i = 0, j = ARROW_N - 1; i < ARROW_N; j = i++) {
        int64_t ax = (arrow[j].x + OFF) * SS, ay = (arrow[j].y + OFF) * SS;
        int64_t bx = (arrow[i].x + OFF) * SS, by = (arrow[i].y + OFF) * SS;
        int64_t dx = bx - ax, dy = by - ay;
        int64_t len2 = dx * dx + dy * dy;
        int64_t t_num = (px - ax) * dx + (py - ay) * dy;
        int64_t cx, cy;
        if (t_num <= 0 || len2 == 0) { cx = ax; cy = ay; }
        else if (t_num >= len2)      { cx = bx; cy = by; }
        else {

            cx = ax + (dx * t_num) / len2;
            cy = ay + (dy * t_num) / len2;
        }
        int64_t ex = px - cx, ey = py - cy;
        int64_t d2 = ex * ex + ey * ey;
        if (d2 < best) best = d2;
    }
    return best;
}

static void cursor_build(void)
{
    for (int y = 0; y < CUR_H; y++) {
        for (int x = 0; x < CUR_W; x++) {
            int nb = 0, nw = 0;
            for (int sy = 0; sy < SS; sy++) {
                for (int sx = 0; sx < SS; sx++) {
                    int px = x * SS + sx * 1 + SS / 2 - SS / 2;
                    int py = y * SS + sy * 1;

                    px = x * SS + sx;
                    py = y * SS + sy;
                    if (poly_inside(px, py)) {
                        nb++;                               
                    } else if (poly_dist2(px, py) <= BORDER_D2) {
                        nw++;                              
                    }
                }
            }
            int cov = nb + nw;
            uint32_t pix = 0;
            if (cov > 0) {
                uint32_t a = (uint32_t)(cov * 255) / (SS * SS);
                uint32_t c = (uint32_t)(nw * 255) / (uint32_t)cov; 
                pix = (a << 24) | (c << 16) | (c << 8) | c;        
            }
            cur_pixels[y * CUR_W + x] = pix;
        }
    }

    lv_memzero(&cur_dsc, sizeof(cur_dsc));
    cur_dsc.header.magic  = LV_IMAGE_HEADER_MAGIC;
    cur_dsc.header.cf     = LV_COLOR_FORMAT_ARGB8888;
    cur_dsc.header.w      = CUR_W;
    cur_dsc.header.h      = CUR_H;
    cur_dsc.header.stride = CUR_W * 4;
    cur_dsc.data          = (const uint8_t *)cur_pixels;
    cur_dsc.data_size     = sizeof(cur_pixels);

    serial_print("[cursor] antialiased bitmap built ");
    serial_print_dec(CUR_W); serial_print("x"); serial_print_dec(CUR_H);
    serial_print(" sample(2,4)=");
    serial_print_hex(cur_pixels[4 * CUR_W + 2]);
    serial_print("\n");
}

static lv_obj_t *cursor_create(void)
{
    cursor_build();

    lv_obj_t *layer = lv_layer_sys();
    if (!layer) {
        serial_print("[cursor] ERROR: no sys layer\n");
        return NULL;
    }

    lv_obj_t *obj = lv_image_create(layer);
    if (!obj) {
        serial_print("[cursor] ERROR: lv_image_create failed\n");
        return NULL;
    }

    lv_image_set_src(obj, &cur_dsc);
    lv_obj_set_size(obj, CUR_W, CUR_H);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);


    lv_obj_set_style_translate_x(obj, -OFF, 0);
    lv_obj_set_style_translate_y(obj, -OFF, 0);

    lv_obj_move_foreground(obj);
    return obj;
}

static void mouse_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;


    mouse_poll();

    data->point.x = (int32_t)mouse_get_x();
    data->point.y = (int32_t)mouse_get_y();
    data->state = (mouse_get_buttons() & 0x01)
                  ? LV_INDEV_STATE_PRESSED
                  : LV_INDEV_STATE_RELEASED;


    static int n = 0;
    if ((n++ % 2000) == 0) {
        serial_print("[mouse-read] x=");
        serial_print_dec((uint64_t)data->point.x);
        serial_print(" y=");
        serial_print_dec((uint64_t)data->point.y);
        serial_print(" btn=");
        serial_print_dec(mouse_get_buttons());
        serial_print("\n");
        mouse_debug_dump();
    }
}

int lvgl_input_init(void)
{
    serial_print("[lvgl] lvgl_input_init() entered\n");

    lv_display_t *disp = lv_display_get_default();
    if (!disp) {
        serial_print("[lvgl] ERROR: no default display\n");
        return -1;
    }

    mouse_indev = lv_indev_create();
    if (!mouse_indev) {
        serial_print("[lvgl] ERROR: lv_indev_create failed\n");
        return -1;
    }
    lv_indev_set_type(mouse_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(mouse_indev, mouse_read_cb);
    lv_indev_set_display(mouse_indev, disp);


    lv_timer_t *rt = lv_indev_get_read_timer(mouse_indev);
    if (rt) {
        lv_timer_set_period(rt, 5);
        serial_print("[lvgl] indev read period = 5 ms\n");
    }
    lv_timer_t *ft = lv_display_get_refr_timer(disp);
    if (ft) {
        lv_timer_set_period(ft, 10);
        serial_print("[lvgl] display refresh period = 10 ms\n");
    }

    cursor_obj = cursor_create();
    if (!cursor_obj) {
        serial_print("[lvgl] ERROR: cursor creation failed\n");
        return -1;
    }
    lv_indev_set_cursor(mouse_indev, cursor_obj);
    lv_obj_set_pos(cursor_obj, mouse_get_x(), mouse_get_y());
    lv_obj_invalidate(cursor_obj);

    serial_print("[lvgl] mouse input + custom cursor ready\n");
    return 0;
}