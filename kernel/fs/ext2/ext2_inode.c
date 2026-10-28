/*
 * Lethal Kernel - EXT2 Inode Operations
 *
 * v2.0: Reading, writing, and managing inodes.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext2.h"
#include "../../memory/heap.h"
#include <string.h>

static uint32_t ext2_inode_to_block_group(struct ext2_filesystem *fs, uint32_t inode_num)
{
    return (inode_num - 1) / fs->inodes_per_group;
}

static uint32_t ext2_inode_to_offset(struct ext2_filesystem *fs, uint32_t inode_num)
{
    return (inode_num - 1) % fs->inodes_per_group;
}

int ext2_read_inode(struct ext2_filesystem *fs, uint32_t inode_num,
                     struct ext2_inode *inode)
{
    if (inode_num == 0 || inode_num > fs->sb.s_inodes_count) {
        return -1;
    }

    uint32_t group = ext2_inode_to_block_group(fs, inode_num);
    uint32_t offset = ext2_inode_to_offset(fs, inode_num);

    uint32_t table_block = fs->gdt[group].bg_inode_table;
    uint32_t inodes_per_block = fs->block_size / fs->inode_size;
    uint32_t block_offset = offset / inodes_per_block;
    uint32_t inode_offset = (offset % inodes_per_block) * fs->inode_size;

    uint8_t *buffer = (uint8_t *)kmalloc(fs->block_size);
    if (!buffer) return -1;

    if (ext2_read_block(fs, table_block + block_offset, buffer) != 0) {
        kfree(buffer);
        return -1;
    }

    memcpy(inode, buffer + inode_offset, sizeof(struct ext2_inode));
    kfree(buffer);
    return 0;
}

int ext2_write_inode(struct ext2_filesystem *fs, uint32_t inode_num,
                      struct ext2_inode *inode)
{
    if (inode_num == 0 || inode_num > fs->sb.s_inodes_count) {
        return -1;
    }

    uint32_t group = ext2_inode_to_block_group(fs, inode_num);
    uint32_t offset = ext2_inode_to_offset(fs, inode_num);

    uint32_t table_block = fs->gdt[group].bg_inode_table;
    uint32_t inodes_per_block = fs->block_size / fs->inode_size;
    uint32_t block_offset = offset / inodes_per_block;
    uint32_t inode_offset = (offset % inodes_per_block) * fs->inode_size;

    uint8_t *buffer = (uint8_t *)kmalloc(fs->block_size);
    if (!buffer) return -1;

    if (ext2_read_block(fs, table_block + block_offset, buffer) != 0) {
        kfree(buffer);
        return -1;
    }

    memcpy(buffer + inode_offset, inode, sizeof(struct ext2_inode));
    ext2_write_block(fs, table_block + block_offset, buffer);
    kfree(buffer);
    return 0;
}

int ext2_alloc_inode_core(struct ext2_filesystem *fs, uint32_t *inode_num)
{
    if (ext2_alloc_inode(fs, inode_num) != 0) {
        return -1;
    }

    struct ext2_inode inode;
    memset(&inode, 0, sizeof(inode));
    inode.i_mode = EXT2_S_IFREG | 0644;
    inode.i_uid = 0;
    inode.i_gid = 0;
    inode.i_links_count = 1;
    inode.i_blocks = 0;
    inode.i_size = 0;

    if (ext2_write_inode(fs, *inode_num, &inode) != 0) {
        ext2_free_inode(fs, *inode_num);
        return -1;
    }

    return 0;
}