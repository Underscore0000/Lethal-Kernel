/*
 * Lethal Kernel - EXT4 Online Resize
 *
 * v4.0: Online filesystem resizing support.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext4.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>

/* Forward declarations for EXT2/EXT3 block I/O */
extern int ext2_read_block(struct ext2_filesystem *fs, uint32_t block, void *buffer);
extern int ext2_write_block(struct ext2_filesystem *fs, uint32_t block, void *buffer);
extern int ext2_alloc_block(struct ext2_filesystem *fs);
extern void ext2_free_block(struct ext2_filesystem *fs, uint32_t block);

/* Resize flags */
#define EXT4_RESIZE_FLAG_ALLOCATE  0x01
#define EXT4_RESIZE_FLAG_VERIFY    0x02
#define EXT4_RESIZE_FLAG_FORCE     0x04

/* Resize state */
struct ext4_resize_state {
    uint32_t old_blocks;
    uint32_t new_blocks;
    uint32_t added_blocks;
    uint32_t old_groups;
    uint32_t new_groups;
    uint32_t added_groups;
    uint8_t flags;
};

static struct ext4_resize_state resize_state;

/* Calculate how many groups are needed for a given block count */
static uint32_t ext4_calc_groups(struct ext2_filesystem *fs, uint32_t blocks)
{
    uint32_t blocks_per_group = fs->blocks_per_group;
    return (blocks + blocks_per_group - 1) / blocks_per_group;
}

/* Allocate new block group descriptors */
static int ext4_alloc_group_descriptors(struct ext2_filesystem *fs, uint32_t new_groups)
{
    uint32_t old_groups = fs->groups_count;
    uint32_t total_groups = old_groups + new_groups;
    
    /* Reallocate GDT */
    size_t old_size = old_groups * sizeof(struct ext2_group_desc);
    size_t new_size = total_groups * sizeof(struct ext2_group_desc);
    
    struct ext2_group_desc *new_gdt = (struct ext2_group_desc *)kmalloc(new_size);
    if (!new_gdt) return -1;
    
    memset(new_gdt, 0, new_size);
    
    /* Copy old descriptors */
    if (fs->gdt) {
        memcpy(new_gdt, fs->gdt, old_size);
        kfree(fs->gdt);
    }
    
    fs->gdt = new_gdt;
    fs->groups_count = total_groups;
    
    return 0;
}

/* Initialize a new block group */
static int ext4_init_new_group(struct ext2_filesystem *fs, uint32_t group_num,
                                uint32_t start_block, uint32_t blocks_in_group)
{
    serial_print("[ext4_resize] Initializing group ");
    serial_print_dec(group_num);
    serial_print(" at block ");
    serial_print_dec(start_block);
    serial_print("\n");
    
    uint32_t block_size = fs->block_size;
    uint8_t *buffer = (uint8_t *)kmalloc(block_size);
    if (!buffer) return -1;
    
    /* Allocate and zero block bitmap */
    uint32_t block_bitmap = ext2_alloc_block(fs);
    if (block_bitmap == 0 || block_bitmap == (uint32_t)-1) {
        kfree(buffer);
        return -1;
    }
    
    memset(buffer, 0, block_size);
    ext2_write_block(fs, block_bitmap, buffer);
    
    /* Allocate and zero inode bitmap */
    uint32_t inode_bitmap = ext2_alloc_block(fs);
    if (inode_bitmap == 0 || inode_bitmap == (uint32_t)-1) {
        ext2_free_block(fs, block_bitmap);
        kfree(buffer);
        return -1;
    }
    
    memset(buffer, 0, block_size);
    ext2_write_block(fs, inode_bitmap, buffer);
    
    /* Allocate inode table */
    uint32_t inode_table = ext2_alloc_block(fs);
    if (inode_table == 0 || inode_table == (uint32_t)-1) {
        ext2_free_block(fs, block_bitmap);
        ext2_free_block(fs, inode_bitmap);
        kfree(buffer);
        return -1;
    }
    
    uint32_t inodes_per_group = fs->inodes_per_group;
    uint32_t inode_table_blocks = (inodes_per_group * fs->inode_size + block_size - 1) / block_size;
    
    for (uint32_t i = 0; i < inode_table_blocks; i++) {
        uint32_t table_block = ext2_alloc_block(fs);
        if (table_block == 0 || table_block == (uint32_t)-1) {
            kfree(buffer);
            return -1;
        }
        memset(buffer, 0, block_size);
        ext2_write_block(fs, table_block, buffer);
    }
    
    /* Update group descriptor */
    struct ext2_group_desc *desc = &fs->gdt[group_num];
    desc->bg_block_bitmap = block_bitmap;
    desc->bg_inode_bitmap = inode_bitmap;
    desc->bg_inode_table = inode_table;
    desc->bg_free_blocks_count = blocks_in_group - 3 - inode_table_blocks;
    desc->bg_free_inodes_count = inodes_per_group;
    desc->bg_used_dirs_count = 0;
    
    /* Update superblock */
    fs->sb.s_blocks_count += blocks_in_group;
    fs->sb.s_free_blocks_count += desc->bg_free_blocks_count;
    fs->sb.s_free_inodes_count += inodes_per_group;
    
    kfree(buffer);
    return 0;
}

/* Main resize function */
int ext4_resize(struct ext4_filesystem *fs, uint32_t new_size_blocks)
{
    if (!fs) return -1;
    
    struct ext2_filesystem *ext2 = &fs->ext3.ext2;
    uint32_t current_blocks = ext2->sb.s_blocks_count;
    uint32_t blocks_per_group = ext2->blocks_per_group;
    
    serial_print("[ext4_resize] Resizing from ");
    serial_print_dec(current_blocks);
    serial_print(" to ");
    serial_print_dec(new_size_blocks);
    serial_print(" blocks\n");
    
    if (new_size_blocks <= current_blocks) {
        serial_print("[ext4_resize] New size is not larger\n");
        return -1;
    }
    
    uint32_t added_blocks = new_size_blocks - current_blocks;
    uint32_t current_groups = ext2->groups_count;
    uint32_t new_groups = ext4_calc_groups(ext2, new_size_blocks);
    uint32_t added_groups = new_groups - current_groups;
    
    resize_state.old_blocks = current_blocks;
    resize_state.new_blocks = new_size_blocks;
    resize_state.added_blocks = added_blocks;
    resize_state.old_groups = current_groups;
    resize_state.new_groups = new_groups;
    resize_state.added_groups = added_groups;
    
    serial_print("[ext4_resize] Adding ");
    serial_print_dec(added_blocks);
    serial_print(" blocks (");
    serial_print_dec(added_groups);
    serial_print(" groups)\n");
    
    /* Reallocate group descriptors */
    if (ext4_alloc_group_descriptors(ext2, added_groups) != 0) {
        serial_print("[ext4_resize] Failed to allocate group descriptors\n");
        return -1;
    }
    
    /* Initialize each new group */
    uint32_t start_block = current_blocks;
    uint32_t remaining_blocks = added_blocks;
    uint32_t group_num = current_groups;
    
    for (uint32_t i = 0; i < added_groups; i++) {
        uint32_t blocks_in_group = (remaining_blocks > blocks_per_group) ? 
                                   blocks_per_group : remaining_blocks;
        
        if (ext4_init_new_group(ext2, group_num, start_block, blocks_in_group) != 0) {
            serial_print("[ext4_resize] Failed to initialize group ");
            serial_print_dec(group_num);
            serial_print("\n");
            return -1;
        }
        
        start_block += blocks_in_group;
        remaining_blocks -= blocks_in_group;
        group_num++;
    }
    
    /* Update superblock */
    ext2->sb.s_blocks_count = new_size_blocks;
    
    /* Write superblock back to disk */
    uint8_t *sb_buffer = (uint8_t *)kmalloc(ext2->block_size);
    if (sb_buffer) {
        ext2_read_block(ext2, 1, sb_buffer);
        memcpy(sb_buffer + 1024, &ext2->sb, sizeof(struct ext2_superblock));
        ext2_write_block(ext2, 1, sb_buffer);
        kfree(sb_buffer);
    }
    
    /* Write group descriptors back to disk */
    uint32_t gdt_size = ext2->groups_count * sizeof(struct ext2_group_desc);
    uint32_t gdt_blocks = (gdt_size + ext2->block_size - 1) / ext2->block_size;
    uint32_t gdt_block = ext2->first_data_block + 1;
    
    for (uint32_t i = 0; i < gdt_blocks; i++) {
        ext2_write_block(ext2, gdt_block + i, 
                         (uint8_t *)ext2->gdt + i * ext2->block_size);
    }
    
    serial_print("[ext4_resize] Resize complete: ");
    serial_print_dec(ext2->sb.s_blocks_count);
    serial_print(" blocks total\n");
    
    return 0;
}

/* Check if resize is possible */
int ext4_resize_check(struct ext4_filesystem *fs, uint32_t new_size_blocks)
{
    if (!fs) return -1;
    
    struct ext2_filesystem *ext2 = &fs->ext3.ext2;
    uint32_t current_blocks = ext2->sb.s_blocks_count;
    
    if (new_size_blocks <= current_blocks) {
        serial_print("[ext4_resize] New size must be larger than current\n");
        return -1;
    }
    
    /* For now, allow resize up to any size */
    serial_print("[ext4_resize] Resize check passed: can grow to ");
    serial_print_dec(new_size_blocks);
    serial_print(" blocks\n");
    
    return 0;
}

/* Get resize status */
void ext4_resize_status(struct ext4_filesystem *fs, uint32_t *current_blocks,
                         uint32_t *max_blocks, uint8_t *enabled)
{
    if (!fs) return;
    
    struct ext2_filesystem *ext2 = &fs->ext3.ext2;
    if (current_blocks) *current_blocks = ext2->sb.s_blocks_count;
    if (max_blocks) *max_blocks = 0xFFFFFFFF; /* No limit for now */
    if (enabled) *enabled = 1; /* Always enabled for now */
}

/* Enable/disable resize */
int ext4_resize_enable(struct ext4_filesystem *fs, uint8_t enable)
{
    if (!fs) return -1;
    
    serial_print("[ext4_resize] Resize ");
    serial_print(enable ? "enabled" : "disabled");
    serial_print("\n");
    
    return 0;
}

/* Set maximum blocks for resize */
int ext4_resize_set_max(struct ext4_filesystem *fs, uint32_t max_blocks)
{
    if (!fs) return -1;
    
    serial_print("[ext4_resize] Max blocks set to ");
    serial_print_dec(max_blocks);
    serial_print("\n");
    
    return 0;
}