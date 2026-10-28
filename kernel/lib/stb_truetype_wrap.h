/*
 * Lethal Kernel - stb_truetype wrappers (SSE-free interface)
 *
 * console.c is compiled without SSE (freestanding kernel), so it cannot
 * call stb_truetype functions that return or take float. These wrappers
 * keep the float math inside stb_truetype.c (compiled with -msse) and
 * expose an int-based interface (scale is passed as scale * 1000).
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_STB_TRUETYPE_WRAP_H
#define LETHAL_STB_TRUETYPE_WRAP_H

#include "../../third_party/stb_truetype.h"


int stbtt_scale_x1000(stbtt_fontinfo *font, int pixel_height);


void stbtt_glyph_box_x1000(stbtt_fontinfo *font, int glyph_index,
                           int scale_x1000,
                           int *x0, int *y0, int *x1, int *y1);


void stbtt_make_glyph_x1000(stbtt_fontinfo *font,
                            unsigned char *output,
                            int w, int h, int stride,
                            int scale_x1000, int glyph_index);

#endif /* LETHAL_STB_TRUETYPE_WRAP_H */