/*
 * Lethal Kernel - Inter-Processor Interrupts
 *
 * v1.0: IPI interface for SMP with TLB shootdown.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_SMP_IPI_H
#define LETHAL_SMP_IPI_H

#include <stdint.h>

/* IPI vector numbers (software defined) */
#define IPI_VECTOR_SCHEDULE   0xF0
#define IPI_VECTOR_SYNC       0xF1
#define IPI_VECTOR_STOP       0xF2
#define IPI_VECTOR_TLB_FLUSH  0xF3
#define IPI_VECTOR_DEBUG      0xF4

/* IPI types */
#define IPI_TYPE_SCHEDULE     0
#define IPI_TYPE_SYNC         1
#define IPI_TYPE_STOP         2
#define IPI_TYPE_TLB_FLUSH    3
#define IPI_TYPE_DEBUG        4

/* Send IPI to a specific CPU */
void ipi_send_cpu(uint32_t apic_id, uint32_t vector);

/* Send IPI to all CPUs except self */
void ipi_send_all_except_self(uint32_t vector);

/* Send IPI to all CPUs */
void ipi_send_all(uint32_t vector);

/* IPI handlers (called from IDT) */
void ipi_handle_schedule(void);
void ipi_handle_sync(void);
void ipi_handle_stop(void);
void ipi_handle_tlb_flush(void);
void ipi_handle_debug(void);

/* Initialize IPI system */
void ipi_init(void);

/* Wait for IPI acknowledgement from all CPUs */
void ipi_sync_barrier(uint32_t expected_count);

/* TLB shootdown - invalidate TLB on all CPUs */
void ipi_tlb_shootdown(uint64_t addr);

#endif /* LETHAL_SMP_IPI_H */