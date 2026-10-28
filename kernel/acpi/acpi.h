/*
 * Lethal Kernel - ACPI (Advanced Configuration and Power Interface)
 *
 * v1.0: Added ACPI table parsing for SMP support.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_ACPI_H
#define LETHAL_ACPI_H

#include <stdint.h>

/* RSDP (Root System Description Pointer) */
struct rsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

/* RSDT (Root System Description Table) */
struct rsdt {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
    /* entries follow */
} __attribute__((packed));

/* XSDT (Extended System Description Table) */
struct xsdt {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
    /* entries follow */
} __attribute__((packed));

/* MADT (Multiple APIC Description Table) */
struct madt {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
    uint32_t local_apic_addr;
    uint32_t flags;
    /* entries follow */
} __attribute__((packed));

/* MADT entry types */
#define MADT_TYPE_LAPIC         0
#define MADT_TYPE_IOAPIC        1
#define MADT_TYPE_ISO           2
#define MADT_TYPE_NMI_SOURCE    3
#define MADT_TYPE_LAPIC_NMI     4
#define MADT_TYPE_LAPIC_ADDR    5
#define MADT_TYPE_X2APIC        9

/* MADT LAPIC entry */
struct madt_lapic {
    uint8_t type;
    uint8_t length;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed));

/* MADT I/O APIC entry */
struct madt_ioapic {
    uint8_t type;
    uint8_t length;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_address;
    uint32_t global_interrupt_base;
} __attribute__((packed));

/* MADT Interrupt Source Override entry */
struct madt_iso {
    uint8_t type;
    uint8_t length;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t global_interrupt;
    uint16_t flags;
} __attribute__((packed));

/* MADT LAPIC NMI entry */
struct madt_lapic_nmi {
    uint8_t type;
    uint8_t length;
    uint8_t processor_id;
    uint16_t flags;
    uint8_t lint;
} __attribute__((packed));

/* Function prototypes */
void acpi_init(void);
void *acpi_find_table(const char *signature);

#endif /* LETHAL_ACPI_H */