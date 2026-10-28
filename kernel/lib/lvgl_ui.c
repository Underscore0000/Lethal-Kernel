/*
 * Lethal Kernel - LVGL UI di test
 *
 * Crea una finestra con una label ("Hello, Lethal Kernel!") e un
 * bottone ("Click me"). Serve per verificare che LVGL funzioni.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "../../third_party/lvgl/lvgl.h"
#include "../drivers/serial/serial.h"

/* Callback del bottone */
static void btn_clicked_cb(lv_event_t *e)
{
    (void)e;
    serial_print("[lvgl] Button clicked!\n");
}


void lvgl_ui_test(void)
{
    lv_obj_t *scr = lv_scr_act();


    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1E1E2E), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *win = lv_obj_create(scr);
    lv_obj_set_size(win, 500, 300);
    lv_obj_center(win);
    lv_obj_set_style_bg_color(win, lv_color_hex(0x313244), 0);
    lv_obj_set_style_radius(win, 16, 0);
    lv_obj_set_style_shadow_width(win, 20, 0);
    lv_obj_set_style_shadow_color(win, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(win, LV_OPA_50, 0);


    lv_obj_t *title = lv_label_create(win);
    lv_label_set_text(title, "Lethal Kernel");
    lv_obj_set_style_text_color(title, lv_color_hex(0x89B4FA), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);


    lv_obj_t *label = lv_label_create(win);
    lv_label_set_text(label, "GUI in LVGL funziona!");
    lv_obj_set_style_text_color(label, lv_color_hex(0xCDD6F4), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 60);


    lv_obj_t *btn = lv_button_create(win);
    lv_obj_set_size(btn, 200, 60);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xA6E3A1), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x94E2D5), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_add_event_cb(btn, btn_clicked_cb, LV_EVENT_CLICKED, NULL);


    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Cliccami!");
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0x1E1E2E), 0);
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_16, 0);
    lv_obj_center(btn_label);

    serial_print("[lvgl] UI test created\n");
}