/*
 * Lethal Kernel - EXT3 VFS Bindings
 *
 * v3.0: VFS interface with journaling support.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext3.h"
#include "../vfs.h"
#include "../../core/console.h"
#include "../../core/panic.h"
#include "../../memory/heap.h"
#include <string.h>

/* Forward declarations from EXT2 */
extern int ext2_mount(uint32_t dev_id);
extern int ext2_unmount(void);
extern int ext2_vfs_open(const char *path, uint32_t flags, void **handle);
extern long ext2_vfs_read(void *handle, void *buffer, size_t count);
extern long ext2_vfs_write(void *handle, const void *buffer, size_t count);
extern int ext2_vfs_close(void *handle);
extern long ext2_vfs_size(void *handle);
extern int ext2_vfs_mkdir(const char *path);

/* Forward declarations from EXT3 journal */
extern int ext3_journal_init(struct ext3_filesystem *fs);
extern int ext3_journal_start(struct ext3_filesystem *fs, uint32_t max_blocks);
extern int ext3_journal_stop(struct ext3_filesystem *fs);
extern int ext3_journal_commit(struct ext3_filesystem *fs);
extern int ext3_journal_replay(struct ext3_filesystem *fs);
extern int ext3_write_journal_sb(struct ext3_filesystem *fs);

/* Forward declarations from ext3_journal.c */
extern void ext3_journal_set_fs(struct ext3_filesystem *fs);
extern int ext3_write_journal_sb(struct ext3_filesystem *fs);

static struct ext3_filesystem *fs = NULL;

/* Mount EXT3 filesystem */
int ext3_mount(uint32_t dev_id)
{
    serial_print("[ext3] mounting on device ");
    serial_print_dec(dev_id);
    serial_print("\n");
    
    /* Allocate EXT3 filesystem structure */
    fs = (struct ext3_filesystem *)kmalloc(sizeof(struct ext3_filesystem));
    if (!fs) return -1;
    
    memset(fs, 0, sizeof(struct ext3_filesystem));
    
    /* Mount underlying EXT2 */
    if (ext2_mount(dev_id) != 0) {
        kfree(fs);
        return -1;
    }
    

    fs->ext2.block_size = 1024; /* Placeholder - should get from ext2 */
    fs->ext2.dev_id = dev_id;
    
    /* Initialize journal */
    if (ext3_journal_init(fs) != 0) {
        serial_print("[ext3] Journal initialization failed\n");
        ext2_unmount();
        kfree(fs);
        fs = NULL;
        return -1;
    }
    
    ext3_journal_set_fs(fs);
    
    serial_print("[ext3] Mounted successfully with journaling\n");
    return 0;
}

/* Unmount EXT3 */
int ext3_unmount(void)
{
    if (!fs) return -1;
    
    /* Commit any pending transaction */
    if (fs->current_trans) {
        ext3_journal_commit(fs);
    }
    
    /* Flush journal superblock */
    fs->journal_state = EXT3_JOURNAL_CLEAN;
    ext3_write_journal_sb(fs);
    
    /* Unmount underlying EXT2 */
    ext2_unmount();
    
    kfree(fs);
    fs = NULL;
    ext3_journal_set_fs(NULL);
    
    serial_print("[ext3] Unmounted\n");
    return 0;
}

/* VFS operations - wrap EXT2 with journaling */

int ext3_vfs_open(const char *path, uint32_t flags, void **handle)
{
    if (!fs) return -1;
    
    /* Start transaction for write operations */
    if (flags & (VFS_O_WRITE | VFS_O_CREAT | VFS_O_TRUNC)) {
        ext3_journal_start(fs, 64);
    }
    
    int result = ext2_vfs_open(path, flags, handle);
    
    if (flags & (VFS_O_WRITE | VFS_O_CREAT | VFS_O_TRUNC)) {
        ext3_journal_stop(fs);
    }
    
    return result;
}

long ext3_vfs_read(void *handle, void *buffer, size_t count)
{
    return ext2_vfs_read(handle, buffer, count);
}

long ext3_vfs_write(void *handle, const void *buffer, size_t count)
{
    if (!fs) return -1;
    
    /* Start transaction */
    ext3_journal_start(fs, 64);
    
    long result = ext2_vfs_write(handle, buffer, count);
    
    /* Add modified blocks to journal */
    ext3_journal_stop(fs);
    
    return result;
}

int ext3_vfs_close(void *handle)
{
    return ext2_vfs_close(handle);
}

long ext3_vfs_size(void *handle)
{
    return ext2_vfs_size(handle);
}

int ext3_vfs_mkdir(const char *path)
{
    if (!fs) return -1;
    
    /* Start transaction */
    ext3_journal_start(fs, 32);
    
    int result = ext2_vfs_mkdir(path);
    
    ext3_journal_stop(fs);
    
    return result;
}

/* VFS operations structure */
struct vfs_filesystem_ops ext3_ops = {
    .open = ext3_vfs_open,
    .read = ext3_vfs_read,
    .write = ext3_vfs_write,
    .close = ext3_vfs_close,
    .size = ext3_vfs_size,
    .mkdir = ext3_vfs_mkdir
};

struct vfs_filesystem_ops *ext3_get_ops(void)
{
    return &ext3_ops;
}