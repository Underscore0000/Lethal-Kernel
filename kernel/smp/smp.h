/*
 * Lethal Kernel - SMP Interface
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_SMP_SMP_H
#define LETHAL_SMP_SMP_H

#include <stdint.h>
#include "cpu.h"

/* AP bootstrap trampoline location */
#define AP_TRAMPOLINE_BASE  0x8000
#define AP_TRAMPOLINE_SIZE  4096

/* SMP initialization phases */
#define SMP_PHASE_NONE      0
#define SMP_PHASE_ACPI      1
#define SMP_PHASE_APIC      2
#define SMP_PHASE_BOOT      3
#define SMP_PHASE_RUNNING   4

/* Initialize SMP subsystem */
void smp_init(void);

/* Boot AP (Application Processors) */
void smp_boot_aps(void);

/* Called by AP trampoline when a new CPU comes online */
void ap_bootstrap(uint32_t cpu_index);

/* Check if we are on the bootstrap processor */
int smp_is_bsp(void);

/* Get current CPU index */
int smp_current_cpu(void);

/* Get APIC ID of current CPU */
uint32_t smp_current_apic_id(void);

/* Get number of online CPUs */
int smp_cpu_count(void);

/* Wait for all APs to boot */
void smp_wait_for_aps(int timeout_ms);

/* Return whether SMP is enabled */
int smp_enabled(void);

/* SMP configuration */
struct smp_config {
    uint32_t    max_cpus;
    uint32_t    apic_base;
    uint8_t     enabled;
    uint8_t     use_ioapic;
    uint8_t     use_lapic_timer;
};

extern struct smp_config smp_config;

#endif /* LETHAL_SMP_SMP_H */