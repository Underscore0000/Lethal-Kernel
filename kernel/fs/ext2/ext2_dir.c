/*
 * Lethal Kernel - EXT2 Directory Operations
 *
 * v2.1: Directory reading, lookup, and creation with permissions.
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
extern int ext2_cache_read(struct ext2_filesystem *fs, uint32_t block, void *buffer);
extern int ext2_cache_write(struct ext2_filesystem *fs, uint32_t block, void *buffer);

static uint32_t ext2_dir_entry_size(const char *name)
{
    uint32_t name_len = strlen(name);
    uint32_t size = sizeof(struct ext2_dir_entry) + name_len;
    /* Align to 4 bytes */
    size = (size + 3) & ~3;
    return size;
}

/* This function must be NON-static because it's declared in ext2.h */
int ext2_dir_write_entry(struct ext2_filesystem *fs, uint32_t dir_inode,
                          const char *name, uint32_t inode_num, uint8_t type)
{
    struct ext2_inode dir;
    if (ext2_read_inode(fs, dir_inode, &dir) != 0) {
        return -1;
    }

    uint8_t *block_buffer = (uint8_t *)kmalloc(fs->block_size);
    if (!block_buffer) return -1;

    uint32_t entry_size = ext2_dir_entry_size(name);
    uint32_t total_blocks = (dir.i_size + fs->block_size - 1) / fs->block_size;

    /* Check if we can fit in existing blocks */
    for (uint32_t block_idx = 0; block_idx < total_blocks; block_idx++) {
        uint32_t block = dir.i_block[block_idx];
        if (ext2_cache_read(fs, block, block_buffer) != 0) {
            kfree(block_buffer);
            return -1;
        }

        uint32_t offset = 0;
        struct ext2_dir_entry *entry;
        
        while (offset < fs->block_size) {
            entry = (struct ext2_dir_entry *)(block_buffer + offset);
            uint32_t rec_len = entry->rec_len;
            
            /* Check if this entry is unused or can be split */
            if (entry->inode == 0 || 
                (rec_len >= entry_size + sizeof(struct ext2_dir_entry))) {
                /* Insert new entry */
                if (entry->inode != 0) {
                    /* Split the entry */
                    uint32_t old_rec_len = entry->rec_len;
                    entry->rec_len = entry_size;
                    offset += entry_size;
                    
                    struct ext2_dir_entry *next = 
                        (struct ext2_dir_entry *)(block_buffer + offset);
                    next->inode = 0;
                    next->rec_len = old_rec_len - entry_size;
                    next->name_len = 0;
                    next->file_type = 0;
                }
                
                /* Fill the entry */
                entry->inode = inode_num;
                entry->name_len = strlen(name);
                entry->file_type = type;
                memcpy(entry->name, name, entry->name_len);
                entry->rec_len = entry_size;
                
                ext2_cache_write(fs, block, block_buffer);
                kfree(block_buffer);
                return 0;
            }
            offset += rec_len;
        }
    }

    /* Need a new block */
    uint32_t new_block = ext2_alloc_block(fs);
    if (new_block == 0 || new_block == (uint32_t)-1) {
        kfree(block_buffer);
        return -1;
    }

    memset(block_buffer, 0, fs->block_size);
    
    struct ext2_dir_entry *entry = (struct ext2_dir_entry *)block_buffer;
    entry->inode = inode_num;
    entry->name_len = strlen(name);
    entry->file_type = type;
    memcpy(entry->name, name, entry->name_len);
    entry->rec_len = fs->block_size;
    
    ext2_cache_write(fs, new_block, block_buffer);
    
    /* Add block to inode */
    uint32_t block_idx = total_blocks;
    if (block_idx < 15) {
        dir.i_block[block_idx] = new_block;
    } else {
        kfree(block_buffer);
        return -1;
    }
    dir.i_size += fs->block_size;
    dir.i_blocks += fs->block_size / 512;
    
    ext2_write_inode(fs, dir_inode, &dir);
    kfree(block_buffer);
    return 0;
}

int ext2_lookup(struct ext2_filesystem *fs, uint32_t dir_inode,
                 const char *name, uint32_t *inode_num)
{
    struct ext2_inode dir;
    if (ext2_read_inode(fs, dir_inode, &dir) != 0) {
        return -1;
    }

    uint8_t *block_buffer = (uint8_t *)kmalloc(fs->block_size);
    if (!block_buffer) return -1;

    uint32_t total_blocks = (dir.i_size + fs->block_size - 1) / fs->block_size;

    for (uint32_t block_idx = 0; block_idx < total_blocks; block_idx++) {
        uint32_t block = dir.i_block[block_idx];
        if (ext2_cache_read(fs, block, block_buffer) != 0) {
            kfree(block_buffer);
            return -1;
        }

        uint32_t offset = 0;
        while (offset < fs->block_size) {
            struct ext2_dir_entry *entry = 
                (struct ext2_dir_entry *)(block_buffer + offset);
            
            if (entry->inode == 0) {
                offset += entry->rec_len;
                continue;
            }

            if (entry->name_len == strlen(name) &&
                memcmp(entry->name, name, entry->name_len) == 0) {
                *inode_num = entry->inode;
                kfree(block_buffer);
                return 0;
            }

            offset += entry->rec_len;
        }
    }

    kfree(block_buffer);
    return -1;
}

int ext2_readdir(struct ext2_filesystem *fs, uint32_t dir_inode,
                  void (*callback)(const char *name, uint32_t inode, uint8_t type))
{
    struct ext2_inode dir;
    if (ext2_read_inode(fs, dir_inode, &dir) != 0) {
        return -1;
    }

    uint8_t *block_buffer = (uint8_t *)kmalloc(fs->block_size);
    if (!block_buffer) return -1;

    uint32_t total_blocks = (dir.i_size + fs->block_size - 1) / fs->block_size;

    for (uint32_t block_idx = 0; block_idx < total_blocks; block_idx++) {
        uint32_t block = dir.i_block[block_idx];
        if (ext2_cache_read(fs, block, block_buffer) != 0) {
            kfree(block_buffer);
            return -1;
        }

        uint32_t offset = 0;
        while (offset < fs->block_size) {
            struct ext2_dir_entry *entry = 
                (struct ext2_dir_entry *)(block_buffer + offset);
            
            if (entry->inode == 0) {
                offset += entry->rec_len;
                continue;
            }

            if (entry->name_len > 0) {
                char name[EXT2_NAME_LEN + 1];
                memcpy(name, entry->name, entry->name_len);
                name[entry->name_len] = '\0';
                callback(name, entry->inode, entry->file_type);
            }

            offset += entry->rec_len;
        }
    }

    kfree(block_buffer);
    return 0;
}

int ext2_mkdir(struct ext2_filesystem *fs, uint32_t parent_inode,
                const char *name, uint32_t mode)
{
    /* Allocate inode */
    uint32_t inode_num;
    if (ext2_alloc_inode(fs, &inode_num) != 0) {
        return -1;
    }

    /* Initialize inode */
    struct ext2_inode inode;
    memset(&inode, 0, sizeof(inode));
    inode.i_mode = EXT2_S_IFDIR | (mode & 0777);
    inode.i_uid = 0;
    inode.i_gid = 0;
    inode.i_links_count = 2;  /* . and .. */
    inode.i_blocks = 0;
    inode.i_size = 0;

    /* Allocate first block for directory */
    uint32_t block = ext2_alloc_block(fs);
    if (block == 0 || block == (uint32_t)-1) {
        ext2_free_inode(fs, inode_num);
        return -1;
    }

    uint8_t *block_buffer = (uint8_t *)kmalloc(fs->block_size);
    if (!block_buffer) {
        ext2_free_block(fs, block);
        ext2_free_inode(fs, inode_num);
        return -1;
    }

    memset(block_buffer, 0, fs->block_size);

    /* Create "." entry */
    struct ext2_dir_entry *entry = (struct ext2_dir_entry *)block_buffer;
    entry->inode = inode_num;
    entry->name_len = 1;
    entry->file_type = EXT2_FT_DIR;
    memcpy(entry->name, ".", 1);
    entry->rec_len = 12;

    /* Create ".." entry */
    entry = (struct ext2_dir_entry *)(block_buffer + 12);
    entry->inode = parent_inode;
    entry->name_len = 2;
    entry->file_type = EXT2_FT_DIR;
    memcpy(entry->name, "..", 2);
    entry->rec_len = fs->block_size - 12;

    ext2_cache_write(fs, block, block_buffer);

    /* Update inode */
    inode.i_block[0] = block;
    inode.i_size = fs->block_size;
    inode.i_blocks = fs->block_size / 512;

    ext2_write_inode(fs, inode_num, &inode);

    /* Add entry to parent directory */
    ext2_dir_write_entry(fs, parent_inode, name, inode_num, EXT2_FT_DIR);

    /* Increment parent link count */
    struct ext2_inode parent;
    ext2_read_inode(fs, parent_inode, &parent);
    parent.i_links_count++;
    ext2_write_inode(fs, parent_inode, &parent);

    kfree(block_buffer);
    return 0;
}