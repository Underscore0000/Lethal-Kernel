/*
 * Lethal Kernel - PIT (8253/8254) timer driver
 *
 * Programs channel 0 of the legacy Programmable Interval Timer to fire
 * IRQ0 at a fixed frequency, and counts ticks so the rest of the kernel
 * has a notion of elapsed time (timer_ms) and can sleep.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "timer.h"
#include "../../arch/x86_64/io.h"
#include "console.h"

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND       0x43
#define PIT_BASE_FREQ     1193182u

static volatile uint64_t ticks;
static uint32_t configured_hz;

void timer_init(uint32_t hz)
{
    ticks = 0;
    configured_hz = hz;

    uint32_t divisor = PIT_BASE_FREQ / hz;
    if (divisor > 0xFFFF) {
        divisor = 0xFFFF;
    }

    outb(PIT_COMMAND, 0x36); /* channel 0, lobyte/hibyte, mode 3 (square wave) */
    outb(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_DATA, (uint8_t)((divisor >> 8) & 0xFF));

    serial_print("[timer] PIT programmed at ");
    serial_print_dec(hz);
    serial_print(" Hz\n");
}

void timer_tick_handler(void)
{
    ticks++;
}

uint64_t timer_ticks(void)
{
    return ticks;
}

uint64_t timer_ms(void)
{
    if (configured_hz == 0) {
        return 0;
    }
    return (ticks * 1000ULL) / configured_hz;
}

void timer_sleep_ms(uint64_t ms)
{
    uint64_t target = timer_ms() + ms;
    while (timer_ms() < target) {
        __asm__ volatile ("hlt");
    }
}
