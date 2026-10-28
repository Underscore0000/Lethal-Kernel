/*
 * Lethal Kernel - EXT4 Extents
 *
 * v4.0: Efficient extent-based block mapping.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext4.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>

#define EXT4_EXTENT_MAGIC 0xF30A

/* Extent depth */
#define EXT4_EXTENT_LEAF 0
#define EXT4_EXTENT_INDEX 1

/* Helper to read an extent block */
static int ext4_read_extent_block(struct ext4_filesystem *fs, uint32_t block,
                                   struct ext4_extent_header **header)
{
    uint8_t *buffer = (uint8_t *)kmalloc(fs->block_size);
    if (!buffer) return -1;
    
    if (ext2_read_block(&fs->ext3.ext2, block, buffer) != 0) {
        kfree(buffer);
        return -1;
    }
    
    *header = (struct ext4_extent_header *)buffer;
    if ((*header)->eh_magic != EXT4_EXTENT_MAGIC) {
        kfree(buffer);
        return -1;
    }
    
    return 0;
}

/* Initialize an extent header */
static void ext4_init_extent_header(struct ext4_extent_header *header, 
                                      uint16_t entries, uint16_t max, uint16_t depth)
{
    header->eh_magic = EXT4_EXTENT_MAGIC;
    header->eh_entries = entries;
    header->eh_max = max;
    header->eh_depth = depth;
    header->eh_generation = 0;
}

/* Get block using extents */
int ext4_extent_get_block(struct ext4_filesystem *fs, struct ext2_inode *inode,
                           uint32_t block_index, uint32_t *block_num)
{
    if (!fs || !inode || !block_num) return -1;
    
    /* Check if inode uses extents */
    if (!(inode->i_flags & EXT4_INODE_EXTENTS)) {
        /* Fallback to indirect blocks */
        return ext2_get_block(&fs->ext3.ext2, inode, block_index, block_num);
    }
    
    /* The first block of the inode contains the extent header */
    uint32_t first_block = inode->i_block[0];
    if (first_block == 0) {
        return -1;
    }
    
    struct ext4_extent_header *header;
    if (ext4_read_extent_block(fs, first_block, &header) != 0) {
        return -1;
    }
    
    /* Walk the extent tree if needed */
    /* For simplicity, we only support leaf level for now */
    if (header->eh_depth != EXT4_EXTENT_LEAF) {
        /* In a real implementation, we would traverse the tree */
        serial_print("[ext4] WARNING: extent depth > 0 not supported yet\n");
        return -1;
    }
    
    /* Find the extent that covers block_index */
    struct ext4_extent *extent = (struct ext4_extent *)(header + 1);
    for (uint16_t i = 0; i < header->eh_entries; i++) {
        if (block_index >= extent[i].ee_block &&
            block_index < extent[i].ee_block + (extent[i].ee_len & 0x7FFF)) {
            uint32_t offset = block_index - extent[i].ee_block;
            uint32_t start = extent[i].ee_start | ((uint32_t)extent[i].ee_start_hi << 16);
            *block_num = start + offset;
            return 0;
        }
    }
    
    return -1;
}

/* Allocate a new extent */
int ext4_extent_alloc_block(struct ext4_filesystem *fs, struct ext2_inode *inode,
                             uint32_t block_index, uint32_t *block_num)
{
    if (!fs || !inode || !block_num) return -1;
    
    /* Allocate physical block */
    uint32_t new_block = ext2_alloc_block(&fs->ext3.ext2);
    if (new_block == 0 || new_block == (uint32_t)-1) {
        return -1;
    }
    
    /* Check if inode uses extents */
    if (!(inode->i_flags & EXT4_INODE_EXTENTS)) {
        /* Enable extents for this inode */
        inode->i_flags |= EXT4_INODE_EXTENTS;
    }
    
    /* Initialize extent header if needed */
    uint32_t first_block = inode->i_block[0];
    struct ext4_extent_header *header = NULL;
    uint8_t *buffer = NULL;
    int is_new = 0;
    
    if (first_block == 0) {
        /* Allocate block for extent tree root */
        first_block = ext2_alloc_block(&fs->ext3.ext2);
        if (first_block == 0 || first_block == (uint32_t)-1) {
            ext2_free_block(&fs->ext3.ext2, new_block);
            return -1;
        }
        inode->i_block[0] = first_block;
        is_new = 1;
        buffer = (uint8_t *)kmalloc(fs->block_size);
        if (!buffer) {
            ext2_free_block(&fs->ext3.ext2, first_block);
            ext2_free_block(&fs->ext3.ext2, new_block);
            return -1;
        }
        memset(buffer, 0, fs->block_size);
        header = (struct ext4_extent_header *)buffer;
        ext4_init_extent_header(header, 0, 4, EXT4_EXTENT_LEAF);
    } else {
        if (ext4_read_extent_block(fs, first_block, &header) != 0) {
            ext2_free_block(&fs->ext3.ext2, new_block);
            return -1;
        }
        buffer = (uint8_t *)header;
    }
    
    /* Add a new extent */
    struct ext4_extent *extent = (struct ext4_extent *)(header + 1);
    uint16_t entry = header->eh_entries;
    
    /* Simple case: append extent */
    extent[entry].ee_block = block_index;
    extent[entry].ee_len = 1;  /* One block */
    extent[entry].ee_start = new_block & 0xFFFFFFFF;
    extent[entry].ee_start_hi = (new_block >> 16) & 0xFFFF;
    header->eh_entries++;
    
    /* Write back */
    ext2_write_block(&fs->ext3.ext2, first_block, buffer);
    
    if (is_new) {
        kfree(buffer);
    }
    
    *block_num = new_block;
    return 0;
}

/* Free all extents for an inode */
int ext4_extent_free_all(struct ext4_filesystem *fs, struct ext2_inode *inode)
{
    if (!fs || !inode) return -1;
    
    if (!(inode->i_flags & EXT4_INODE_EXTENTS)) {
        return 0;
    }
    
    uint32_t first_block = inode->i_block[0];
    if (first_block == 0) return 0;
    
    struct ext4_extent_header *header;
    if (ext4_read_extent_block(fs, first_block, &header) != 0) {
        return -1;
    }
    
    /* Free all extents */
    struct ext4_extent *extent = (struct ext4_extent *)(header + 1);
    for (uint16_t i = 0; i < header->eh_entries; i++) {
        uint32_t start = extent[i].ee_start | ((uint32_t)extent[i].ee_start_hi << 16);
        uint32_t len = extent[i].ee_len & 0x7FFF;
        for (uint32_t b = 0; b < len; b++) {
            ext2_free_block(&fs->ext3.ext2, start + b);
        }
    }
    
    /* Free the extent tree block */
    ext2_free_block(&fs->ext3.ext2, first_block);
    inode->i_block[0] = 0;
    
    return 0;
}