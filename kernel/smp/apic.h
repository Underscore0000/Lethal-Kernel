/*
 * Lethal Kernel - Local APIC Interface
 *
 * v1.0: Local APIC driver interface for SMP support.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_SMP_APIC_H
#define LETHAL_SMP_APIC_H

#include <stdint.h>
#include "cpu.h"

/* Local APIC registers offsets (from base) */
#define LAPIC_ID           0x0020
#define LAPIC_VERSION      0x0030
#define LAPIC_TPR          0x0080
#define LAPIC_APR          0x0090
#define LAPIC_PPR          0x00A0
#define LAPIC_EOI          0x00B0
#define LAPIC_RRD          0x00C0
#define LAPIC_LDR          0x00D0
#define LAPIC_DFR          0x00E0
#define LAPIC_SIVR         0x00F0
#define LAPIC_ISR_BASE     0x0100
#define LAPIC_ISR_END      0x0170
#define LAPIC_TMR_BASE     0x0180
#define LAPIC_TMR_END      0x01F0
#define LAPIC_IRR_BASE     0x0200
#define LAPIC_IRR_END      0x0270
#define LAPIC_ESR          0x0280
#define LAPIC_ICR_LOW      0x0300
#define LAPIC_ICR_HIGH     0x0310
#define LAPIC_LVT_TIMER    0x0320
#define LAPIC_LVT_THERMAL  0x0330
#define LAPIC_LVT_PERFMON  0x0340
#define LAPIC_LVT_LINT0    0x0350
#define LAPIC_LVT_LINT1    0x0360
#define LAPIC_LVT_ERROR    0x0370
#define LAPIC_TIMER_ICR    0x0380
#define LAPIC_TIMER_CCR    0x0390
#define LAPIC_TIMER_DCR    0x03E0

/* LAPIC SIVR flags */
#define LAPIC_SIVR_ENABLE  (1 << 8)
#define LAPIC_SIVR_VECTOR  0xFF

/* LAPIC LVT flags */
#define LAPIC_LVT_MASKED   (1 << 16)
#define LAPIC_LVT_EDGE     (0 << 15)
#define LAPIC_LVT_LEVEL    (1 << 15)
#define LAPIC_LVT_REMOTE   (1 << 14)
#define LAPIC_LVT_SEND_PENDING (1 << 12)
#define LAPIC_LVT_STATUS_MASK (1 << 12)
#define LAPIC_LVT_POLARITY_HIGH (0 << 13)
#define LAPIC_LVT_POLARITY_LOW  (1 << 13)

/* ICR flags */
#define ICR_DEST_SHIFT     0
#define ICR_DEST_MASK      0x000000FF
#define ICR_VECTOR_SHIFT   0
#define ICR_VECTOR_MASK    0x000000FF
#define ICR_DEST_MOD_SHIFT 8
#define ICR_DEST_MOD_MASK  (3 << 8)
#define ICR_DEST_MOD_PHYS  (0 << 8)
#define ICR_DEST_MOD_LOG   (1 << 8)
#define ICR_DELIVERY_SHIFT 8
#define ICR_DELIVERY_MASK  (7 << 8)
#define ICR_DELIVERY_FIXED (0 << 8)
#define ICR_DELIVERY_LPRI  (1 << 8)
#define ICR_DELIVERY_SMI   (2 << 8)
#define ICR_DELIVERY_NMI   (4 << 8)
#define ICR_DELIVERY_INIT  (5 << 8)
#define ICR_DELIVERY_STARTUP (6 << 8)
#define ICR_LEVEL_SHIFT    14
#define ICR_LEVEL_DEASSERT (0 << 14)
#define ICR_LEVEL_ASSERT   (1 << 14)
#define ICR_TRIGGER_SHIFT  15
#define ICR_TRIGGER_EDGE   (0 << 15)
#define ICR_TRIGGER_LEVEL  (1 << 15)
#define ICR_STATUS_SHIFT   12
#define ICR_STATUS_MASK    (1 << 12)

/* LAPIC timer DCR values */
#define LAPIC_TIMER_DCR_DIV1  0x0B
#define LAPIC_TIMER_DCR_DIV2  0x00
#define LAPIC_TIMER_DCR_DIV4  0x01
#define LAPIC_TIMER_DCR_DIV8  0x02
#define LAPIC_TIMER_DCR_DIV16 0x03
#define LAPIC_TIMER_DCR_DIV32 0x08
#define LAPIC_TIMER_DCR_DIV64 0x09
#define LAPIC_TIMER_DCR_DIV128 0x0A

/* Error codes from ESR */
#define LAPIC_ESR_SEND_CS    (1 << 0)
#define LAPIC_ESR_RECV_CS    (1 << 1)
#define LAPIC_ESR_SEND_ACC   (1 << 2)
#define LAPIC_ESR_RECV_ACC   (1 << 3)
#define LAPIC_ESR_SEND_PT    (1 << 4)
#define LAPIC_ESR_RECV_PT    (1 << 5)
#define LAPIC_ESR_SEND_ILL   (1 << 6)
#define LAPIC_ESR_RECV_ILL   (1 << 7)
#define LAPIC_ESR_ILLEGAL    (1 << 8)

/* APIC base address - defaults to 0xFEE00000 */
#define LAPIC_BASE_DEFAULT  0xFEE00000ULL

/* Spurious interrupt vector */
#define LAPIC_SPURIOUS_VECTOR 0xFF

/* Function prototypes */
void lapic_init(void);
void lapic_enable(void);
void lapic_disable(void);
uint32_t lapic_get_id(void);
uint32_t lapic_get_version(void);
void lapic_eoi(void);
void lapic_send_ipi(uint32_t dest_apic_id, uint32_t vector, uint32_t flags);
void lapic_send_init_ipi(uint32_t dest_apic_id);
void lapic_send_startup_ipi(uint32_t dest_apic_id, uint8_t vector);
void lapic_write(uint32_t reg, uint32_t value);
uint32_t lapic_read(uint32_t reg);

/* LAPIC timer functions */
void lapic_timer_init(uint32_t hz);
void lapic_timer_set_periodic(uint32_t initial_count);
void lapic_timer_set_oneshot(uint32_t initial_count);
void lapic_timer_stop(void);
uint64_t lapic_timer_get_ticks(void);

/* AP boot synchronization */
void lapic_ap_boot_report(void);
int lapic_ap_boot_wait(int expected_cpus, int timeout_ms);

#endif /* LETHAL_SMP_APIC_H */