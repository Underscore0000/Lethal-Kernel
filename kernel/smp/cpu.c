/*
 * Lethal Kernel - Per-CPU Data Management
 *
 * v1.0: Added per-CPU data structures and GS base management for SMP.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "cpu.h"
#include "../core/console.h"
#include "../core/panic.h"
#include "../memory/pmm.h"
#include "../memory/vmm.h"
#include "../../arch/x86_64/msr.h"

/* Global CPU array */
struct cpu_info cpus[MAX_CPUS] __attribute__((aligned(64)));
int num_cpus = 1;
int boot_cpu_id = 0;

/* Boot CPU's stack (already defined in entry.asm) */
extern char kernel_stack_top[];

static void cpu_init_info(int index, uint32_t apic_id, uint32_t acpi_id)
{
    struct cpu_info *cpu = &cpus[index];
    
    /* Zero the structure first */
    for (size_t i = 0; i < sizeof(struct cpu_info); i++) {
        ((uint8_t *)cpu)[i] = 0;
    }
    
    cpu->apic_id = apic_id;
    cpu->acpi_processor_id = acpi_id;
    cpu->flags = CPU_FLAG_PRESENT | CPU_FLAG_ENABLED;
    cpu->state = CPU_STATE_OFFLINE;
    cpu->booted = 0;
    
    /* Allocate kernel stack for this CPU if not boot CPU */
    if (index != boot_cpu_id) {
        uint64_t stack_frame = pmm_alloc_frame();
        if (!stack_frame) {
            panic("cpu_init: failed to allocate stack for CPU");
        }
        cpu->boot_stack_bottom = stack_frame;
        cpu->boot_stack_top = stack_frame + 4096;
        cpu->kernel_stack = cpu->boot_stack_top;
    } else {
        cpu->kernel_stack = (uint64_t)kernel_stack_top;
        cpu->state = CPU_STATE_ONLINE;
        cpu->booted = 1;
    }
    
    /* TSS will be set later by the GDT */
    cpu->tss = 0;
    cpu->tss_selector = 0;
}

void cpu_init(void)
{
    serial_print("[cpu] initializing per-CPU data\n");
    
    /* Initialize all entries */
    for (int i = 0; i < MAX_CPUS; i++) {
        cpus[i].apic_id = 0xFFFFFFFF;
        cpus[i].state = CPU_STATE_OFFLINE;
        cpus[i].booted = 0;
    }
    
    /* Initialize boot CPU (index 0) */
    /* APIC ID for boot CPU will be set by APIC initialization */
    cpu_init_info(0, 0, 0);
    boot_cpu_id = 0;
    num_cpus = 1;
    
    /* Set GS base to point to boot CPU data */
    cpu_set_gs_base((uint64_t)(uintptr_t)&cpus[0]);
    
    serial_print("[cpu] boot CPU initialized\n");
}

void cpu_set_gs_base(uint64_t base)
{
    /* Set GS base using MSR */
    write_msr(0xC0000101, base);  /* IA32_GS_BASE */
    write_msr(0xC0000102, base);  /* IA32_KERNEL_GS_BASE */
}

struct cpu_info *cpu_by_apic_id(uint32_t apic_id)
{
    for (int i = 0; i < MAX_CPUS; i++) {
        if (cpus[i].apic_id == apic_id && (cpus[i].flags & CPU_FLAG_PRESENT)) {
            return &cpus[i];
        }
    }
    return NULL;
}

struct cpu_info *cpu_by_index(int index)
{
    if (index < 0 || index >= MAX_CPUS) {
        return NULL;
    }
    if (!(cpus[index].flags & CPU_FLAG_PRESENT)) {
        return NULL;
    }
    return &cpus[index];
}

/* Called by AP trampoline to set up per-CPU data for a new CPU */
void cpu_ap_init(uint32_t apic_id, uint32_t acpi_id)
{
    int index = -1;
    
    /* Find first available CPU slot */
    for (int i = 1; i < MAX_CPUS; i++) {
        if (!(cpus[i].flags & CPU_FLAG_PRESENT)) {
            index = i;
            break;
        }
    }
    
    if (index < 0) {
        panic("cpu_ap_init: no free CPU slots");
        return;
    }
    
    /* Initialize the CPU info */
    cpu_init_info(index, apic_id, acpi_id);
    cpus[index].state = CPU_STATE_ONLINE;
    cpus[index].booted = 1;
    
    /* Set GS base to point to this CPU's data */
    cpu_set_gs_base((uint64_t)(uintptr_t)&cpus[index]);
    
    num_cpus++;
    
    serial_print("[cpu] AP ");
    serial_print_dec(index);
    serial_print(" (APIC ID ");
    serial_print_dec(apic_id);
    serial_print(") online\n");
}