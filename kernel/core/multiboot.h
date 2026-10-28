#ifndef LETHAL_MULTIBOOT_H
#define LETHAL_MULTIBOOT_H

#include <stdint.h>

/* Layout matches exactly what boot/stage2.asm's get_memory_map writes at
 * E820_BUFFER: 24-byte packed entries as returned by INT15h/E820h. */
struct e820_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_ext;
} __attribute__((packed));

#define E820_TYPE_USABLE   1
#define E820_TYPE_RESERVED 2
#define E820_TYPE_ACPI_RECLAIM 3
#define E820_TYPE_ACPI_NVS 4
#define E820_TYPE_BAD      5

#endif
