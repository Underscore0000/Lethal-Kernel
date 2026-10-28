/*
 * Lethal Kernel - LVGL display driver
 *
 * Collega LVGL al framebuffer VBE. La risoluzione NON è più fissa:
 * viene letta dal framebuffer (fb_width/fb_height), quindi la GUI e il
 * wallpaper si adattano a qualunque monitor/risoluzione.
 */
#include <stdint.h>
#include <stddef.h>
#include "../../third_party/lvgl/lvgl.h"
#include "../drivers/video/framebuffer.h"
#include "../drivers/serial/serial.h"
#include "../memory/heap.h"


#define LVGL_FALLBACK_W 1024
#define LVGL_FALLBACK_H 768


#define LVGL_BUF_LINES 40

static lv_display_t *lvgl_disp = NULL;
static uint32_t lvgl_w = 0;
static uint32_t lvgl_h = 0;


static uint8_t *alloc_aligned(size_t size)
{
    uint8_t *raw = (uint8_t *)kmalloc(size + 64);
    if (!raw) return NULL;
    uintptr_t a = ((uintptr_t)raw + 63u) & ~(uintptr_t)63u;
    return (uint8_t *)a;
}


static void lvgl_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);


    const uint32_t *src = (const uint32_t *)px_map;

    for (int32_t y = 0; y < h; y++) {
        int32_t py = area->y1 + y;
        if (py < 0 || py >= (int32_t)lvgl_h) continue;

        for (int32_t x = 0; x < w; x++) {
            int32_t px = area->x1 + x;
            if (px < 0 || px >= (int32_t)lvgl_w) continue;
            fb_putpixel((uint32_t)px, (uint32_t)py, src[y * w + x]);
        }
    }

    lv_display_flush_ready(disp);
}

int lvgl_display_init(void)
{
    lv_init();

    lvgl_w = fb_width();
    lvgl_h = fb_height();
    if (lvgl_w == 0 || lvgl_h == 0) {
        lvgl_w = LVGL_FALLBACK_W;
        lvgl_h = LVGL_FALLBACK_H;
    }

    size_t buf_size = (size_t)lvgl_w * LVGL_BUF_LINES * 4;
    uint8_t *buf1 = alloc_aligned(buf_size);
    uint8_t *buf2 = alloc_aligned(buf_size);
    if (!buf1 || !buf2) {
        serial_print("[lvgl] cannot allocate render buffers\n");
        return -1;
    }

    lvgl_disp = lv_display_create((int32_t)lvgl_w, (int32_t)lvgl_h);
    if (!lvgl_disp) {
        serial_print("[lvgl] display_create failed\n");
        return -1;
    }

    lv_display_set_color_format(lvgl_disp, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_flush_cb(lvgl_disp, lvgl_flush);
    lv_display_set_buffers(lvgl_disp, buf1, buf2, (uint32_t)buf_size,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    serial_print("[lvgl] display initialized ");
    serial_print_dec(lvgl_w);
    serial_print("x");
    serial_print_dec(lvgl_h);
    serial_print("x32\n");
    return 0;
}

void lvgl_tick_inc(uint32_t ms)
{
    lv_tick_inc(ms);
}

void lvgl_handler(void)
{
    lv_timer_handler();
}

lv_display_t *lvgl_get_display(void)
{
    return lvgl_disp;
}