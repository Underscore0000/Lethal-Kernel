/*
 * Lethal Kernel - EXT2 Bitmap Operations
 *
 * v2.0: Block and inode bitmap management.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext2.h"
#include "../../memory/heap.h"
#include <string.h>

/* Forward declarations of block I/O functions from ext2_super.c */
extern int ext2_read_block(struct ext2_filesystem *fs, uint32_t block, void *buffer);
extern int ext2_write_block(struct ext2_filesystem *fs, uint32_t block, void *buffer);

static int ext2_read_bitmap(struct ext2_filesystem *fs, uint32_t bitmap_block,
                             uint8_t *bitmap)
{
    return ext2_read_block(fs, bitmap_block, bitmap);
}

static int ext2_write_bitmap(struct ext2_filesystem *fs, uint32_t bitmap_block,
                              uint8_t *bitmap)
{
    return ext2_write_block(fs, bitmap_block, bitmap);
}

static int ext2_find_free_bit(uint8_t *bitmap, uint32_t size, uint32_t *index)
{
    for (uint32_t i = 0; i < size; i++) {
        for (uint32_t bit = 0; bit < 8; bit++) {
            if (!(bitmap[i] & (1 << bit))) {
                *index = i * 8 + bit;
                return 1;
            }
        }
    }
    return 0;
}

static void ext2_set_bit(uint8_t *bitmap, uint32_t index, int value)
{
    uint32_t byte = index / 8;
    uint32_t bit = index % 8;
    if (value) {
        bitmap[byte] |= (1 << bit);
    } else {
        bitmap[byte] &= ~(1 << bit);
    }
}

int ext2_alloc_block(struct ext2_filesystem *fs)
{
    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size);
    if (!bitmap) return -1;

    for (uint32_t i = 0; i < fs->groups_count; i++) {
        uint32_t bitmap_block = fs->gdt[i].bg_block_bitmap;
        
        if (ext2_read_bitmap(fs, bitmap_block, bitmap) != 0) {
            kfree(bitmap);
            return -1;
        }

        uint32_t blocks_in_group = fs->blocks_per_group;
        if (i == fs->groups_count - 1) {
            blocks_in_group = fs->sb.s_blocks_count - i * fs->blocks_per_group;
        }

        uint32_t free_block;
        if (ext2_find_free_bit(bitmap, fs->block_size * 8, &free_block)) {
            if (free_block < blocks_in_group) {
                uint32_t block = i * fs->blocks_per_group + free_block + fs->first_data_block;
                ext2_set_bit(bitmap, free_block, 1);
                ext2_write_bitmap(fs, bitmap_block, bitmap);
                fs->sb.s_free_blocks_count--;
                kfree(bitmap);
                return block;
            }
        }
    }

    kfree(bitmap);
    return -1;
}

void ext2_free_block(struct ext2_filesystem *fs, uint32_t block)
{
    uint32_t block_index = block - fs->first_data_block;
    uint32_t group = block_index / fs->blocks_per_group;
    uint32_t offset = block_index % fs->blocks_per_group;

    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size);
    if (!bitmap) return;

    uint32_t bitmap_block = fs->gdt[group].bg_block_bitmap;
    if (ext2_read_bitmap(fs, bitmap_block, bitmap) == 0) {
        ext2_set_bit(bitmap, offset, 0);
        ext2_write_bitmap(fs, bitmap_block, bitmap);
        fs->sb.s_free_blocks_count++;
    }

    kfree(bitmap);
}

int ext2_alloc_inode(struct ext2_filesystem *fs, uint32_t *inode_num)
{
    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size);
    if (!bitmap) return -1;

    for (uint32_t i = 0; i < fs->groups_count; i++) {
        uint32_t bitmap_block = fs->gdt[i].bg_inode_bitmap;
        
        if (ext2_read_bitmap(fs, bitmap_block, bitmap) != 0) {
            kfree(bitmap);
            return -1;
        }

        uint32_t inodes_in_group = fs->inodes_per_group;
        if (i == fs->groups_count - 1) {
            inodes_in_group = fs->sb.s_inodes_count - i * fs->inodes_per_group;
        }

        uint32_t free_inode;
        if (ext2_find_free_bit(bitmap, fs->block_size * 8, &free_inode)) {
            if (free_inode < inodes_in_group) {
                uint32_t inode = i * fs->inodes_per_group + free_inode + 1;
                ext2_set_bit(bitmap, free_inode, 1);
                ext2_write_bitmap(fs, bitmap_block, bitmap);
                fs->sb.s_free_inodes_count--;
                *inode_num = inode;
                kfree(bitmap);
                return 0;
            }
        }
    }

    kfree(bitmap);
    return -1;
}

void ext2_free_inode(struct ext2_filesystem *fs, uint32_t inode_num)
{
    uint32_t inode_index = inode_num - 1;
    uint32_t group = inode_index / fs->inodes_per_group;
    uint32_t offset = inode_index % fs->inodes_per_group;

    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size);
    if (!bitmap) return;

    uint32_t bitmap_block = fs->gdt[group].bg_inode_bitmap;
    if (ext2_read_bitmap(fs, bitmap_block, bitmap) == 0) {
        ext2_set_bit(bitmap, offset, 0);
        ext2_write_bitmap(fs, bitmap_block, bitmap);
        fs->sb.s_free_inodes_count++;
    }

    kfree(bitmap);
}