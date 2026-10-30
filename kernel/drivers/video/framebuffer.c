/*
 * Lethal Kernel - Framebuffer driver (VBE linear framebuffer)
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "framebuffer.h"
#include "../../memory/vmm.h"
#include "../../core/console.h"
#include "../../drivers/serial/serial.h"
#include <string.h>

#define FB_VIRT_BASE  (0x0000700000000000ULL + (32ULL * 1024 * 1024))


static volatile uint32_t *fb_ptr = 0;
static uint32_t fb_w = 0;
static uint32_t fb_h = 0;
static uint32_t fb_p = 0;

int fb_init(const struct framebuffer_info *info)
{
    serial_print("[fb] fb_init called, info=");
    serial_print_hex((uint64_t)(uintptr_t)info);
    serial_print("\n");

    if (!info) {
        serial_print("[fb] info is NULL\n");
        return -1;
    }

    serial_print("[fb] addr=");
    serial_print_hex(info->addr);
    serial_print(" width=");
    serial_print_dec(info->width);
    serial_print(" height=");
    serial_print_dec(info->height);
    serial_print(" pitch=");
    serial_print_dec(info->pitch);
    serial_print(" bpp=");
    serial_print_dec(info->bpp);
    serial_print("\n");

    serial_print("[fb] red_pos=");
    serial_print_dec(info->red_pos);
    serial_print(" red_mask=");
    serial_print_dec(info->red_mask);
    serial_print(" green_pos=");
    serial_print_dec(info->green_pos);
    serial_print(" green_mask=");
    serial_print_dec(info->green_mask);
    serial_print(" blue_pos=");
    serial_print_dec(info->blue_pos);
    serial_print(" blue_mask=");
    serial_print_dec(info->blue_mask);
    serial_print("\n");

    if (info->addr == 0 || info->width == 0 || info->height == 0 ||
        info->pitch == 0 || info->bpp == 0) {
        serial_print("[fb] struct is empty, aborting\n");
        return -1;
    }

    
    if (info->bpp != 32) {
        serial_print("[fb] bpp != 32, aborting\n");
        return -1;
    }

    uint64_t size = (uint64_t)info->pitch * (uint64_t)info->height;
    uint64_t pages = (size + 4095ULL) / 4096ULL;

    serial_print("[fb] mapping ");
    serial_print_dec(pages);
    serial_print(" pages at virt=");
    serial_print_hex(FB_VIRT_BASE);
    serial_print(" phys=");
    serial_print_hex(info->addr);
    serial_print("\n");

    
    for (uint64_t i = 0; i < pages; i++) {
        uint64_t virt = FB_VIRT_BASE + (i * 4096ULL);
        uint64_t phys = info->addr + (i * 4096ULL);
        int r = vmm_map_page(virt, phys, PTE_PRESENT | PTE_WRITE | PTE_PCD);
        if (r != 0) {
            serial_print("[fb] vmm_map_page failed at page ");
            serial_print_dec(i);
            serial_print(" (virt=");
            serial_print_hex(virt);
            serial_print(" phys=");
            serial_print_hex(phys);
            serial_print(")\n");
            return -1;
        }
    }

    fb_ptr = (volatile uint32_t *)FB_VIRT_BASE;
    fb_w = info->width;
    fb_h = info->height;
    fb_p = info->pitch;

    serial_print("[fb] framebuffer mapped OK: ");
    serial_print_dec(fb_w);
    serial_print("x");
    serial_print_dec(fb_h);
    serial_print(" pitch=");
    serial_print_dec(fb_p);
    serial_print("\n");

    return 0;
}

uint32_t fb_width(void)  { return fb_w; }
uint32_t fb_height(void) { return fb_h; }
uint32_t fb_pitch(void)  { return fb_p; }

void fb_clear(uint32_t color)
{
    if (!fb_ptr) return;

    uint32_t *p = (uint32_t *)fb_ptr;
    uint64_t count = (uint64_t)fb_p / 4 * fb_h;
    for (uint64_t i = 0; i < count; i++)
        p[i] = color;
}


void fb_scroll_up(int px, uint32_t color)
{
    if (!fb_ptr) return;
    if (px <= 0) return;

    if ((uint32_t)px >= fb_h) {
        fb_clear(color);
        return;
    }

    uint32_t *p = (uint32_t *)fb_ptr;
    uint64_t words_per_row = (uint64_t)fb_p / 4;
    uint64_t total  = words_per_row * fb_h;
    uint64_t offset = words_per_row * (uint64_t)px;
    uint64_t keep   = total - offset;

    for (uint64_t i = 0; i < keep; i++)
        p[i] = p[i + offset];

    for (uint64_t i = keep; i < total; i++)
        p[i] = color;
}

void fb_putpixel(int x, int y, uint32_t color)
{
    if (!fb_ptr) return;
    if (x < 0 || y < 0) return;
    if ((uint32_t)x >= fb_w || (uint32_t)y >= fb_h) return;

    uint32_t *row = (uint32_t *)((uint8_t *)fb_ptr + (uint64_t)y * fb_p);
    row[x] = color;
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t color)
{
    if (!fb_ptr) return;
    if (w <= 0 || h <= 0) return;

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w;
    int y1 = y + h;
    if (x1 > (int)fb_w) x1 = (int)fb_w;
    if (y1 > (int)fb_h) y1 = (int)fb_h;
    if (x0 >= x1 || y0 >= y1) return;

    for (int yy = y0; yy < y1; yy++) {
        uint32_t *row = (uint32_t *)((uint8_t *)fb_ptr + (uint64_t)yy * fb_p);
        for (int xx = x0; xx < x1; xx++)
            row[xx] = color;
    }
}

uint32_t fb_getpixel(int x, int y)
{
    if (!fb_ptr) return 0;
    if (x < 0 || y < 0) return 0;
    if ((uint32_t)x >= fb_w || (uint32_t)y >= fb_h) return 0;

    uint32_t *row = (uint32_t *)((uint8_t *)fb_ptr + (uint64_t)y * fb_p);
    return row[x];
}