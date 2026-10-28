/*
 * Lethal Kernel - EXT4 Filesystem
 *
 * v4.0: Extents, delayed allocation, checksums, and inline data.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_EXT4_H
#define LETHAL_EXT4_H

#include <stdint.h>
#include <stddef.h>
#include "../../core/mutex.h"
#include "../ext3/ext3.h"

/* EXT4 features (incompat) */
#define EXT4_FEATURE_INCOMPAT_EXTENTS   0x0040
#define EXT4_FEATURE_INCOMPAT_FLEX_BG   0x0200
#define EXT4_FEATURE_INCOMPAT_INLINE_DATA 0x2000

/* EXT4 features (ro_compat) */
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE 0x0008
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM  0x0010
#define EXT4_FEATURE_RO_COMPAT_METADATA_CSUM 0x4000

/* Extent header */
struct ext4_extent_header {
    uint16_t eh_magic;           /* 0xF30A */
    uint16_t eh_entries;         /* Number of valid entries */
    uint16_t eh_max;             /* Maximum entries */
    uint16_t eh_depth;           /* Depth of the tree */
    uint32_t eh_generation;      /* Generation number */
} __attribute__((packed));

/* Extent index (internal node) */
struct ext4_extent_index {
    uint32_t ei_block;           /* First logical block */
    uint32_t ei_leaf;            /* Physical block of leaf */
    uint16_t ei_leaf_hi;         /* High 16 bits of leaf */
    uint16_t ei_unused;
} __attribute__((packed));

/* Extent (leaf node) */
struct ext4_extent {
    uint32_t ee_block;           /* First logical block */
    uint16_t ee_len;             /* Number of blocks (low 15 bits) */
    uint16_t ee_start_hi;        /* High 16 bits of start block */
    uint32_t ee_start;           /* Physical block (low 32 bits) */
} __attribute__((packed));

/* EXT4 inode flags */
#define EXT4_INODE_EXTENTS       0x80000000  /* Inode uses extents */
#define EXT4_INODE_INLINE_DATA   0x10000000  /* Inline data in inode */

/* EXT4 filesystem structure */
struct ext4_filesystem {
    struct ext3_filesystem ext3;  /* Base EXT3 filesystem */
    uint32_t inode_size;          /* Inode size (usually 256) */
    uint32_t features_incompat;   /* Incompatible features */
    uint32_t features_ro_compat;  /* Read-only compatible features */
    uint32_t block_size;          /* Block size in bytes */
    mutex_t extent_lock;          /* Lock for extent operations */
    uint8_t checksum_enabled;     /* 1 if checksums are enabled */
};

/* VFS operations */
int ext4_mount(uint32_t dev_id);
int ext4_unmount(void);

/* Extent operations */
int ext4_extent_get_block(struct ext4_filesystem *fs, struct ext2_inode *inode,
                           uint32_t block_index, uint32_t *block_num);
int ext4_extent_alloc_block(struct ext4_filesystem *fs, struct ext2_inode *inode,
                             uint32_t block_index, uint32_t *block_num);
int ext4_extent_free_all(struct ext4_filesystem *fs, struct ext2_inode *inode);

/* Delayed allocation */
int ext4_delalloc_alloc(struct ext4_filesystem *fs, uint32_t inode_num,
                         uint32_t block, uint32_t *allocated_block);
void ext4_delalloc_flush(struct ext4_filesystem *fs);

/* Checksum */
uint32_t ext4_checksum_crc32c(uint32_t crc, const void *data, size_t len);
int ext4_verify_checksum(struct ext4_filesystem *fs, uint32_t block, void *data);
void ext4_calculate_checksum(struct ext4_filesystem *fs, uint32_t block, void *data);

/* Inline data */
int ext4_inline_read(struct ext4_filesystem *fs, struct ext2_inode *inode,
                      void *buffer, size_t offset, size_t count);
int ext4_inline_write(struct ext4_filesystem *fs, struct ext2_inode *inode,
                       const void *buffer, size_t offset, size_t count);

/* VFS bindings */
int ext4_vfs_open(const char *path, uint32_t flags, void **handle);
long ext4_vfs_read(void *handle, void *buffer, size_t count);
long ext4_vfs_write(void *handle, const void *buffer, size_t count);
int ext4_vfs_close(void *handle);
long ext4_vfs_size(void *handle);
int ext4_vfs_mkdir(const char *path);

#endif /* LETHAL_EXT4_H */