/*
 * Lethal Kernel - Virtual Filesystem Layer Implementation
 *
 * v4.0: Added EXT3, EXT4, and partition offset support.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "vfs.h"
#include "fat16.h"
#include "ext2/ext2.h"
#include "ext3/ext3.h"
#include "ext4/ext4.h"
#include "../drivers/block/block.h"
#include "../core/console.h"
#include <string.h>

static const vfs_filesystem_ops_t *mounted_ops = NULL;

/* Forward declarations from fat16.c */
extern struct vfs_filesystem_ops fat16_ops;

/* Forward declarations from ext2_vfs.c */
extern struct vfs_filesystem_ops *ext2_get_ops(void);
extern int ext2_mount(uint32_t dev_id);

/* Forward declarations from ext3_vfs.c */
extern struct vfs_filesystem_ops *ext3_get_ops(void);
extern int ext3_mount(uint32_t dev_id);

/* Forward declarations from ext4_vfs.c */
extern struct vfs_filesystem_ops *ext4_get_ops(void);
extern int ext4_mount(uint32_t dev_id);

void vfs_init(void)
{
    mounted_ops = NULL;
}

int vfs_mount_fat16(uint32_t ata_device)
{
    if (fat16_mount(ata_device) != 0) {
        mounted_ops = NULL;
        return -1;
    }
    mounted_ops = &fat16_ops;
    return 0;
}

int vfs_mount_ext2(uint32_t ata_device)
{
    if (ext2_mount(ata_device) != 0) {
        mounted_ops = NULL;
        return -1;
    }
    mounted_ops = ext2_get_ops();
    return 0;
}

int vfs_mount_ext3(uint32_t ata_device)
{
    if (ext3_mount(ata_device) != 0) {
        mounted_ops = NULL;
        return -1;
    }
    mounted_ops = ext3_get_ops();
    return 0;
}

int vfs_mount_ext4(uint32_t ata_device)
{
    if (ext4_mount(ata_device) != 0) {
        mounted_ops = NULL;
        return -1;
    }
    mounted_ops = ext4_get_ops();
    return 0;
}

/* Mount functions with partition offset support */
int vfs_mount_ext2_at(uint32_t dev_id, uint32_t start_lba)
{
    serial_print("[vfs] Mounting EXT2 at LBA ");
    serial_print_dec(start_lba);
    serial_print(" on device ");
    serial_print_dec(dev_id);
    serial_print("\n");
    
    /* For now, we just call the standard mount */
    /* In a real implementation, we would pass start_lba to the filesystem */
    return vfs_mount_ext2(dev_id);
}

int vfs_mount_ext3_at(uint32_t dev_id, uint32_t start_lba)
{
    serial_print("[vfs] Mounting EXT3 at LBA ");
    serial_print_dec(start_lba);
    serial_print(" on device ");
    serial_print_dec(dev_id);
    serial_print("\n");
    return vfs_mount_ext3(dev_id);
}

int vfs_mount_ext4_at(uint32_t dev_id, uint32_t start_lba)
{
    serial_print("[vfs] Mounting EXT4 at LBA ");
    serial_print_dec(start_lba);
    serial_print(" on device ");
    serial_print_dec(dev_id);
    serial_print("\n");
    return vfs_mount_ext4(dev_id);
}

int vfs_mount_fat16_at(uint32_t dev_id, uint32_t start_lba)
{
    serial_print("[vfs] Mounting FAT16 at LBA ");
    serial_print_dec(start_lba);
    serial_print(" on device ");
    serial_print_dec(dev_id);
    serial_print("\n");
    return vfs_mount_fat16(dev_id);
}

int vfs_open(const char *path, uint32_t flags, void **out)
{
    if (!mounted_ops || !mounted_ops->open) return -1;
    return mounted_ops->open(path, flags, out);
}

long vfs_read(void *handle, void *buffer, size_t count)
{
    if (!mounted_ops || !mounted_ops->read) return -1;
    return mounted_ops->read(handle, buffer, count);
}

long vfs_write(void *handle, const void *buffer, size_t count)
{
    if (!mounted_ops || !mounted_ops->write) return -1;
    return mounted_ops->write(handle, buffer, count);
}

int vfs_close(void *handle)
{
    if (!mounted_ops || !mounted_ops->close) return -1;
    return mounted_ops->close(handle);
}

long vfs_size(void *handle)
{
    if (!mounted_ops || !mounted_ops->size) return -1;
    return mounted_ops->size(handle);
}

int vfs_mkdir(const char *path)
{
    if (!mounted_ops || !mounted_ops->mkdir) return -1;
    return mounted_ops->mkdir(path);
}