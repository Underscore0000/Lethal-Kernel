/*
 * Lethal Kernel - Virtual Filesystem Layer
 *
 * v4.0: Added EXT3, EXT4, and partition offset support.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_VFS_H
#define LETHAL_VFS_H

#include <stddef.h>
#include <stdint.h>

typedef struct vfs_file vfs_file_t;

/* VFS operations structure */
typedef struct vfs_filesystem_ops {
    int  (*open)(const char *path, uint32_t flags, void **out);
    long (*read)(void *handle, void *buffer, size_t count);
    long (*write)(void *handle, const void *buffer, size_t count);
    int  (*close)(void *handle);
    long (*size)(void *handle);
    int  (*mkdir)(const char *path);
} vfs_filesystem_ops_t;

/* File open flags */
#define VFS_O_READ    0x0001u
#define VFS_O_WRITE   0x0002u
#define VFS_O_CREAT   0x0004u
#define VFS_O_TRUNC   0x0008u
#define VFS_O_APPEND  0x0010u

/* VFS initialization */
void vfs_init(void);

/* Filesystem mounting - standard */
int vfs_mount_fat16(uint32_t ata_device);
int vfs_mount_ext2(uint32_t ata_device);
int vfs_mount_ext3(uint32_t ata_device);
int vfs_mount_ext4(uint32_t ata_device);

/* Filesystem mounting - with partition offset */
int vfs_mount_ext2_at(uint32_t dev_id, uint32_t start_lba);
int vfs_mount_ext3_at(uint32_t dev_id, uint32_t start_lba);
int vfs_mount_ext4_at(uint32_t dev_id, uint32_t start_lba);
int vfs_mount_fat16_at(uint32_t dev_id, uint32_t start_lba);

/* File operations */
int  vfs_open(const char *path, uint32_t flags, void **out);
long vfs_read(void *handle, void *buffer, size_t count);
long vfs_write(void *handle, const void *buffer, size_t count);
int  vfs_close(void *handle);
long vfs_size(void *handle);

/* Directory operations */
int vfs_mkdir(const char *path);

#endif /* LETHAL_VFS_H */