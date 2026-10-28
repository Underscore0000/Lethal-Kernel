/*
 * Lethal Kernel - I/O APIC Implementation
 *
 * v1.0: I/O APIC driver for SMP interrupt routing.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ioapic.h"
#include "apic.h"
#include "../acpi/acpi.h"
#include "../acpi/madt.h"
#include "../core/console.h"
#include "../core/panic.h"
#include "../../arch/x86_64/io.h"

#define IOAPIC_MAX 4

static struct ioapic_info ioapics[IOAPIC_MAX];
static uint32_t ioapic_count = 0;
static uint8_t pic_disabled = 0;

static uint32_t ioapic_read_reg(volatile uint32_t *base, uint32_t reg)
{
    base[IOAPIC_IOREGSEL / 4] = reg;
    return base[IOAPIC_IOWIN / 4];
}

static void ioapic_write_reg(volatile uint32_t *base, uint32_t reg, uint32_t value)
{
    base[IOAPIC_IOREGSEL / 4] = reg;
    base[IOAPIC_IOWIN / 4] = value;
}

static void ioapic_read_entry(volatile uint32_t *base, uint32_t entry, uint32_t *low, uint32_t *high)
{
    *low = ioapic_read_reg(base, IOAPIC_REDTBL_BASE + entry * 2);
    *high = ioapic_read_reg(base, IOAPIC_REDTBL_BASE + entry * 2 + 1);
}

static void ioapic_write_entry(volatile uint32_t *base, uint32_t entry, uint32_t low, uint32_t high)
{
    ioapic_write_reg(base, IOAPIC_REDTBL_BASE + entry * 2, low);
    ioapic_write_reg(base, IOAPIC_REDTBL_BASE + entry * 2 + 1, high);
}

static void ioapic_add(uint32_t ioapic_id, uint64_t base_addr, uint32_t gsi_base)
{
    if (ioapic_count >= IOAPIC_MAX) {
        serial_print("[ioapic] WARNING: too many I/O APICs\n");
        return;
    }
    
    struct ioapic_info *io = &ioapics[ioapic_count];
    io->ioapic_id = ioapic_id;
    io->base_address = base_addr;
    io->global_interrupt_base = gsi_base;
    io->base = (volatile uint32_t *)(uintptr_t)base_addr;
    io->present = 1;
    
    /* Read version to determine max entries */
    io->version = ioapic_read_reg(io->base, IOAPIC_VERSION);
    io->max_redirection = (io->version >> 16) & 0xFF;
    io->redtbl_entries = io->max_redirection + 1;
    
    serial_print("[ioapic] found I/O APIC id=");
    serial_print_dec(ioapic_id);
    serial_print(" version=0x");
    serial_print_hex(io->version);
    serial_print(" gsi_base=");
    serial_print_dec(gsi_base);
    serial_print(" entries=");
    serial_print_dec(io->redtbl_entries);
    serial_print(" addr=0x");
    serial_print_hex(base_addr);
    serial_print("\n");
    
    /* Mask all entries initially */
    for (uint32_t i = 0; i < io->redtbl_entries; i++) {
        ioapic_write_entry(io->base, i, IOAPIC_REDTBL_MASKED, 0);
    }
    
    ioapic_count++;
}

static void ioapic_parse_madt(void)
{
    struct madt_info *madt = madt_get_info();
    if (!madt) {
        serial_print("[ioapic] WARNING: MADT not available\n");
        return;
    }
    
    for (uint32_t i = 0; i < madt->ioapic_count; i++) {
        struct madt_ioapic *io = &madt->ioapics[i];
        ioapic_add(io->ioapic_id, io->ioapic_address, io->global_interrupt_base);
    }
}

void ioapic_init(void)
{
    serial_print("[ioapic] initializing...\n");
    
    /* Parse MADT to find I/O APICs */
    ioapic_parse_madt();
    
    if (ioapic_count == 0) {
        serial_print("[ioapic] WARNING: no I/O APIC found, legacy PIC will be used\n");
        return;
    }
    
    serial_print("[ioapic] initialized with ");
    serial_print_dec(ioapic_count);
    serial_print(" I/O APICs\n");
}

struct ioapic_info *ioapic_find(uint32_t ioapic_id)
{
    for (uint32_t i = 0; i < ioapic_count; i++) {
        if (ioapics[i].ioapic_id == ioapic_id) {
            return &ioapics[i];
        }
    }
    return NULL;
}

void ioapic_redirect_irq(uint32_t irq, uint32_t vector, uint32_t apic_id, uint32_t flags)
{
    /* Find which I/O APIC handles this GSI */
    struct ioapic_info *io = NULL;
    uint32_t entry = 0;
    
    for (uint32_t i = 0; i < ioapic_count; i++) {
        if (irq >= ioapics[i].global_interrupt_base &&
            irq < ioapics[i].global_interrupt_base + ioapics[i].redtbl_entries) {
            io = &ioapics[i];
            entry = irq - io->global_interrupt_base;
            break;
        }
    }
    
    if (!io) {
        serial_print("[ioapic] WARNING: no I/O APIC for IRQ ");
        serial_print_dec(irq);
        serial_print("\n");
        return;
    }
    
    /* Build redirection entry */
    uint32_t low = vector;
    uint32_t high = apic_id << 24;
    
    /* Set delivery mode */
    if (flags & IOAPIC_REDTBL_NMI) {
        low |= IOAPIC_REDTBL_NMI;
    } else {
        low |= IOAPIC_REDTBL_FIXED;
    }
    
    /* Set trigger mode */
    if (flags & IOAPIC_REDTBL_LEVEL) {
        low |= IOAPIC_REDTBL_LEVEL;
    }
    
    /* Set polarity */
    if (flags & IOAPIC_REDTBL_HIGH) {
        low |= IOAPIC_REDTBL_HIGH;
    }
    
    /* Destination mode - physical by default */
    low |= IOAPIC_REDTBL_PHYSICAL;
    
    /* Write the entry */
    ioapic_write_entry(io->base, entry, low, high);
    
    serial_print("[ioapic] IRQ ");
    serial_print_dec(irq);
    serial_print(" -> vector 0x");
    serial_print_hex(vector);
    serial_print(" on CPU ");
    serial_print_dec(apic_id);
    serial_print("\n");
}

void ioapic_mask_irq(uint32_t irq)
{
    /* Find the entry and mask it */
    for (uint32_t i = 0; i < ioapic_count; i++) {
        if (irq >= ioapics[i].global_interrupt_base &&
            irq < ioapics[i].global_interrupt_base + ioapics[i].redtbl_entries) {
            uint32_t entry = irq - ioapics[i].global_interrupt_base;
            uint32_t low, high;
            ioapic_read_entry(ioapics[i].base, entry, &low, &high);
            low |= IOAPIC_REDTBL_MASKED;
            ioapic_write_entry(ioapics[i].base, entry, low, high);
            return;
        }
    }
}

void ioapic_unmask_irq(uint32_t irq)
{
    for (uint32_t i = 0; i < ioapic_count; i++) {
        if (irq >= ioapics[i].global_interrupt_base &&
            irq < ioapics[i].global_interrupt_base + ioapics[i].redtbl_entries) {
            uint32_t entry = irq - ioapics[i].global_interrupt_base;
            uint32_t low, high;
            ioapic_read_entry(ioapics[i].base, entry, &low, &high);
            low &= ~IOAPIC_REDTBL_MASKED;
            ioapic_write_entry(ioapics[i].base, entry, low, high);
            return;
        }
    }
}

uint32_t ioapic_get_count(void)
{
    return ioapic_count;
}

struct ioapic_info *ioapic_get(uint32_t index)
{
    if (index >= ioapic_count) return NULL;
    return &ioapics[index];
}

void ioapic_disable_pic(void)
{
    if (pic_disabled) return;
    
    /* Disable legacy PIC */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
    pic_disabled = 1;
    serial_print("[ioapic] legacy PIC disabled\n");
}

void ioapic_enable_pic(void)
{
    if (!pic_disabled) return;
    
    /* Enable legacy PIC */
    outb(0x21, 0x00);
    outb(0xA1, 0x00);
    pic_disabled = 0;
    serial_print("[ioapic] legacy PIC enabled\n");
}