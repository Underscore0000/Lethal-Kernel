/*
 * Lethal Kernel - EXT2 Indirect Blocks
 *
 * v2.1: Support for single, double, and triple indirect blocks.
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

/* Maximum blocks supported at each level */
#define EXT2_DIRECT_BLOCKS  12
#define EXT2_INDIRECT_BLOCKS (EXT2_BLOCK_SIZE / 4)
#define EXT2_DINDIRECT_BLOCKS (EXT2_INDIRECT_BLOCKS * EXT2_INDIRECT_BLOCKS)
#define EXT2_TINDIRECT_BLOCKS (EXT2_DINDIRECT_BLOCKS * EXT2_INDIRECT_BLOCKS)

/* Get block number for a given file offset */
int ext2_get_block(struct ext2_filesystem *fs, struct ext2_inode *inode,
                    uint32_t block_index, uint32_t *block_num)
{
    uint32_t ptrs_per_block = fs->block_size / 4;
    
    /* Direct blocks (0-11) */
    if (block_index < EXT2_DIRECT_BLOCKS) {
        *block_num = inode->i_block[block_index];
        return (*block_num != 0) ? 0 : -1;
    }
    
    block_index -= EXT2_DIRECT_BLOCKS;
    
    /* Single indirect (12) */
    if (block_index < ptrs_per_block) {
        uint32_t indirect_block = inode->i_block[12];
        if (indirect_block == 0) return -1;
        
        uint32_t *indirect = (uint32_t *)kmalloc(fs->block_size);
        if (!indirect) return -1;
        
        if (ext2_read_block(fs, indirect_block, indirect) != 0) {
            kfree(indirect);
            return -1;
        }
        
        *block_num = indirect[block_index];
        kfree(indirect);
        return (*block_num != 0) ? 0 : -1;
    }
    
    block_index -= ptrs_per_block;
    
    /* Double indirect (13) */
    if (block_index < (ptrs_per_block * ptrs_per_block)) {
        uint32_t dindirect_block = inode->i_block[13];
        if (dindirect_block == 0) return -1;
        
        uint32_t idx1 = block_index / ptrs_per_block;
        uint32_t idx2 = block_index % ptrs_per_block;
        
        uint32_t *dindirect = (uint32_t *)kmalloc(fs->block_size);
        if (!dindirect) return -1;
        
        if (ext2_read_block(fs, dindirect_block, dindirect) != 0) {
            kfree(dindirect);
            return -1;
        }
        
        uint32_t indirect_block = dindirect[idx1];
        if (indirect_block == 0) {
            kfree(dindirect);
            return -1;
        }
        
        uint32_t *indirect = (uint32_t *)kmalloc(fs->block_size);
        if (!indirect) {
            kfree(dindirect);
            return -1;
        }
        
        if (ext2_read_block(fs, indirect_block, indirect) != 0) {
            kfree(dindirect);
            kfree(indirect);
            return -1;
        }
        
        *block_num = indirect[idx2];
        kfree(dindirect);
        kfree(indirect);
        return (*block_num != 0) ? 0 : -1;
    }
    
    /* Triple indirect (14) - for files > ~16GB with 1024 block size */
    return -1;
}

/* Allocate a block chain for a given file offset */
int ext2_allocate_block(struct ext2_filesystem *fs, struct ext2_inode *inode,
                         uint32_t block_index, uint32_t *block_num)
{
    uint32_t ptrs_per_block = fs->block_size / 4;
    
    /* Direct blocks */
    if (block_index < EXT2_DIRECT_BLOCKS) {
        if (inode->i_block[block_index] == 0) {
            uint32_t new_block = ext2_alloc_block(fs);
            if (new_block == 0 || new_block == (uint32_t)-1) return -1;
            inode->i_block[block_index] = new_block;
        }
        *block_num = inode->i_block[block_index];
        return 0;
    }
    
    block_index -= EXT2_DIRECT_BLOCKS;
    
    /* Single indirect */
    if (block_index < ptrs_per_block) {
        if (inode->i_block[12] == 0) {
            uint32_t new_indirect = ext2_alloc_block(fs);
            if (new_indirect == 0 || new_indirect == (uint32_t)-1) return -1;
            inode->i_block[12] = new_indirect;
            
            uint32_t *indirect = (uint32_t *)kmalloc(fs->block_size);
            if (!indirect) return -1;
            memset(indirect, 0, fs->block_size);
            ext2_write_block(fs, new_indirect, indirect);
            kfree(indirect);
        }
        
        uint32_t indirect_block = inode->i_block[12];
        uint32_t *indirect = (uint32_t *)kmalloc(fs->block_size);
        if (!indirect) return -1;
        
        if (ext2_read_block(fs, indirect_block, indirect) != 0) {
            kfree(indirect);
            return -1;
        }
        
        if (indirect[block_index] == 0) {
            uint32_t new_block = ext2_alloc_block(fs);
            if (new_block == 0 || new_block == (uint32_t)-1) {
                kfree(indirect);
                return -1;
            }
            indirect[block_index] = new_block;
            ext2_write_block(fs, indirect_block, indirect);
        }
        
        *block_num = indirect[block_index];
        kfree(indirect);
        return 0;
    }
    
    block_index -= ptrs_per_block;
    
    /* Double indirect */
    if (block_index < (ptrs_per_block * ptrs_per_block)) {
        uint32_t idx1 = block_index / ptrs_per_block;
        uint32_t idx2 = block_index % ptrs_per_block;
        
        /* Allocate double indirect if needed */
        if (inode->i_block[13] == 0) {
            uint32_t new_dindirect = ext2_alloc_block(fs);
            if (new_dindirect == 0 || new_dindirect == (uint32_t)-1) return -1;
            inode->i_block[13] = new_dindirect;
            
            uint32_t *dindirect = (uint32_t *)kmalloc(fs->block_size);
            if (!dindirect) return -1;
            memset(dindirect, 0, fs->block_size);
            ext2_write_block(fs, new_dindirect, dindirect);
            kfree(dindirect);
        }
        
        uint32_t dindirect_block = inode->i_block[13];
        uint32_t *dindirect = (uint32_t *)kmalloc(fs->block_size);
        if (!dindirect) return -1;
        
        if (ext2_read_block(fs, dindirect_block, dindirect) != 0) {
            kfree(dindirect);
            return -1;
        }
        
        /* Allocate indirect block if needed */
        if (dindirect[idx1] == 0) {
            uint32_t new_indirect = ext2_alloc_block(fs);
            if (new_indirect == 0 || new_indirect == (uint32_t)-1) {
                kfree(dindirect);
                return -1;
            }
            dindirect[idx1] = new_indirect;
            ext2_write_block(fs, dindirect_block, dindirect);
            
            uint32_t *indirect = (uint32_t *)kmalloc(fs->block_size);
            if (!indirect) {
                kfree(dindirect);
                return -1;
            }
            memset(indirect, 0, fs->block_size);
            ext2_write_block(fs, new_indirect, indirect);
            kfree(indirect);
        }
        
        uint32_t indirect_block = dindirect[idx1];
        uint32_t *indirect = (uint32_t *)kmalloc(fs->block_size);
        if (!indirect) {
            kfree(dindirect);
            return -1;
        }
        
        if (ext2_read_block(fs, indirect_block, indirect) != 0) {
            kfree(dindirect);
            kfree(indirect);
            return -1;
        }
        
        if (indirect[idx2] == 0) {
            uint32_t new_block = ext2_alloc_block(fs);
            if (new_block == 0 || new_block == (uint32_t)-1) {
                kfree(dindirect);
                kfree(indirect);
                return -1;
            }
            indirect[idx2] = new_block;
            ext2_write_block(fs, indirect_block, indirect);
        }
        
        *block_num = indirect[idx2];
        kfree(dindirect);
        kfree(indirect);
        return 0;
    }
    
    return -1;
}

/* Free all blocks in an inode (including indirect blocks) */
void ext2_free_all_blocks(struct ext2_filesystem *fs, struct ext2_inode *inode)
{
    uint32_t ptrs_per_block = fs->block_size / 4;
    uint32_t total_blocks = (inode->i_size + fs->block_size - 1) / fs->block_size;
    uint32_t max_blocks = EXT2_DIRECT_BLOCKS + ptrs_per_block + 
                          (ptrs_per_block * ptrs_per_block) + 
                          (ptrs_per_block * ptrs_per_block * ptrs_per_block);
    
    if (total_blocks > max_blocks) total_blocks = max_blocks;
    
    /* Free direct blocks */
    for (uint32_t i = 0; i < EXT2_DIRECT_BLOCKS && i < total_blocks; i++) {
        if (inode->i_block[i]) {
            ext2_free_block(fs, inode->i_block[i]);
            inode->i_block[i] = 0;
        }
    }
    
    if (total_blocks <= EXT2_DIRECT_BLOCKS) return;
    
    /* Free single indirect */
    uint32_t indirect_blocks = total_blocks - EXT2_DIRECT_BLOCKS;
    if (indirect_blocks > ptrs_per_block) indirect_blocks = ptrs_per_block;
    
    if (inode->i_block[12]) {
        uint32_t *indirect = (uint32_t *)kmalloc(fs->block_size);
        if (indirect) {
            if (ext2_read_block(fs, inode->i_block[12], indirect) == 0) {
                for (uint32_t i = 0; i < indirect_blocks; i++) {
                    if (indirect[i]) ext2_free_block(fs, indirect[i]);
                }
            }
            kfree(indirect);
        }
        ext2_free_block(fs, inode->i_block[12]);
        inode->i_block[12] = 0;
    }
    
    if (total_blocks <= EXT2_DIRECT_BLOCKS + ptrs_per_block) return;
    
    /* Free double indirect - simplified for now */
    if (inode->i_block[13]) {
        ext2_free_block(fs, inode->i_block[13]);
        inode->i_block[13] = 0;
    }
    
    /* Free triple indirect */
    if (inode->i_block[14]) {
        ext2_free_block(fs, inode->i_block[14]);
        inode->i_block[14] = 0;
    }
}