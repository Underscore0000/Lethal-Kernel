/*
 * Lethal Kernel - stb_truetype wrapper
 *

 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>

#include "stb_truetype_wrap.h"
#include "../memory/heap.h"
#include "../core/panic.h"



void *malloc(size_t n)              { return kmalloc(n); }
void *calloc(size_t n, size_t m)    { return kcalloc(n, m); }
void *realloc(void *p, size_t n)    { return krealloc(p, n); }
void  free(void *p)                 { kfree(p); }

void __assert_fail(const char *assertion, const char *file,
                   unsigned int line, const char *function)
{
    (void)assertion; (void)file; (void)line; (void)function;
    panic("stb_truetype assertion failed");
    __builtin_unreachable();
}

/* --- stb_truetype implementation ---------------------------------------- */

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb_truetype.h"

/* --- SSE-free wrappers --------------------------------------------------- */

int stbtt_scale_x1000(stbtt_fontinfo *font, int pixel_height)
{
    float s = stbtt_ScaleForPixelHeight(font, (float)pixel_height);
    return (int)(s * 1000.0f);
}

void stbtt_glyph_box_x1000(stbtt_fontinfo *font, int glyph_index,
                           int scale_x1000,
                           int *x0, int *y0, int *x1, int *y1)
{
    float s = (float)scale_x1000 / 1000.0f;
    stbtt_GetGlyphBitmapBox(font, glyph_index, s, s, x0, y0, x1, y1);
}

void stbtt_make_glyph_x1000(stbtt_fontinfo *font,
                            unsigned char *output,
                            int w, int h, int stride,
                            int scale_x1000, int glyph_index)
{
    float s = (float)scale_x1000 / 1000.0f;
    stbtt_MakeGlyphBitmap(font, output, w, h, stride, s, s, glyph_index);
}