/*
 * Lethal Kernel - Framebuffer driver (VBE linear framebuffer)
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_DRIVERS_VIDEO_FRAMEBUFFER_H
#define LETHAL_DRIVERS_VIDEO_FRAMEBUFFER_H

#include <stdint.h>


struct framebuffer_info {
    uint64_t addr;        
    uint32_t width;       
    uint32_t height;      
    uint32_t pitch;       
    uint32_t bpp;         
    uint8_t  red_pos;
    uint8_t  red_mask;
    uint8_t  green_pos;
    uint8_t  green_mask;
    uint8_t  blue_pos;
    uint8_t  blue_mask;
    uint8_t  reserved0;
    uint8_t  reserved1;
    uint64_t reserved2;
} __attribute__((packed));


#define FB_COLOR_BLACK   0x00000000u
#define FB_COLOR_WHITE   0x00FFFFFFu
#define FB_COLOR_RED     0x00FF0000u
#define FB_COLOR_GREEN   0x0000FF00u
#define FB_COLOR_BLUE    0x000000FFu


int fb_init(const struct framebuffer_info *info);


uint32_t fb_width(void);
uint32_t fb_height(void);
uint32_t fb_pitch(void);


void fb_clear(uint32_t color);
void fb_scroll_up(int px, uint32_t color);


void fb_putpixel(int x, int y, uint32_t color);


void fb_fill_rect(int x, int y, int w, int h, uint32_t color);

uint32_t fb_getpixel(int x, int y);

#endif /* LETHAL_DRIVERS_VIDEO_FRAMEBUFFER_H */