/*
 * Lethal Kernel - Multiboot2 information structures
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_MULTIBOOT2_H
#define LETHAL_MULTIBOOT2_H

#include <stdint.h>

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289

/* Tag types we care about (see Multiboot2 spec, section 3.6). */
#define MB2_TAG_END          0
#define MB2_TAG_CMDLINE      1
#define MB2_TAG_BOOT_LOADER  2
#define MB2_TAG_MODULE       3
#define MB2_TAG_BASIC_MEM    4
#define MB2_TAG_BOOTDEV      5
#define MB2_TAG_MMAP         6
#define MB2_TAG_VBE          7
#define MB2_TAG_FRAMEBUFFER  8
#define MB2_TAG_ELF_SYMS     9
#define MB2_TAG_APM          10
#define MB2_TAG_EFI32        11
#define MB2_TAG_EFI64        12
#define MB2_TAG_SMBIOS       13
#define MB2_TAG_ACPI_OLD     14
#define MB2_TAG_ACPI_NEW     15
#define MB2_TAG_NETWORK      16
#define MB2_TAG_EFI_MMAP     17
#define MB2_TAG_EFI_BS       18
#define MB2_TAG_EFI32_IH     19
#define MB2_TAG_EFI64_IH     20
#define MB2_TAG_LOAD_ADDR    21

/* Memory map entry types (see MB2_TAG_MMAP). */
#define MB2_MMAP_AVAILABLE   1
#define MB2_MMAP_RESERVED    2
#define MB2_MMAP_ACPI        3
#define MB2_MMAP_NVS         4
#define MB2_MMAP_BAD         5

struct mb2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct mb2_info {
    uint32_t total_size;
    uint32_t reserved;
    /* tags follow, each aligned to 8 bytes */
} __attribute__((packed));

struct mb2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    /* entries follow (entry_size bytes each) */
} __attribute__((packed));

/*
 * Multiboot2 memory map entry. Layout is byte-for-byte compatible with
 * Lethal Kernel's struct e820_entry (base:8, length:8, type:4, acpi_ext:4),
 * which is what pmm_init() consumes, so the array can be handed over as-is.
 */
struct mb2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} __attribute__((packed));

struct mb2_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;   /* 1 = RGB, 2 = EGA text */
    uint16_t reserved;
    /* For type 1 (RGB): color info follows. */
    uint8_t  red_field_position;
    uint8_t  red_mask_size;
    uint8_t  green_field_position;
    uint8_t  green_mask_size;
    uint8_t  blue_field_position;
    uint8_t  blue_mask_size;
} __attribute__((packed));

#endif /* LETHAL_MULTIBOOT2_H */