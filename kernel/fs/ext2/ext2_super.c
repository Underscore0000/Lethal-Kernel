/*
 * Lethal Kernel - EXT2 Superblock Operations
 *
 * v2.0: Superblock reading, verification, and validation.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext2.h"
#include "../../drivers/storage/ata.h"
#include "../../core/console.h"
#include "../../core/panic.h"
#include "../../memory/heap.h"
#include <string.h>

#define EXT2_SECTOR_SIZE 512

static struct ext2_filesystem *ext2_fs = NULL;

/* Block I/O functions - used by other EXT2 modules */
int ext2_read_block(struct ext2_filesystem *fs, uint32_t block, void *buffer)
{
    uint32_t sector = block * (fs->block_size / EXT2_SECTOR_SIZE);
    for (uint32_t i = 0; i < fs->block_size / EXT2_SECTOR_SIZE; i++) {
        if (ata_read28(fs->dev_id, sector + i,
                       (uint8_t *)buffer + i * EXT2_SECTOR_SIZE) != 0) {
            return -1;
        }
    }
    return 0;
}

int ext2_write_block(struct ext2_filesystem *fs, uint32_t block, void *buffer)
{
    uint32_t sector = block * (fs->block_size / EXT2_SECTOR_SIZE);
    for (uint32_t i = 0; i < fs->block_size / EXT2_SECTOR_SIZE; i++) {
        if (ata_write28(fs->dev_id, sector + i,
                        (uint8_t *)buffer + i * EXT2_SECTOR_SIZE) != 0) {
            return -1;
        }
    }
    return 0;
}

int ext2_read_superblock(struct ext2_filesystem *fs)
{
    uint8_t *buffer = (uint8_t *)kmalloc(fs->block_size);
    if (!buffer) return -1;

    /* Superblock is at block 0 (offset 1024) for block sizes > 1024 */
    /* Or at block 1 for 1024 byte blocks */
    uint32_t sb_block = (fs->block_size == 1024) ? 1 : 0;
    
    if (ext2_read_block(fs, sb_block, buffer) != 0) {
        kfree(buffer);
        return -1;
    }

    /* Copy superblock (at offset 1024) */
    memcpy(&fs->sb, buffer + 1024, sizeof(struct ext2_superblock));
    kfree(buffer);

    return 0;
}

int ext2_verify_superblock(struct ext2_superblock *sb)
{
    if (sb->s_magic != EXT2_SUPER_MAGIC) {
        serial_print("[ext2] Invalid superblock magic: 0x");
        serial_print_hex(sb->s_magic);
        serial_print(" (expected 0xEF53)\n");
        return -1;
    }

    if (sb->s_rev_level > 1) {
        serial_print("[ext2] Unsupported revision level: ");
        serial_print_dec(sb->s_rev_level);
        serial_print("\n");
        return -1;
    }

    if (sb->s_log_block_size > 2) {
        serial_print("[ext2] Unsupported block size: 1024 << ");
        serial_print_dec(sb->s_log_block_size);
        serial_print("\n");
        return -1;
    }

    return 0;
}

int ext2_read_group_desc(struct ext2_filesystem *fs)
{
    uint32_t gdt_size = fs->groups_count * sizeof(struct ext2_group_desc);
    uint32_t gdt_blocks = (gdt_size + fs->block_size - 1) / fs->block_size;

    fs->gdt = (struct ext2_group_desc *)kmalloc(gdt_size);
    if (!fs->gdt) return -1;

    uint32_t gdt_block = fs->first_data_block + 1;
    uint8_t *buffer = (uint8_t *)fs->gdt;

    for (uint32_t i = 0; i < gdt_blocks; i++) {
        if (ext2_read_block(fs, gdt_block + i, buffer + i * fs->block_size) != 0) {
            kfree(fs->gdt);
            fs->gdt = NULL;
            return -1;
        }
    }

    return 0;
}

struct ext2_filesystem *ext2_get_fs(void)
{
    return ext2_fs;
}

void ext2_set_fs(struct ext2_filesystem *fs)
{
    ext2_fs = fs;
}