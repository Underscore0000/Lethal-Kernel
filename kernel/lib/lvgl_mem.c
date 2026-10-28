/*
 * Lethal Kernel - LVGL memory backend
 *
 * Fornisce le funzioni di memoria e stringa che LVGL usa in modalità
 * LV_STDLIB_CUSTOM. Tutto è rediretto a kmalloc/kfree e alle funzioni
 * stringa del kernel.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "../../third_party/lvgl/lvgl.h"
#include "../memory/heap.h"
#include "../include/string.h"


void *lv_malloc_core(size_t size)
{
    return kmalloc(size);
}

void *lv_realloc_core(void *p, size_t size)
{
    return krealloc(p, size);
}

void lv_free_core(void *p)
{
    kfree(p);
}

void lv_mem_init(void)   { }
void lv_mem_deinit(void) { }

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
    if (!mon_p) return;
    mon_p->total_size = 0;
    mon_p->free_size = 0;
    mon_p->free_biggest_size = 0;
    mon_p->used_cnt = 0;
    mon_p->max_used = 0;
    mon_p->used_pct = 0;
    mon_p->frag_pct = 0;
}

lv_result_t lv_mem_test_core(void)
{
    return LV_RESULT_OK;
}


void *lv_memcpy(void *dst, const void *src, size_t n)
{
    return memcpy(dst, src, n);
}

void *lv_memmove(void *dst, const void *src, size_t n)
{
    return memmove(dst, src, n);
}

void lv_memset(void *dst, uint8_t v, size_t n)
{
    memset(dst, (int)v, n);
}

int lv_memcmp(const void *a, const void *b, size_t n)
{
    return memcmp(a, b, n);
}



size_t lv_strlen(const char *s)
{
    return strlen(s);
}

int lv_strcmp(const char *a, const char *b)
{
    return strcmp(a, b);
}

int lv_strncmp(const char *a, const char *b, size_t n)
{
    return strncmp(a, b, n);
}

char *lv_strcpy(char *dst, const char *src)
{
    return strcpy(dst, src);
}

char *lv_strncpy(char *dst, const char *src, size_t n)
{
    return strncpy(dst, src, n);
}

char *lv_strcat(char *dst, const char *src)
{
    return strcat(dst, src);
}


char *lv_strdup(const char *src)
{
    if (!src) return NULL;
    size_t n = strlen(src) + 1;
    char *dst = (char *)kmalloc(n);
    if (!dst) return NULL;
    memcpy(dst, src, n);
    return dst;
}


size_t lv_strlcpy(char *dst, const char *src, size_t dst_size)
{
    if (!dst || !src) return 0;
    size_t src_len = strlen(src);
    if (dst_size == 0) return src_len;
    size_t copy = (src_len < dst_size - 1) ? src_len : (dst_size - 1);
    memcpy(dst, src, copy);
    dst[copy] = '\0';
    return src_len;
}

/* =========================================================================
 * sprintf (stub)
 * ========================================================================= */

int lv_snprintf(char *buf, size_t n, const char *fmt, ...)
{
    (void)fmt;
    if (buf && n > 0) buf[0] = '\0';
    return 0;
}

int lv_vsnprintf(char *buf, size_t n, const char *fmt, va_list args)
{
    (void)fmt; (void)args;
    if (buf && n > 0) buf[0] = '\0';
    return 0;
}