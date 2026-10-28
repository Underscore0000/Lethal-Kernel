/*
 * Lethal Kernel - EXT2 File Operations
 *
 * v2.1: Updated to use indirect blocks and buffer cache.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext2.h"
#include "../../memory/heap.h"
#include <string.h>

/* Forward declarations */
extern int ext2_read_block(struct ext2_filesystem *fs, uint32_t block, void *buffer);
extern int ext2_write_block(struct ext2_filesystem *fs, uint32_t block, void *buffer);
extern int ext2_alloc_block(struct ext2_filesystem *fs);
extern void ext2_free_block(struct ext2_filesystem *fs, uint32_t block);
extern int ext2_read_inode(struct ext2_filesystem *fs, uint32_t inode_num, struct ext2_inode *inode);
extern int ext2_write_inode(struct ext2_filesystem *fs, uint32_t inode_num, struct ext2_inode *inode);
extern int ext2_alloc_inode(struct ext2_filesystem *fs, uint32_t *inode_num);
extern void ext2_free_inode(struct ext2_filesystem *fs, uint32_t inode_num);

/* Indirect block functions */
extern int ext2_get_block(struct ext2_filesystem *fs, struct ext2_inode *inode,
                           uint32_t block_index, uint32_t *block_num);
extern int ext2_allocate_block(struct ext2_filesystem *fs, struct ext2_inode *inode,
                                uint32_t block_index, uint32_t *block_num);
extern void ext2_free_all_blocks(struct ext2_filesystem *fs, struct ext2_inode *inode);

/* Buffer cache functions */
extern int ext2_cache_read(struct ext2_filesystem *fs, uint32_t block, void *buffer);
extern int ext2_cache_write(struct ext2_filesystem *fs, uint32_t block, void *buffer);
extern void ext2_cache_flush(struct ext2_filesystem *fs);

int ext2_create_file(struct ext2_filesystem *fs, uint32_t parent_inode,
                      const char *name, uint32_t mode)
{
    uint32_t inode_num;
    if (ext2_alloc_inode(fs, &inode_num) != 0) {
        return -1;
    }
    
    struct ext2_inode inode;
    memset(&inode, 0, sizeof(inode));
    inode.i_mode = EXT2_S_IFREG | (mode & 0777);
    inode.i_uid = 0;
    inode.i_gid = 0;
    inode.i_links_count = 1;
    inode.i_blocks = 0;
    inode.i_size = 0;
    
    ext2_write_inode(fs, inode_num, &inode);
    
    return ext2_dir_write_entry(fs, parent_inode, name, inode_num, EXT2_FT_REG_FILE);
}

int ext2_read_file(struct ext2_filesystem *fs, uint32_t inode_num,
                    void *buffer, size_t offset, size_t count)
{
    struct ext2_inode inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) {
        return -1;
    }

    if (offset >= inode.i_size) return 0;
    if (offset + count > inode.i_size) {
        count = inode.i_size - offset;
    }

    uint8_t *dst = (uint8_t *)buffer;
    uint8_t *block_buffer = (uint8_t *)kmalloc(fs->block_size);
    if (!block_buffer) return -1;

    uint32_t start_block = offset / fs->block_size;
    uint32_t end_block = (offset + count - 1) / fs->block_size;
    uint32_t start_offset = offset % fs->block_size;
    size_t bytes_read = 0;

    for (uint32_t block_idx = start_block; block_idx <= end_block; block_idx++) {
        uint32_t block_num;
        if (ext2_get_block(fs, &inode, block_idx, &block_num) != 0) {
            kfree(block_buffer);
            return bytes_read;
        }

        if (ext2_cache_read(fs, block_num, block_buffer) != 0) {
            kfree(block_buffer);
            return bytes_read;
        }

        size_t copy_offset = (block_idx == start_block) ? start_offset : 0;
        size_t copy_size = fs->block_size - copy_offset;
        if (bytes_read + copy_size > count) {
            copy_size = count - bytes_read;
        }

        memcpy(dst + bytes_read, block_buffer + copy_offset, copy_size);
        bytes_read += copy_size;
    }

    kfree(block_buffer);
    return bytes_read;
}

int ext2_write_file(struct ext2_filesystem *fs, uint32_t inode_num,
                     const void *buffer, size_t offset, size_t count)
{
    struct ext2_inode inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) {
        return -1;
    }

    const uint8_t *src = (const uint8_t *)buffer;
    uint8_t *block_buffer = (uint8_t *)kmalloc(fs->block_size);
    if (!block_buffer) return -1;

    /* Extend file if needed */
    if (offset + count > inode.i_size) {
        uint32_t total_blocks = (offset + count + fs->block_size - 1) / fs->block_size;
        uint32_t current_blocks = (inode.i_size + fs->block_size - 1) / fs->block_size;

        for (uint32_t block_idx = current_blocks; block_idx < total_blocks; block_idx++) {
            uint32_t new_block;
            if (ext2_allocate_block(fs, &inode, block_idx, &new_block) != 0) {
                kfree(block_buffer);
                return -1;
            }
        }
    }

    uint32_t start_block = offset / fs->block_size;
    uint32_t end_block = (offset + count - 1) / fs->block_size;
    uint32_t start_offset = offset % fs->block_size;
    size_t bytes_written = 0;

    for (uint32_t block_idx = start_block; block_idx <= end_block; block_idx++) {
        uint32_t block_num;
        if (ext2_get_block(fs, &inode, block_idx, &block_num) != 0) {
            kfree(block_buffer);
            return bytes_written;
        }

        if (ext2_cache_read(fs, block_num, block_buffer) != 0) {
            kfree(block_buffer);
            return bytes_written;
        }

        size_t write_offset = (block_idx == start_block) ? start_offset : 0;
        size_t write_size = fs->block_size - write_offset;
        if (bytes_written + write_size > count) {
            write_size = count - bytes_written;
        }

        memcpy(block_buffer + write_offset, src + bytes_written, write_size);
        ext2_cache_write(fs, block_num, block_buffer);
        bytes_written += write_size;
    }

    /* Update inode size */
    if (offset + count > inode.i_size) {
        inode.i_size = offset + count;
        inode.i_blocks = (inode.i_size + fs->block_size - 1) / fs->block_size * (fs->block_size / 512);
    }

    ext2_write_inode(fs, inode_num, &inode);
    ext2_cache_flush(fs);
    kfree(block_buffer);
    return bytes_written;
}

int ext2_delete_file(struct ext2_filesystem *fs, uint32_t inode_num)
{
    struct ext2_inode inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) {
        return -1;
    }

    /* Free all data blocks */
    ext2_free_all_blocks(fs, &inode);

    /* Update inode */
    memset(&inode, 0, sizeof(inode));
    inode.i_dtime = 0; /* Current time would go here */
    ext2_write_inode(fs, inode_num, &inode);

    /* Free the inode */
    ext2_free_inode(fs, inode_num);
    ext2_cache_flush(fs);
    return 0;
}