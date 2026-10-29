/*
 * Lethal Kernel - CMOS Real Time Clock reader
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "rtc.h"
#include "../../arch/x86_64/io.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

static int rtc_update_in_progress(void)
{
    outb(CMOS_ADDRESS, 0x0A);
    return inb(CMOS_DATA) & 0x80;
}

static uint8_t bcd_to_bin(uint8_t val)
{
    return (uint8_t)((val & 0x0F) + ((val / 16) * 10));
}

static void read_raw(struct rtc_time *out)
{
    while (rtc_update_in_progress()) {
        /* spin */
    }
    out->second = cmos_read(0x00);
    out->minute = cmos_read(0x02);
    out->hour   = cmos_read(0x04);
    out->day    = cmos_read(0x07);
    out->month  = cmos_read(0x08);
    out->year   = cmos_read(0x09);
}

void rtc_read(struct rtc_time *out)
{
    struct rtc_time a, b;

    /* Read twice and compare to reject a read torn by an update tick. */
    do {
        read_raw(&a);
        read_raw(&b);
    } while (a.second != b.second || a.minute != b.minute || a.hour != b.hour ||
             a.day != b.day || a.month != b.month || a.year != b.year);

    uint8_t status_b = cmos_read(0x0B);
    int is_bcd = !(status_b & 0x04);

    if (is_bcd) {
        a.second = bcd_to_bin(a.second);
        a.minute = bcd_to_bin(a.minute);
        a.hour   = (uint8_t)(bcd_to_bin(a.hour & 0x7F) | (a.hour & 0x80));
        a.day    = bcd_to_bin(a.day);
        a.month  = bcd_to_bin(a.month);
        a.year   = bcd_to_bin((uint8_t)a.year);
    }

    /* Assume 21st century (standard practice for CMOS, which only stores
     * a 2-digit year in this basic register set). */
    out->second = a.second;
    out->minute = a.minute;
    out->hour   = (uint8_t)(a.hour & 0x7F);
    out->day    = a.day;
    out->month  = a.month;
    out->year   = (uint16_t)(2000 + a.year);
}
