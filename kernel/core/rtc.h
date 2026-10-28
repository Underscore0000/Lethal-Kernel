#ifndef LETHAL_RTC_H
#define LETHAL_RTC_H

#include <stdint.h>

struct rtc_time {
    uint8_t  second;
    uint8_t  minute;
    uint8_t  hour;
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
};

/* Reads the current date/time from the CMOS RTC, waiting out any in
 * progress update and reading twice to reject a torn read. */
void rtc_read(struct rtc_time *out);

#endif
