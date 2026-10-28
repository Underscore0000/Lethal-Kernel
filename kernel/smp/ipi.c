/*
 * Lethal Kernel - Inter-Processor Interrupts Implementation
 *
 * v1.0: IPI handling for SMP with TLB shootdown support.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ipi.h"
#include "apic.h"
#include "cpu.h"
#include "smp.h"
#include "../core/console.h"
#include "../core/panic.h"

static volatile uint32_t ipi_ack_count = 0;

/* TLB shootdown variables */
static volatile uint32_t tlb_shootdown_count = 0;
static volatile uint64_t tlb_shootdown_addr = 0;

void ipi_send_cpu(uint32_t apic_id, uint32_t vector)
{
    lapic_send_ipi(apic_id, vector, ICR_DELIVERY_FIXED | ICR_LEVEL_ASSERT | ICR_DEST_MOD_PHYS);
}

void ipi_send_all_except_self(uint32_t vector)
{
    uint32_t self_id = lapic_get_id();
    for (int i = 0; i < MAX_CPUS; i++) {
        if (cpus[i].flags & CPU_FLAG_PRESENT && cpus[i].state == CPU_STATE_ONLINE) {
            if (cpus[i].apic_id != self_id) {
                ipi_send_cpu(cpus[i].apic_id, vector);
            }
        }
    }
}

void ipi_send_all(uint32_t vector)
{
    for (int i = 0; i < MAX_CPUS; i++) {
        if (cpus[i].flags & CPU_FLAG_PRESENT && cpus[i].state == CPU_STATE_ONLINE) {
            ipi_send_cpu(cpus[i].apic_id, vector);
        }
    }
}

void ipi_handle_schedule(void)
{
    /* Called when receiving a scheduling IPI */
    struct cpu_info *cpu = cpu_current();
    if (cpu) {
        cpu->ipi_count++;
    }
    lapic_eoi();
}

void ipi_handle_sync(void)
{
    /* Synchronization IPI - used for barriers */
    __sync_synchronize();
    ipi_ack_count++;
    lapic_eoi();
}

void ipi_handle_stop(void)
{
    /* Stop CPU - used for crash/kdump */
    struct cpu_info *cpu = cpu_current();
    cpu->state = CPU_STATE_HALTED;
    lapic_eoi();
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void ipi_handle_tlb_flush(void)
{
    /* Flush TLB on this CPU */
    if (tlb_shootdown_addr) {
        __asm__ volatile ("invlpg (%0)" :: "r"(tlb_shootdown_addr) : "memory");
    } else {
        /* Flush entire TLB */
        uint64_t cr3;
        __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
        __asm__ volatile ("mov %0, %%cr3" :: "r"(cr3) : "memory");
    }
    tlb_shootdown_count++;
    lapic_eoi();
}

void ipi_handle_debug(void)
{
    /* Debug IPI - break into debugger */
    __asm__ volatile ("int3");
    lapic_eoi();
}

void ipi_init(void)
{
    serial_print("[ipi] initialized\n");
}

/* Wait for IPI acknowledgement from all CPUs */
void ipi_sync_barrier(uint32_t expected_count)
{
    ipi_ack_count = 0;
    ipi_send_all(IPI_VECTOR_SYNC);
    
    while (ipi_ack_count < expected_count - 1) {
        __asm__ volatile ("pause");
    }
}

/* TLB shootdown - invalidate TLB on all CPUs */
void ipi_tlb_shootdown(uint64_t addr)
{
    tlb_shootdown_addr = addr;
    tlb_shootdown_count = 0;
    
    /* Send IPI to all CPUs except self */
    ipi_send_all_except_self(IPI_VECTOR_TLB_FLUSH);
    
    /* Wait for all CPUs to acknowledge */
    uint32_t expected = smp_cpu_count() - 1;
    uint32_t timeout = 1000;
    while (tlb_shootdown_count < expected && timeout > 0) {
        __asm__ volatile ("pause");
        timeout--;
    }
}