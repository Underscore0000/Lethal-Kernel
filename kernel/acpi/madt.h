/*
 * Lethal Kernel - ACPI MADT Interface
 *
 * v1.0: Provides access to parsed MADT data for SMP.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_ACPI_MADT_H
#define LETHAL_ACPI_MADT_H

#include <stdint.h>
#include "acpi.h"

#define MAX_LAPIC_ENTRIES  32
#define MAX_IOAPIC_ENTRIES 8
#define MAX_ISO_ENTRIES    24
#define MAX_NMI_ENTRIES    16

struct madt_info {
    uint32_t local_apic_addr;
    uint32_t flags;
    uint32_t lapic_count;
    uint32_t ioapic_count;
    uint32_t iso_count;
    uint32_t lapic_nmi_count;
    struct madt_lapic lapics[MAX_LAPIC_ENTRIES];
    struct madt_ioapic ioapics[MAX_IOAPIC_ENTRIES];
    struct madt_iso isos[MAX_ISO_ENTRIES];
    struct madt_lapic_nmi lapic_nmis[MAX_NMI_ENTRIES];
};

void madt_parse(void);
struct madt_info *madt_get_info(void);
void *madt_find_entry(uint8_t type, int *index);

#endif /* LETHAL_ACPI_MADT_H */