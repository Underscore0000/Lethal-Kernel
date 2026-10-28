/*
 * Lethal Kernel - I/O APIC Interface
 *
 * v1.0: I/O APIC driver for SMP interrupt routing.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_SMP_IOAPIC_H
#define LETHAL_SMP_IOAPIC_H

#include <stdint.h>

#define IOAPIC_MAX_ENTRIES 24

/* I/O APIC registers */
#define IOAPIC_IOREGSEL    0x00
#define IOAPIC_IOWIN       0x10
#define IOAPIC_ID          0x00
#define IOAPIC_VERSION     0x01
#define IOAPIC_ARBITRATION 0x02
#define IOAPIC_REDTBL_BASE 0x10

/* I/O APIC redirection table entry flags */
#define IOAPIC_REDTBL_MASKED  (1 << 16)
#define IOAPIC_REDTBL_PHYSICAL (0 << 8)
#define IOAPIC_REDTBL_LOGICAL  (1 << 8)
#define IOAPIC_REDTBL_EDGE    (0 << 15)
#define IOAPIC_REDTBL_LEVEL   (1 << 15)
#define IOAPIC_REDTBL_LOW     (0 << 13)
#define IOAPIC_REDTBL_HIGH    (1 << 13)
#define IOAPIC_REDTBL_FIXED   (0 << 8)
#define IOAPIC_REDTBL_LOWEST  (1 << 8)
#define IOAPIC_REDTBL_SMI     (2 << 8)
#define IOAPIC_REDTBL_NMI     (4 << 8)
#define IOAPIC_REDTBL_INIT    (5 << 8)
#define IOAPIC_REDTBL_EXTINT  (7 << 8)

struct ioapic_info {
    uint32_t    ioapic_id;
    uint32_t    version;
    uint32_t    max_redirection;
    uint64_t    base_address;
    uint32_t    global_interrupt_base;
    uint32_t    redtbl_entries;
    uint8_t     present;
    volatile uint32_t *base;
};

/* Initialize I/O APIC(s) */
void ioapic_init(void);

/* Find I/O APIC by ID */
struct ioapic_info *ioapic_find(uint32_t ioapic_id);

/* Redirect an interrupt to a CPU */
void ioapic_redirect_irq(uint32_t irq, uint32_t vector, uint32_t apic_id, uint32_t flags);

/* Mask/unmask an interrupt */
void ioapic_mask_irq(uint32_t irq);
void ioapic_unmask_irq(uint32_t irq);

/* Get number of I/O APICs - renamed to avoid conflict */
uint32_t ioapic_get_count(void);

/* Get I/O APIC info */
struct ioapic_info *ioapic_get(uint32_t index);

/* Legacy PIC disable/enable */
void ioapic_disable_pic(void);
void ioapic_enable_pic(void);

#endif /* LETHAL_SMP_IOAPIC_H */