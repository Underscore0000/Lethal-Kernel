/*
 * Lethal Kernel - ACPI MADT (Multiple APIC Description Table) Parser
 *
 * v1.0: Parses MADT to discover APIC IDs and I/O APICs for SMP.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "madt.h"
#include "acpi.h"
#include "../smp/cpu.h"
#include "../core/console.h"
#include "../core/timer.h"
#include "../drivers/serial/serial.h"

static struct madt_info madt_info;
static struct madt *madt_table = NULL;
static int madt_parsed = 0;

static void madt_parse_entries(void)
{
    uint8_t *ptr = (uint8_t *)madt_table + sizeof(struct madt);
    uint8_t *end = (uint8_t *)madt_table + madt_table->length;
    
    madt_info.lapic_count = 0;
    madt_info.ioapic_count = 0;
    madt_info.iso_count = 0;
    madt_info.lapic_nmi_count = 0;
    
    while (ptr < end) {
        uint8_t type = ptr[0];
        uint8_t length = ptr[1];
        
        switch (type) {
            case MADT_TYPE_LAPIC: {
                struct madt_lapic *lapic = (struct madt_lapic *)ptr;
                if (madt_info.lapic_count < 32) {
                    madt_info.lapics[madt_info.lapic_count++] = *lapic;
                    serial_print("[madt] LAPIC: id=");
                    serial_print_dec(lapic->processor_id);
                    serial_print(" apic=");
                    serial_print_dec(lapic->apic_id);
                    serial_print(" flags=0x");
                    serial_print_hex(lapic->flags);
                    serial_print("\n");
                }
                break;
            }
            
            case MADT_TYPE_IOAPIC: {
                struct madt_ioapic *ioapic = (struct madt_ioapic *)ptr;
                if (madt_info.ioapic_count < 8) {
                    madt_info.ioapics[madt_info.ioapic_count++] = *ioapic;
                    serial_print("[madt] IOAPIC: id=");
                    serial_print_dec(ioapic->ioapic_id);
                    serial_print(" addr=0x");
                    serial_print_hex(ioapic->ioapic_address);
                    serial_print(" gsi_base=");
                    serial_print_dec(ioapic->global_interrupt_base);
                    serial_print("\n");
                }
                break;
            }
            
            case MADT_TYPE_ISO: {
                struct madt_iso *iso = (struct madt_iso *)ptr;
                if (madt_info.iso_count < 24) {
                    madt_info.isos[madt_info.iso_count++] = *iso;
                    serial_print("[madt] ISO: bus=");
                    serial_print_dec(iso->bus_source);
                    serial_print(" irq=");
                    serial_print_dec(iso->irq_source);
                    serial_print(" gsi=");
                    serial_print_dec(iso->global_interrupt);
                    serial_print("\n");
                }
                break;
            }
            
            case MADT_TYPE_LAPIC_NMI: {
                struct madt_lapic_nmi *nmi = (struct madt_lapic_nmi *)ptr;
                if (madt_info.lapic_nmi_count < 16) {
                    madt_info.lapic_nmis[madt_info.lapic_nmi_count++] = *nmi;
                }
                break;
            }
            
            case MADT_TYPE_LAPIC_ADDR: {
                /* 64-bit APIC address override */
                madt_info.local_apic_addr = *(uint32_t *)(ptr + 4);
                serial_print("[madt] APIC address override: 0x");
                serial_print_hex(madt_info.local_apic_addr);
                serial_print("\n");
                break;
            }
            
            case MADT_TYPE_X2APIC: {
                /* x2APIC - ignore for now */
                break;
            }
            
            default:
                serial_print("[madt] unknown entry type ");
                serial_print_dec(type);
                serial_print("\n");
                break;
        }
        
        ptr += length;
    }
}

void madt_parse(void)
{
    if (madt_parsed) return;
    
    serial_print("[madt] parsing...\n");
    

    for (size_t i = 0; i < sizeof(madt_info); i++) {
        ((uint8_t *)&madt_info)[i] = 0;
    }
    madt_info.local_apic_addr = 0xFEE00000; 
    
    madt_table = (struct madt *)acpi_find_table("APIC");
    if (!madt_table) {
        serial_print("[madt] WARNING: MADT not found\n");
        madt_parsed = 1;
        return;
    }
    
    madt_info.local_apic_addr = madt_table->local_apic_addr;
    madt_info.flags = madt_table->flags;
    
    serial_print("[madt] found at 0x");
    serial_print_hex((uint64_t)madt_table);
    serial_print(" (addr=0x");
    serial_print_hex(madt_info.local_apic_addr);
    serial_print(")\n");
    
    madt_parse_entries();
    
    madt_parsed = 1;
    serial_print("[madt] parsed: ");
    serial_print_dec(madt_info.lapic_count);
    serial_print(" LAPICs, ");
    serial_print_dec(madt_info.ioapic_count);
    serial_print(" I/O APICs\n");
}

struct madt_info *madt_get_info(void)
{
    if (!madt_parsed) {
        madt_parse();
    }
    return &madt_info;
}

void *madt_find_entry(uint8_t type, int *index)
{
    if (!madt_parsed || !madt_table) return NULL;
    
    int count = 0;
    uint8_t *ptr = (uint8_t *)madt_table + sizeof(struct madt);
    uint8_t *end = (uint8_t *)madt_table + madt_table->length;
    
    while (ptr < end) {
        uint8_t t = ptr[0];
        uint8_t len = ptr[1];
        
        if (t == type) {
            if (index && *index == count) {
                return ptr;
            }
            count++;
        }
        ptr += len;
    }
    
    return NULL;
}