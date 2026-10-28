/*
 * Lethal Kernel - EXT2 VFS Bindings
 *
 * v2.0: VFS interface for EXT2 filesystem.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext2.h"
#include "../vfs.h"
#include "../../core/console.h"
#include "../../core/panic.h"
#include "../../memory/heap.h"
#include <string.h>

static struct ext2_filesystem *fs = NULL;
static uint32_t root_inode = 2;

struct ext2_file_handle {
    uint32_t inode_num;
    uint32_t flags;
    uint32_t position;
    struct ext2_filesystem *fs;
};

/* Forward declarations from ext2_super.c */
extern void ext2_set_fs(struct ext2_filesystem *f);

/* Forward declarations from ext2_super.c */
extern int ext2_read_superblock(struct ext2_filesystem *fs);
extern int ext2_verify_superblock(struct ext2_superblock *sb);
extern int ext2_read_group_desc(struct ext2_filesystem *fs);

/* Forward declarations from ext2_inode.c */
extern int ext2_read_inode(struct ext2_filesystem *fs, uint32_t inode_num, struct ext2_inode *inode);
extern int ext2_write_inode(struct ext2_filesystem *fs, uint32_t inode_num, struct ext2_inode *inode);
extern int ext2_alloc_inode_core(struct ext2_filesystem *fs, uint32_t *inode_num);

/* Forward declarations from ext2_dir.c */
extern int ext2_lookup(struct ext2_filesystem *fs, uint32_t dir_inode, const char *name, uint32_t *inode_num);
extern int ext2_readdir(struct ext2_filesystem *fs, uint32_t dir_inode, void (*callback)(const char *name, uint32_t inode, uint8_t type));
extern int ext2_mkdir(struct ext2_filesystem *fs, uint32_t parent_inode, const char *name, uint32_t mode);

/* Forward declarations from ext2_file.c */
extern int ext2_read_file(struct ext2_filesystem *fs, uint32_t inode_num, void *buffer, size_t offset, size_t count);
extern int ext2_write_file(struct ext2_filesystem *fs, uint32_t inode_num, const void *buffer, size_t offset, size_t count);
extern int ext2_delete_file(struct ext2_filesystem *fs, uint32_t inode_num);

int ext2_mount(uint32_t dev_id)
{
    serial_print("[ext2] mounting on device ");
    serial_print_dec(dev_id);
    serial_print("\n");

    struct ext2_filesystem *new_fs = (struct ext2_filesystem *)kmalloc(sizeof(struct ext2_filesystem));
    if (!new_fs) return -1;

    memset(new_fs, 0, sizeof(struct ext2_filesystem));
    new_fs->dev_id = dev_id;
    mutex_init(&new_fs->lock);

    if (ext2_read_superblock(new_fs) != 0) {
        kfree(new_fs);
        serial_print("[ext2] Failed to read superblock\n");
        return -1;
    }

    if (ext2_verify_superblock(&new_fs->sb) != 0) {
        kfree(new_fs);
        serial_print("[ext2] Invalid superblock\n");
        return -1;
    }

    new_fs->block_size = 1024 << new_fs->sb.s_log_block_size;
    new_fs->inode_size = new_fs->sb.s_inode_size ? new_fs->sb.s_inode_size : 128;
    new_fs->blocks_per_group = new_fs->sb.s_blocks_per_group;
    new_fs->inodes_per_group = new_fs->sb.s_inodes_per_group;
    new_fs->first_data_block = new_fs->sb.s_first_data_block;
    new_fs->groups_count = (new_fs->sb.s_blocks_count + new_fs->blocks_per_group - 1) / 
                            new_fs->blocks_per_group;

    serial_print("[ext2] Block size: ");
    serial_print_dec(new_fs->block_size);
    serial_print(" bytes\n");
    serial_print("[ext2] Inodes: ");
    serial_print_dec(new_fs->sb.s_inodes_count);
    serial_print("\n");
    serial_print("[ext2] Blocks: ");
    serial_print_dec(new_fs->sb.s_blocks_count);
    serial_print("\n");
    serial_print("[ext2] Groups: ");
    serial_print_dec(new_fs->groups_count);
    serial_print("\n");

    if (ext2_read_group_desc(new_fs) != 0) {
        kfree(new_fs);
        serial_print("[ext2] Failed to read group descriptors\n");
        return -1;
    }

    struct ext2_inode root_inode_data;
    if (ext2_read_inode(new_fs, root_inode, &root_inode_data) != 0) {
        kfree(new_fs->gdt);
        kfree(new_fs);
        serial_print("[ext2] Root inode not found\n");
        return -1;
    }

    ext2_set_fs(new_fs);
    fs = new_fs;
    new_fs->mounted = 1;

    serial_print("[ext2] Mounted successfully\n");
    return 0;
}

int ext2_unmount(void)
{
    if (!fs) return -1;

    mutex_lock(&fs->lock);
    fs->mounted = 0;
    mutex_unlock(&fs->lock);

    if (fs->gdt) kfree(fs->gdt);
    kfree(fs);
    fs = NULL;
    ext2_set_fs(NULL);

    serial_print("[ext2] Unmounted\n");
    return 0;
}

int ext2_vfs_open(const char *path, uint32_t flags, void **handle)
{
    if (!fs || !fs->mounted) return -1;
    if (!path || path[0] != '/') return -1;

    mutex_lock(&fs->lock);

    uint32_t current_inode = root_inode;
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char *token = strtok(path_copy + 1, "/");
    
    while (token) {
        if (strcmp(token, ".") == 0) {
            token = strtok(NULL, "/");
            continue;
        }
        if (strcmp(token, "..") == 0) {
            current_inode = root_inode;
            token = strtok(NULL, "/");
            continue;
        }

        uint32_t next_inode;
        if (ext2_lookup(fs, current_inode, token, &next_inode) != 0) {
            mutex_unlock(&fs->lock);
            return -1;
        }

        struct ext2_inode inode;
        if (ext2_read_inode(fs, next_inode, &inode) != 0) {
            mutex_unlock(&fs->lock);
            return -1;
        }

        current_inode = next_inode;
        token = strtok(NULL, "/");
    }

    struct ext2_file_handle *fh = (struct ext2_file_handle *)kmalloc(sizeof(struct ext2_file_handle));
    if (!fh) {
        mutex_unlock(&fs->lock);
        return -1;
    }

    fh->inode_num = current_inode;
    fh->flags = flags;
    fh->position = 0;
    fh->fs = fs;

    *handle = fh;
    mutex_unlock(&fs->lock);
    return 0;
}

long ext2_vfs_read(void *handle, void *buffer, size_t count)
{
    if (!handle) return -1;

    struct ext2_file_handle *fh = (struct ext2_file_handle *)handle;
    if (!fh->fs || !fh->fs->mounted) return -1;

    mutex_lock(&fh->fs->lock);
    long result = ext2_read_file(fh->fs, fh->inode_num, buffer, fh->position, count);
    if (result > 0) {
        fh->position += result;
    }
    mutex_unlock(&fh->fs->lock);
    return result;
}

long ext2_vfs_write(void *handle, const void *buffer, size_t count)
{
    if (!handle) return -1;

    struct ext2_file_handle *fh = (struct ext2_file_handle *)handle;
    if (!fh->fs || !fh->fs->mounted) return -1;

    mutex_lock(&fh->fs->lock);
    long result = ext2_write_file(fh->fs, fh->inode_num, buffer, fh->position, count);
    if (result > 0) {
        fh->position += result;
    }
    mutex_unlock(&fh->fs->lock);
    return result;
}

int ext2_vfs_close(void *handle)
{
    if (!handle) return -1;

    struct ext2_file_handle *fh = (struct ext2_file_handle *)handle;
    kfree(fh);
    return 0;
}

long ext2_vfs_size(void *handle)
{
    if (!handle) return -1;

    struct ext2_file_handle *fh = (struct ext2_file_handle *)handle;
    if (!fh->fs || !fh->fs->mounted) return -1;

    struct ext2_inode inode;
    if (ext2_read_inode(fh->fs, fh->inode_num, &inode) != 0) {
        return -1;
    }
    return (long)inode.i_size;
}

int ext2_vfs_mkdir(const char *path)
{
    if (!fs || !fs->mounted) return -1;
    if (!path) return -1;

    mutex_lock(&fs->lock);

    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    size_t len = strlen(path_copy);
    if (len > 1 && path_copy[len - 1] == '/') {
        path_copy[len - 1] = '\0';
    }

    char *last_slash = strrchr(path_copy, '/');
    if (!last_slash) {
        mutex_unlock(&fs->lock);
        return -1;
    }

    *last_slash = '\0';
    const char *dir_name = last_slash + 1;

    uint32_t parent_inode = root_inode;
    if (strlen(path_copy) > 1) {
        void *parent_handle;
        if (ext2_vfs_open(path_copy, 0, &parent_handle) != 0) {
            mutex_unlock(&fs->lock);
            return -1;
        }
        struct ext2_file_handle *fh = (struct ext2_file_handle *)parent_handle;
        parent_inode = fh->inode_num;
        ext2_vfs_close(parent_handle);
    }

    int result = ext2_mkdir(fs, parent_inode, dir_name, 0755);
    mutex_unlock(&fs->lock);
    return result;
}

struct vfs_filesystem_ops ext2_ops = {
    .open = ext2_vfs_open,
    .read = ext2_vfs_read,
    .write = ext2_vfs_write,
    .close = ext2_vfs_close,
    .size = ext2_vfs_size,
    .mkdir = ext2_vfs_mkdir
};

struct vfs_filesystem_ops *ext2_get_ops(void)
{
    return &ext2_ops;
}