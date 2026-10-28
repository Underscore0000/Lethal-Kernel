/*
 * Lethal Kernel - Local APIC Implementation
 *
 * v1.0: Local APIC driver for SMP support.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "apic.h"
#include "../core/console.h"
#include "../core/panic.h"
#include "../core/timer.h"
#include "../../arch/x86_64/msr.h"
#include "../../arch/x86_64/io.h"

/* APIC base address - can be relocated but we use default */
static volatile uint32_t *lapic_base = (volatile uint32_t *)LAPIC_BASE_DEFAULT;

/* Timer calibration */
static uint64_t lapic_timer_hz = 0;
static uint32_t lapic_timer_divisor = LAPIC_TIMER_DCR_DIV16;

/* AP boot counter */
static volatile uint32_t ap_boot_counter = 0;

void lapic_write(uint32_t reg, uint32_t value)
{
    lapic_base[reg / 4] = value;
}

uint32_t lapic_read(uint32_t reg)
{
    return lapic_base[reg / 4];
}

void lapic_enable(void)
{
    /* Enable APIC via MSR */
    uint64_t msr = read_msr(MSR_APIC_BASE);
    msr |= MSR_APIC_BASE_EN;
    write_msr(MSR_APIC_BASE, msr);
    
    /* Set spurious interrupt vector with software enable */
    uint32_t sivr = lapic_read(LAPIC_SIVR);
    sivr &= ~LAPIC_SIVR_VECTOR;
    sivr |= LAPIC_SPURIOUS_VECTOR;
    sivr |= LAPIC_SIVR_ENABLE;
    lapic_write(LAPIC_SIVR, sivr);
    
    /* Clear error status register */
    lapic_write(LAPIC_ESR, 0);
    lapic_write(LAPIC_ESR, 0);
    
    /* Set task priority to 0 (allow all interrupts) */
    lapic_write(LAPIC_TPR, 0);
    
    /* Set logical destination to NMI */
    lapic_write(LAPIC_LDR, (lapic_read(LAPIC_LDR) & 0x00FFFFFF) | (1 << 24));
    
    /* Set destination format to flat mode */
    lapic_write(LAPIC_DFR, 0xFFFFFFFF);
}

void lapic_disable(void)
{
    /* Disable APIC via MSR */
    uint64_t msr = read_msr(MSR_APIC_BASE);
    msr &= ~MSR_APIC_BASE_EN;
    write_msr(MSR_APIC_BASE, msr);
}

void lapic_init(void)
{
    serial_print("[lapic] initializing...\n");
    
    /* Check if APIC is already enabled */
    uint64_t msr = read_msr(MSR_APIC_BASE);
    if (!(msr & MSR_APIC_BASE_EN)) {
        serial_print("[lapic] APIC not enabled, enabling...\n");
        lapic_enable();
    }
    
    uint32_t version = lapic_read(LAPIC_VERSION);
    serial_print("[lapic] version: 0x");
    serial_print_hex(version);
    serial_print(" (max LVT entries: ");
    serial_print_dec((version >> 16) & 0xFF);
    serial_print(")\n");
    
    uint32_t id = lapic_get_id();
    serial_print("[lapic] APIC ID: ");
    serial_print_dec(id);
    serial_print("\n");
    
    /* Mask all LVT entries initially */
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_THERMAL, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_PERFMON, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT0, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT1, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_ERROR, LAPIC_LVT_MASKED);
    
    serial_print("[lapic] initialized\n");
}

uint32_t lapic_get_id(void)
{
    return lapic_read(LAPIC_ID) >> 24;
}

uint32_t lapic_get_version(void)
{
    return lapic_read(LAPIC_VERSION);
}

void lapic_eoi(void)
{
    lapic_write(LAPIC_EOI, 0);
}

void lapic_send_ipi(uint32_t dest_apic_id, uint32_t vector, uint32_t flags)
{
    /* Wait for any previous IPI to complete */
    while (lapic_read(LAPIC_ICR_LOW) & ICR_STATUS_MASK) {
        __asm__ volatile ("pause");
    }
    
    /* Set destination */
    lapic_write(LAPIC_ICR_HIGH, dest_apic_id << 24);
    
    /* Set vector and flags */
    uint32_t icr = vector | flags;
    lapic_write(LAPIC_ICR_LOW, icr);
}

void lapic_send_init_ipi(uint32_t dest_apic_id)
{
    serial_print("[lapic] sending INIT IPI to APIC ID ");
    serial_print_dec(dest_apic_id);
    serial_print("\n");
    
    lapic_send_ipi(dest_apic_id, 0, ICR_DELIVERY_INIT | ICR_LEVEL_ASSERT);
    
    /* Wait for IPI to complete */
    while (lapic_read(LAPIC_ICR_LOW) & ICR_STATUS_MASK) {
        __asm__ volatile ("pause");
    }
    
    /* Deassert INIT after 10ms */
    timer_sleep_ms(10);
    lapic_send_ipi(dest_apic_id, 0, ICR_DELIVERY_INIT | ICR_LEVEL_DEASSERT);
    
    while (lapic_read(LAPIC_ICR_LOW) & ICR_STATUS_MASK) {
        __asm__ volatile ("pause");
    }
    
    timer_sleep_ms(10);
}

void lapic_send_startup_ipi(uint32_t dest_apic_id, uint8_t vector)
{
    serial_print("[lapic] sending SIPI with vector 0x");
    serial_print_hex(vector);
    serial_print(" to APIC ID ");
    serial_print_dec(dest_apic_id);
    serial_print("\n");
    
    lapic_send_ipi(dest_apic_id, vector, ICR_DELIVERY_STARTUP);
    
    while (lapic_read(LAPIC_ICR_LOW) & ICR_STATUS_MASK) {
        __asm__ volatile ("pause");
    }
}

/* ============================================================================
 * LAPIC Timer Functions
 * ============================================================================ */

static void lapic_timer_calibrate(void)
{
    /* Calibrate LAPIC timer against PIT */
    serial_print("[lapic] calibrating timer...\n");
    
    /* Use divide by 16 for calibration */
    lapic_write(LAPIC_TIMER_DCR, LAPIC_TIMER_DCR_DIV16);
    lapic_timer_divisor = LAPIC_TIMER_DCR_DIV16;
    
    /* Set initial count to maximum */
    lapic_write(LAPIC_TIMER_ICR, 0xFFFFFFFF);
    
    /* Wait 10ms using PIT */
    timer_sleep_ms(10);
    
    /* Read current count */
    uint32_t current = lapic_read(LAPIC_TIMER_CCR);
    uint32_t elapsed = 0xFFFFFFFF - current;
    
    /* Calculate frequency: elapsed ticks per 10ms = elapsed * 100 Hz */
    lapic_timer_hz = (uint64_t)elapsed * 100ULL;
    
    /* If frequency is unreasonably low, try different divisor */
    if (lapic_timer_hz < 1000) {
        serial_print("[lapic] calibration failed, using fallback\n");
        lapic_timer_hz = 1000000; /* 1 MHz fallback */
    }
    
    serial_print("[lapic] timer frequency: ");
    serial_print_dec(lapic_timer_hz);
    serial_print(" Hz\n");
}

void lapic_timer_init(uint32_t hz)
{
    serial_print("[lapic_timer] initializing...\n");
    
    /* Calibrate if not already done */
    if (lapic_timer_hz == 0) {
        lapic_timer_calibrate();
    }
    
    /* Set timer to masked initially */
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED);
    
    /* Set divide configuration */
    lapic_write(LAPIC_TIMER_DCR, lapic_timer_divisor);
    
    /* Calculate initial count */
    uint32_t initial_count = (uint32_t)(lapic_timer_hz / hz);
    
    /* Set timer to periodic mode with the vector */
    uint32_t lvt = LAPIC_SPURIOUS_VECTOR;
    lapic_write(LAPIC_LVT_TIMER, lvt);
    
    /* Set initial count */
    lapic_write(LAPIC_TIMER_ICR, initial_count);
    
    serial_print("[lapic_timer] initialized at ");
    serial_print_dec(hz);
    serial_print(" Hz\n");
}

void lapic_timer_set_periodic(uint32_t initial_count)
{
    uint32_t lvt = lapic_read(LAPIC_LVT_TIMER);
    lvt &= ~LAPIC_LVT_MASKED;
    lvt |= LAPIC_SPURIOUS_VECTOR;
    lapic_write(LAPIC_LVT_TIMER, lvt);
    lapic_write(LAPIC_TIMER_ICR, initial_count);
}

void lapic_timer_set_oneshot(uint32_t initial_count)
{
    uint32_t lvt = lapic_read(LAPIC_LVT_TIMER);
    lvt &= ~LAPIC_LVT_MASKED;
    lvt |= LAPIC_SPURIOUS_VECTOR | (1 << 17);  /* Oneshot mode */
    lapic_write(LAPIC_LVT_TIMER, lvt);
    lapic_write(LAPIC_TIMER_ICR, initial_count);
}

void lapic_timer_stop(void)
{
    uint32_t lvt = lapic_read(LAPIC_LVT_TIMER);
    lvt |= LAPIC_LVT_MASKED;
    lapic_write(LAPIC_LVT_TIMER, lvt);
}

uint64_t lapic_timer_get_ticks(void)
{
    uint32_t current = lapic_read(LAPIC_TIMER_CCR);
    uint32_t initial = lapic_read(LAPIC_TIMER_ICR);
    return (uint64_t)initial - current;
}

/* ============================================================================
 * AP Boot Synchronization Functions
 * ============================================================================ */

void lapic_ap_boot_report(void)
{
    /* Called by an AP when it has successfully booted */
    ap_boot_counter++;
    serial_print("[lapic] AP boot reported, total: ");
    serial_print_dec(ap_boot_counter);
    serial_print("\n");
}

int lapic_ap_boot_wait(int expected_cpus, int timeout_ms)
{
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        if (ap_boot_counter >= (uint32_t)expected_cpus) {
            return 1;
        }
        timer_sleep_ms(1);
        elapsed++;
    }
    return 0;
}