/*
 * Lethal Kernel - ACPI Implementation (Simplified)
 *
 * v1.0: Finds RSDP by searching BIOS memory range only.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "acpi.h"
#include "madt.h"
#include "../core/console.h"
#include "../core/panic.h"
#include "../drivers/serial/serial.h"

static struct rsdp *rsdp = NULL;
static struct xsdt *xsdt = NULL;
static struct rsdt *rsdt = NULL;
static int use_xsdt = 0;

static uint8_t checksum(const uint8_t *data, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

static struct rsdp *find_rsdp(void)
{
    /* Search BIOS range (0xE0000 - 0xFFFFF) */
    for (uint32_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        struct rsdp *r = (struct rsdp *)(uintptr_t)addr;
        /* Check signature */
        if (r->signature[0] == 'R' && r->signature[1] == 'S' &&
            r->signature[2] == 'D' && r->signature[3] == 'P' &&
            r->signature[4] == 'T' && r->signature[5] == 'R') {
            if (checksum((uint8_t *)r, 20) == 0) {
                return r;
            }
        }
    }
    
    return NULL;
}

void acpi_init(void)
{
    serial_print("[acpi] initializing...\n");
    
    rsdp = find_rsdp();
    if (!rsdp) {
        serial_print("[acpi] WARNING: RSDP not found\n");
        return;
    }
    
    serial_print("[acpi] RSDP found at 0x");
    serial_print_hex((uint64_t)(uintptr_t)rsdp);
    serial_print("\n");
    
    if (rsdp->revision >= 2 && rsdp->xsdt_address) {
        xsdt = (struct xsdt *)(uintptr_t)rsdp->xsdt_address;
        use_xsdt = 1;
        serial_print("[acpi] XSDT found at 0x");
        serial_print_hex((uint64_t)(uintptr_t)xsdt);
        serial_print("\n");
    } else {
        rsdt = (struct rsdt *)(uintptr_t)rsdp->rsdt_address;
        use_xsdt = 0;
        serial_print("[acpi] RSDT found at 0x");
        serial_print_hex((uint64_t)(uintptr_t)rsdt);
        serial_print("\n");
    }
    
    /* Parse MADT for SMP */
    madt_parse();
}

void *acpi_find_table(const char *signature)
{
    if (use_xsdt && xsdt) {
        uint64_t *entries = (uint64_t *)((uint8_t *)xsdt + sizeof(struct xsdt));
        int count = (xsdt->length - sizeof(struct xsdt)) / 8;
        for (int i = 0; i < count; i++) {
            struct rsdt *table = (struct rsdt *)(uintptr_t)entries[i];
            if (table->signature[0] == signature[0] &&
                table->signature[1] == signature[1] &&
                table->signature[2] == signature[2] &&
                table->signature[3] == signature[3]) {
                return table;
            }
        }
    } else if (rsdt) {
        uint32_t *entries = (uint32_t *)((uint8_t *)rsdt + sizeof(struct rsdt));
        int count = (rsdt->length - sizeof(struct rsdt)) / 4;
        for (int i = 0; i < count; i++) {
            struct rsdt *table = (struct rsdt *)(uintptr_t)entries[i];
            if (table->signature[0] == signature[0] &&
                table->signature[1] == signature[1] &&
                table->signature[2] == signature[2] &&
                table->signature[3] == signature[3]) {
                return table;
            }
        }
    }
    
    return NULL;
}