/*
 * Lethal Kernel - Standard I/O implementation for kernel
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>



int vsnprintf(char *str, size_t size, const char *format, va_list args)
{
    if (!str || size == 0) return 0;
    
    size_t written = 0;
    const char *f = format;
    
    while (*f != '\0' && written < size - 1) {
        if (*f == '%') {
            f++;
            if (*f == '\0') break;
            
            if (*f == 's') {
                const char *s = va_arg(args, const char *);
                if (s) {
                    while (*s != '\0' && written < size - 1) {
                        str[written++] = *s++;
                    }
                }
                f++;
                continue;
            } else if (*f == 'd' || *f == 'i') {
                int num = va_arg(args, int);
                char buf[16];
                int pos = 0;
                
                if (num == 0) {
                    buf[pos++] = '0';
                } else {
                    if (num < 0) {
                        if (written < size - 1) str[written++] = '-';
                        num = -num;
                    }
                    int temp = num;
                    while (temp > 0) {
                        buf[pos++] = '0' + (temp % 10);
                        temp /= 10;
                    }
                }
                while (pos > 0 && written < size - 1) {
                    str[written++] = buf[--pos];
                }
                f++;
                continue;
            } else if (*f == 'u') {
                unsigned int num = va_arg(args, unsigned int);
                char buf[16];
                int pos = 0;
                
                if (num == 0) {
                    buf[pos++] = '0';
                } else {
                    while (num > 0) {
                        buf[pos++] = '0' + (num % 10);
                        num /= 10;
                    }
                }
                while (pos > 0 && written < size - 1) {
                    str[written++] = buf[--pos];
                }
                f++;
                continue;
            } else if (*f == 'x' || *f == 'X') {
                unsigned int num = va_arg(args, unsigned int);
                char buf[16];
                int pos = 0;
                const char *hex = (*f == 'x') ? "0123456789abcdef" : "0123456789ABCDEF";
                
                if (num == 0) {
                    buf[pos++] = '0';
                } else {
                    while (num > 0) {
                        buf[pos++] = hex[num & 0xF];
                        num >>= 4;
                    }
                }
                while (pos > 0 && written < size - 1) {
                    str[written++] = buf[--pos];
                }
                f++;
                continue;
            } else if (*f == 'p') {
                void *ptr = va_arg(args, void *);
                uintptr_t num = (uintptr_t)ptr;
                char buf[16];
                int pos = 0;
                const char *hex = "0123456789abcdef";
                
                str[written++] = '0';
                if (written < size - 1) str[written++] = 'x';
                
                if (num == 0) {
                    if (written < size - 1) str[written++] = '0';
                } else {
                    while (num > 0) {
                        buf[pos++] = hex[num & 0xF];
                        num >>= 4;
                    }
                    while (pos > 0 && written < size - 1) {
                        str[written++] = buf[--pos];
                    }
                }
                f++;
                continue;
            } else if (*f == '%') {
                if (written < size - 1) str[written++] = '%';
                f++;
                continue;
            } else {
                /* Formato non supportato, scrivi % e il carattere */
                if (written < size - 1) str[written++] = '%';
                if (written < size - 1) str[written++] = *f;
                f++;
                continue;
            }
        }
        str[written++] = *f++;
    }
    
    str[written] = '\0';
    return (int)written;
}


int vsscanf(const char *str, const char *format, va_list args)
{
    int count = 0;
    const char *f = format;
    const char *s = str;
    
    while (*f != '\0' && *s != '\0') {
        if (*f == '%') {
            f++;
            if (*f == '\0') break;
            
            if (*f == 'd' || *f == 'i') {
                int *p = va_arg(args, int *);
                if (p) {
                    int val = 0;
                    int sign = 1;
                    
                    while (*s == ' ') s++;
                    if (*s == '-') { sign = -1; s++; }
                    while (*s >= '0' && *s <= '9') {
                        val = val * 10 + (*s - '0');
                        s++;
                    }
                    *p = val * sign;
                    count++;
                }
                f++;
                continue;
            } else if (*f == 'u') {
                unsigned int *p = va_arg(args, unsigned int *);
                if (p) {
                    unsigned int val = 0;
                    while (*s == ' ') s++;
                    while (*s >= '0' && *s <= '9') {
                        val = val * 10 + (*s - '0');
                        s++;
                    }
                    *p = val;
                    count++;
                }
                f++;
                continue;
            } else if (*f == 'x' || *f == 'X') {
                unsigned int *p = va_arg(args, unsigned int *);
                if (p) {
                    unsigned int val = 0;
                    while (*s == ' ') s++;
                    while ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F')) {
                        int digit;
                        if (*s >= '0' && *s <= '9') digit = *s - '0';
                        else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
                        else digit = *s - 'A' + 10;
                        val = val * 16 + digit;
                        s++;
                    }
                    *p = val;
                    count++;
                }
                f++;
                continue;
            } else if (*f == 'h' && *(f+1) == 'u') {
                unsigned short *p = va_arg(args, unsigned short *);
                if (p) {
                    unsigned int val = 0;
                    while (*s == ' ') s++;
                    while (*s >= '0' && *s <= '9') {
                        val = val * 10 + (*s - '0');
                        s++;
                    }
                    *p = (unsigned short)val;
                    count++;
                }
                f += 2;
                continue;
            } else if (*f == 's') {
                char *p = va_arg(args, char *);
                if (p) {
                    while (*s == ' ') s++;
                    while (*s != '\0' && *s != ' ' && *s != '\t' && *s != '\n') {
                        *p++ = *s++;
                    }
                    *p = '\0';
                    count++;
                }
                f++;
                continue;
            } else {
                f++;
                continue;
            }
        }
        if (*f == *s) {
            f++;
            s++;
        } else {
            break;
        }
    }
    
    return count;
}


int snprintf(char *str, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}


int __snprintf_chk(char *str, size_t maxlen, int flag, size_t dstlen, const char *format, ...)
{
    (void)flag;
    (void)dstlen;
    
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, maxlen, format, args);
    va_end(args);
    return result;
}


int sscanf(const char *str, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsscanf(str, format, args);
    va_end(args);
    return result;
}


int __isoc99_sscanf(const char *str, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsscanf(str, format, args);
    va_end(args);
    return result;
}



static uint32_t rand_seed = 123456789;

int rand(void)
{
    rand_seed = (rand_seed * 1103515245 + 12345) & 0x7FFFFFFF;
    return (int)rand_seed;
}

void srand(unsigned int seed)
{
    rand_seed = seed;
}