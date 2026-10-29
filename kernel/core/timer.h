#ifndef LETHAL_TIMER_H
#define LETHAL_TIMER_H

#include <stdint.h>

/* Programs the 8253/8254 PIT to fire IRQ0 at `hz` times per second and
 * registers the internal tick handler. Call after idt_init(). */
void timer_init(uint32_t hz);

/* Total ticks since timer_init(). */
uint64_t timer_ticks(void);

/* Total milliseconds elapsed since timer_init(), derived from the
 * configured frequency. */
uint64_t timer_ms(void);

/* Busy-waits (via `hlt` between ticks) until at least `ms` milliseconds
 * have elapsed. */
void timer_sleep_ms(uint64_t ms);

/* Called by the IRQ0 stub (arch/x86_64/isr_stubs.asm -> idt.c irq_handler)
 * on every timer interrupt. */
void timer_tick_handler(void);

#endif
