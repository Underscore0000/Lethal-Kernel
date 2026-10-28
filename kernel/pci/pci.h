/*
 * Lethal Kernel - PCI
 *
 * v5.0: PCI bus enumeration and configuration.
 */

#ifndef LETHAL_KERNEL_PCI_PCI_H
#define LETHAL_KERNEL_PCI_PCI_H

#include <stdint.h>
#include <stddef.h>

#define PCI_VENDOR_ID          0x00
#define PCI_DEVICE_ID          0x02
#define PCI_COMMAND            0x04
#define PCI_STATUS             0x06
#define PCI_REVISION_ID        0x08
#define PCI_PROG_IF            0x09
#define PCI_SUBCLASS           0x0A
#define PCI_CLASS              0x0B
#define PCI_CACHE_LINE_SIZE    0x0C
#define PCI_LATENCY_TIMER      0x0D
#define PCI_HEADER_TYPE        0x0E
#define PCI_BIST               0x0F

#define PCI_BAR0               0x10
#define PCI_BAR1               0x14
#define PCI_BAR2               0x18
#define PCI_BAR3               0x1C
#define PCI_BAR4               0x20
#define PCI_BAR5               0x24

#define PCI_CARDBUS_CIS        0x28
#define PCI_SUBSYSTEM_VENDOR   0x2C
#define PCI_SUBSYSTEM_ID       0x2E
#define PCI_ROM_ADDRESS        0x30
#define PCI_CAPABILITIES       0x34

#define PCI_INTERRUPT_LINE     0x3C
#define PCI_INTERRUPT_PIN      0x3D
#define PCI_MIN_GNT            0x3E
#define PCI_MAX_LAT            0x3F

#define PCI_HEADER_TYPE_DEVICE     0x00
#define PCI_HEADER_TYPE_BRIDGE     0x01
#define PCI_HEADER_TYPE_CARDBUS    0x02
#define PCI_HEADER_TYPE_MULTIFUNC  0x80

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;

    uint16_t command;
    uint16_t status;

    uint8_t revision_id;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t class;

    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;

    uint32_t bar0;
    uint32_t bar1;
    uint32_t bar2;
    uint32_t bar3;
    uint32_t bar4;
    uint32_t bar5;

    uint32_t cardbus_cis;

    uint16_t subsystem_vendor;
    uint16_t subsystem_id;

    uint32_t rom_address;

    uint8_t irq_line;
    uint8_t irq_pin;

    uint8_t min_gnt;
    uint8_t max_lat;
} pci_device_t;

int pci_init(void);

int pci_read_device(
    int bus,
    int device,
    int function,
    pci_device_t *pci_dev
);

uint32_t pci_read_config_dword(
    int bus,
    int device,
    int function,
    uint8_t offset
);

uint16_t pci_read_config_word(
    int bus,
    int device,
    int function,
    uint8_t offset
);

uint8_t pci_read_config_byte(
    int bus,
    int device,
    int function,
    uint8_t offset
);

void pci_write_config_dword(
    int bus,
    int device,
    int function,
    uint8_t offset,
    uint32_t value
);

void pci_write_config_byte(
    int bus,
    int device,
    int function,
    uint8_t offset,
    uint8_t value
);

void pci_enable_bus_master(
    int bus,
    int device,
    int function
);

#endif
