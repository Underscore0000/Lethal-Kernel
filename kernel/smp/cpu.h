/*
 * Lethal Kernel - Per-CPU Data Structures
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_SMP_CPU_H
#define LETHAL_SMP_CPU_H

#include <stdint.h>
#include <stddef.h>

#define MAX_CPUS 8

/* CPU states */
#define CPU_STATE_OFFLINE   0
#define CPU_STATE_STARTING  1
#define CPU_STATE_ONLINE    2
#define CPU_STATE_HALTED    3

/* CPU flags */
#define CPU_FLAG_PRESENT    (1 << 0)
#define CPU_FLAG_ENABLED    (1 << 1)

/* Per-CPU data structure - each CPU has one */
struct cpu_info {
    /* APIC identification */
    uint32_t    apic_id;            /* Local APIC ID */
    uint32_t    acpi_processor_id;  /* ACPI processor ID */
    uint32_t    flags;              /* CPU_FLAG_* */
    uint32_t    state;              /* CPU_STATE_* */
    
    /* Bootstrap */
    uint8_t     booted;             /* 1 = booted successfully */
    uint32_t    sipi_count;         /* Number of SIPIs sent */
    uint64_t    boot_stack_top;     /* Stack top during boot */
    uint64_t    boot_stack_bottom;  /* Stack bottom */
    
    /* Kernel data */
    uint64_t    kernel_stack;       /* Current kernel stack */
    uint64_t    tss;                /* TSS address for this CPU */
    uint16_t    tss_selector;       /* TSS selector in GDT */
    
    /* Scheduler */
    void        *current_task;      /* Currently running task */
    void        *ready_queue;       /* Ready queue for this CPU */
    int         task_count;         /* Number of tasks on this CPU */
    
    /* Time */
    uint64_t    ticks;              /* Local tick counter */
    uint64_t    last_tick;          /* Last tick timestamp */
    
    /* Statistics */
    uint64_t    interrupts;         /* Total interrupts handled */
    uint64_t    ipi_count;          /* IPIs received */
    uint64_t    context_switches;   /* Context switches performed */
} __attribute__((aligned(64)));      /* Align to cache line to prevent false sharing */

/* Global CPU array */
extern struct cpu_info cpus[MAX_CPUS];
extern int num_cpus;
extern int boot_cpu_id;

/* Per-CPU accessor - uses GS segment */
static inline struct cpu_info *cpu_current(void)
{
    struct cpu_info *cpu;
    __asm__ volatile ("mov %%gs:0, %0" : "=r"(cpu));
    return cpu;
}

static inline int cpu_id(void)
{
    int id;
    __asm__ volatile ("mov %%gs:4, %0" : "=r"(id));
    return id;
}

/* Initialize GS for a CPU to point to its cpu_info */
void cpu_set_gs_base(uint64_t base);

/* Get CPU info by APIC ID */
struct cpu_info *cpu_by_apic_id(uint32_t apic_id);

/* Get CPU info by index */
struct cpu_info *cpu_by_index(int index);

/* Called by AP trampoline to set up per-CPU data for a new CPU */
void cpu_ap_init(uint32_t apic_id, uint32_t acpi_id);

/* Initialize per-CPU data */
void cpu_init(void);

#endif /* LETHAL_SMP_CPU_H */