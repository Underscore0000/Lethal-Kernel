/*
 * Lethal Kernel - PCI Implementation
 *
 * v5.0: PCI bus enumeration and configuration.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */

#include "pci.h"
#include "../core/console.h"
#include "../arch/x86_64/io.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_COMMAND_IO_SPACE       (1U << 0)
#define PCI_COMMAND_MEMORY_SPACE   (1U << 1)
#define PCI_COMMAND_BUS_MASTER     (1U << 2)

/* ============================================================================
 * PCI Configuration Space Access
 * ============================================================================ */

static uint32_t pci_make_addr(
    int bus,
    int device,
    int function,
    uint8_t offset)
{
    return 0x80000000U |
           ((uint32_t)bus << 16) |
           ((uint32_t)device << 11) |
           ((uint32_t)function << 8) |
           ((uint32_t)offset & 0xFCU);
}

uint32_t pci_read_config_dword(
    int bus,
    int device,
    int function,
    uint8_t offset)
{
    outl(
        PCI_CONFIG_ADDRESS,
        pci_make_addr(bus, device, function, offset)
    );

    return inl(PCI_CONFIG_DATA);
}

void pci_write_config_dword(
    int bus,
    int device,
    int function,
    uint8_t offset,
    uint32_t value)
{
    outl(
        PCI_CONFIG_ADDRESS,
        pci_make_addr(bus, device, function, offset)
    );

    outl(PCI_CONFIG_DATA, value);
}

uint16_t pci_read_config_word(
    int bus,
    int device,
    int function,
    uint8_t offset)
{
    uint32_t value;

    value = pci_read_config_dword(
        bus,
        device,
        function,
        offset
    );

    return (uint16_t)(
        value >> ((offset & 2U) * 8U)
    );
}

uint8_t pci_read_config_byte(
    int bus,
    int device,
    int function,
    uint8_t offset)
{
    uint32_t value;

    value = pci_read_config_dword(
        bus,
        device,
        function,
        offset
    );

    return (uint8_t)(
        value >> ((offset & 3U) * 8U)
    );
}

void pci_write_config_byte(
    int bus,
    int device,
    int function,
    uint8_t offset,
    uint8_t value)
{
    uint32_t dword;
    uint32_t shift;

    dword = pci_read_config_dword(
        bus,
        device,
        function,
        offset & 0xFCU
    );

    shift = (uint32_t)(offset & 3U) * 8U;

    dword &= ~(0xFFU << shift);
    dword |= (uint32_t)value << shift;

    pci_write_config_dword(
        bus,
        device,
        function,
        offset & 0xFCU,
        dword
    );
}

/* ============================================================================
 * PCI Device Read
 * ============================================================================ */

int pci_read_device(
    int bus,
    int device,
    int function,
    pci_device_t *pci_dev)
{
    uint32_t vendor_device;
    uint16_t vendor_id;
    uint16_t device_id;

    if (!pci_dev)
        return -1;

    vendor_device = pci_read_config_dword(
        bus,
        device,
        function,
        PCI_VENDOR_ID
    );

    vendor_id = (uint16_t)(vendor_device & 0xFFFFU);
    device_id = (uint16_t)((vendor_device >> 16) & 0xFFFFU);

    if (vendor_id == 0xFFFFU || vendor_id == 0x0000U)
        return -1;

    pci_dev->vendor_id = vendor_id;
    pci_dev->device_id = device_id;

    pci_dev->command =
        pci_read_config_word(
            bus,
            device,
            function,
            PCI_COMMAND
        );

    pci_dev->status =
        pci_read_config_word(
            bus,
            device,
            function,
            PCI_STATUS
        );

    pci_dev->revision_id =
        pci_read_config_byte(
            bus,
            device,
            function,
            PCI_REVISION_ID
        );

    pci_dev->prog_if =
        pci_read_config_byte(
            bus,
            device,
            function,
            PCI_PROG_IF
        );

    pci_dev->subclass =
        pci_read_config_byte(
            bus,
            device,
            function,
            PCI_SUBCLASS
        );

    pci_dev->class =
        pci_read_config_byte(
            bus,
            device,
            function,
            PCI_CLASS
        );

    pci_dev->header_type =
        pci_read_config_byte(
            bus,
            device,
            function,
            PCI_HEADER_TYPE
        );

    pci_dev->bar0 =
        pci_read_config_dword(
            bus,
            device,
            function,
            PCI_BAR0
        );

    pci_dev->bar1 =
        pci_read_config_dword(
            bus,
            device,
            function,
            PCI_BAR1
        );

    pci_dev->bar2 =
        pci_read_config_dword(
            bus,
            device,
            function,
            PCI_BAR2
        );

    pci_dev->bar3 =
        pci_read_config_dword(
            bus,
            device,
            function,
            PCI_BAR3
        );

    pci_dev->bar4 =
        pci_read_config_dword(
            bus,
            device,
            function,
            PCI_BAR4
        );

    pci_dev->bar5 =
        pci_read_config_dword(
            bus,
            device,
            function,
            PCI_BAR5
        );

    pci_dev->irq_line =
        pci_read_config_byte(
            bus,
            device,
            function,
            PCI_INTERRUPT_LINE
        );

    pci_dev->irq_pin =
        pci_read_config_byte(
            bus,
            device,
            function,
            PCI_INTERRUPT_PIN
        );

    return 0;
}

/* ============================================================================
 * PCI Command
 * ============================================================================ */

void pci_enable_bus_master(
    int bus,
    int device,
    int function)
{
    uint16_t command;

    command = pci_read_config_word(
        bus,
        device,
        function,
        PCI_COMMAND
    );

    command |=
        PCI_COMMAND_MEMORY_SPACE |
        PCI_COMMAND_BUS_MASTER;

    pci_write_config_dword(
        bus,
        device,
        function,
        PCI_COMMAND,
        command
    );
}

/* ============================================================================
 * PCI Initialization
 * ============================================================================ */

int pci_init(void)
{
    int devices_found = 0;

    serial_print("[pci] Initializing...\n");

    for (int bus = 0; bus < 256; bus++) {
        for (int device = 0; device < 32; device++) {
            for (int function = 0; function < 8; function++) {
                pci_device_t dev;

                if (pci_read_device(
                        bus,
                        device,
                        function,
                        &dev) != 0) {
                    continue;
                }

                devices_found++;

                serial_print("[pci] Found device vendor=0x");
                serial_print_hex(dev.vendor_id);

                serial_print(" device=0x");
                serial_print_hex(dev.device_id);

                serial_print(" class=0x");
                serial_print_hex(dev.class);

                serial_print(" subclass=0x");
                serial_print_hex(dev.subclass);

                serial_print("\n");

                /*
                 * Enable MMIO and DMA.
                 */
                pci_enable_bus_master(
                    bus,
                    device,
                    function
                );
            }
        }
    }

    serial_print("[pci] Found ");
    serial_print_dec((uint64_t)devices_found);
    serial_print(" devices\n");

    serial_print("[pci] Initialized\n");

    return 0;
}
