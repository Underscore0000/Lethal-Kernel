/*
 * Lethal Kernel - string library implementation
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "../include/string.h"

/* --- memcpy --- */
void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dst;
}

/* --- memmove --- */
void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dst;
}

/* --- memset --- */
void *memset(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < n; i++)
        d[i] = (uint8_t)c;
    return dst;
}

/* --- memcmp --- */
int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i])
            return (int)pa[i] - (int)pb[i];
    }
    return 0;
}

/* --- strlen --- */
size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

/* --- strcpy --- */
char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++)) { }
    return dst;
}

/* --- strncpy --- */
char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = '\0';
    return dst;
}

/* --- strcat --- */
char *strcat(char *dst, const char *src)
{
    char *d = dst + strlen(dst);
    while ((*d++ = *src++)) { }
    return dst;
}

/* --- strncat --- */
char *strncat(char *dst, const char *src, size_t n)
{
    char *d = dst + strlen(dst);
    size_t i = 0;
    for (; i < n && src[i]; i++)
        d[i] = src[i];
    d[i] = '\0';
    return dst;
}

/* --- strcmp --- */
int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* --- strncmp --- */
int strncmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0')
            return 0;
    }
    return 0;
}

/* --- strchr --- */
char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c)
            return (char *)s;
        s++;
    }
    if ((char)c == '\0')
        return (char *)s;
    return NULL;
}

/* --- strrchr --- */
char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c)
            last = s;
        s++;
    }
    if ((char)c == '\0')
        return (char *)s;
    return (char *)last;
}

/* --- strstr --- */
char *strstr(const char *haystack, const char *needle)
{
    if (!*needle)
        return (char *)haystack;

    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n)
            return (char *)haystack;
    }
    return NULL;
}

/* --- strtok --- */
char *strtok(char *s, const char *delim)
{
    static char *saveptr = NULL;
    if (!s) s = saveptr;
    if (!s) return NULL;
    while (*s && strchr(delim, *s))
        s++;
    if (!*s) {
        saveptr = NULL;
        return NULL;
    }

    char *token = s;


    while (*s && !strchr(delim, *s))
        s++;

    if (*s) {
        *s = '\0';
        saveptr = s + 1;
    } else {
        saveptr = NULL;
    }

    return token;
}