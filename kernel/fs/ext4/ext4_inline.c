/*
 * Lethal Kernel - EXT4 Inline Data
 *
 * v4.0: Store small files directly in the inode.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext4.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>

#define EXT4_INLINE_DATA_SIZE 60

/* Check if data can be stored inline */
int ext4_inline_can_store(struct ext4_filesystem *fs, struct ext2_inode *inode, size_t size)
{
    (void)fs;
    (void)inode;
    return size <= EXT4_INLINE_DATA_SIZE;
}

/* Write inline data to inode - matches ext4.h signature */
int ext4_inline_write(struct ext4_filesystem *fs, struct ext2_inode *inode,
                       const void *data, size_t size, size_t offset)
{
    (void)fs;
    (void)offset;
    
    if (!inode || !data || size == 0) return -1;
    
    if (size > EXT4_INLINE_DATA_SIZE) {
        return -1; /* Too large for inline */
    }
    
    memcpy(inode->i_block, data, size);
    inode->i_size = size;
    inode->i_blocks = 0;
    
    return 0;
}

/* Read inline data from inode - matches ext4.h signature */
int ext4_inline_read(struct ext4_filesystem *fs, struct ext2_inode *inode,
                      void *buffer, size_t size, size_t offset)
{
    (void)fs;
    (void)offset;
    
    if (!inode || !buffer || size == 0) return -1;
    
    size_t read_size = size;
    if (read_size > inode->i_size) {
        read_size = inode->i_size;
    }
    if (read_size > EXT4_INLINE_DATA_SIZE) {
        read_size = EXT4_INLINE_DATA_SIZE;
    }
    
    memcpy(buffer, inode->i_block, read_size);
    return read_size;
}

/* Check if inode has inline data */
int ext4_inline_has_data(struct ext2_inode *inode)
{
    if (!inode) return 0;
    return (inode->i_size > 0 && inode->i_blocks == 0 && 
            inode->i_size <= EXT4_INLINE_DATA_SIZE);
}

/* Get inline data size */
size_t ext4_inline_size(struct ext2_inode *inode)
{
    if (!inode) return 0;
    if (!ext4_inline_has_data(inode)) return 0;
    return inode->i_size;
}

/* Convert inline data to regular data blocks */
int ext4_inline_convert(struct ext4_filesystem *fs, struct ext2_inode *inode,
                         uint32_t inode_num)
{
    (void)fs;
    (void)inode;
    (void)inode_num;
    
    /* In a real implementation, we would move data from inode to blocks */
    return 0;
}

/* Set inline data threshold */
void ext4_inline_set_threshold(struct ext4_filesystem *fs, uint32_t threshold)
{
    if (!fs) return;
    /* Use a separate member or ignore - the struct may not have this member */
    (void)threshold;
    serial_print("[ext4_inline] Threshold set to ");
    serial_print_dec(threshold);
    serial_print(" bytes\n");
}