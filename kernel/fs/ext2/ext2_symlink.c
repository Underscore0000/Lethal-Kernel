/*
 * Lethal Kernel - EXT2 Symbolic and Hard Links
 *
 * v2.1: Support for symbolic links and hard links.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext2.h"
#include "../../memory/heap.h"
#include <string.h>

/* Forward declarations */
extern int ext2_read_file(struct ext2_filesystem *fs, uint32_t inode_num,
                           void *buffer, size_t offset, size_t count);
extern int ext2_write_file(struct ext2_filesystem *fs, uint32_t inode_num,
                            const void *buffer, size_t offset, size_t count);

/* Create a symbolic link */
int ext2_symlink(struct ext2_filesystem *fs, uint32_t parent_inode,
                  const char *name, const char *target, uint32_t mode)
{
    /* Allocate inode */
    uint32_t inode_num;
    if (ext2_alloc_inode(fs, &inode_num) != 0) {
        return -1;
    }
    
    /* Initialize inode */
    struct ext2_inode inode;
    memset(&inode, 0, sizeof(inode));
    inode.i_mode = EXT2_S_IFLNK | (mode & 0777);
    inode.i_uid = 0;
    inode.i_gid = 0;
    inode.i_links_count = 1;
    inode.i_blocks = 0;
    inode.i_size = strlen(target) + 1;
    
    /* Fast symlink: store target in inode blocks */
    if (inode.i_size <= sizeof(inode.i_block)) {
        memcpy(inode.i_block, target, inode.i_size);
        inode.i_blocks = 0;
    } else {
        /* Slow symlink: store target in data blocks */
        ext2_write_file(fs, inode_num, target, 0, inode.i_size);
        inode.i_blocks = (inode.i_size + fs->block_size - 1) / fs->block_size;
        inode.i_blocks *= (fs->block_size / 512);
    }
    
    ext2_write_inode(fs, inode_num, &inode);
    
    /* Add entry to parent directory */
    return ext2_dir_write_entry(fs, parent_inode, name, inode_num, EXT2_FT_SYMLINK);
}

/* Read a symbolic link */
int ext2_read_symlink(struct ext2_filesystem *fs, uint32_t inode_num,
                       char *buffer, size_t size)
{
    struct ext2_inode inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) {
        return -1;
    }
    
    if (!(inode.i_mode & EXT2_S_IFLNK)) {
        return -1;
    }
    
    size_t target_size = inode.i_size;
    if (target_size > size - 1) target_size = size - 1;
    
    if (target_size <= sizeof(inode.i_block)) {
        /* Fast symlink */
        memcpy(buffer, inode.i_block, target_size);
    } else {
        /* Slow symlink */
        ext2_read_file(fs, inode_num, buffer, 0, target_size);
    }
    
    buffer[target_size] = '\0';
    return target_size;
}

/* Create a hard link */
int ext2_hardlink(struct ext2_filesystem *fs, uint32_t dir_inode,
                   const char *name, uint32_t target_inode)
{
    /* Add entry to directory */
    int result = ext2_dir_write_entry(fs, dir_inode, name, target_inode, EXT2_FT_REG_FILE);
    if (result == 0) {
        /* Increment link count */
        struct ext2_inode target;
        if (ext2_read_inode(fs, target_inode, &target) == 0) {
            target.i_links_count++;
            ext2_write_inode(fs, target_inode, &target);
        }
    }
    return result;
}