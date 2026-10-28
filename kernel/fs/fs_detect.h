/*
 * Lethal Kernel - Filesystem Auto-Detection
 *
 * v4.0: Automatically detect filesystem type from disk.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_FS_DETECT_H
#define LETHAL_FS_DETECT_H

#include <stdint.h>
#include "../drivers/block/block.h"

/* Filesystem type constants */
#define FS_TYPE_UNKNOWN  0
#define FS_TYPE_EXT2     1
#define FS_TYPE_EXT3     2
#define FS_TYPE_EXT4     3
#define FS_TYPE_FAT16    4
#define FS_TYPE_FAT32    5
#define FS_TYPE_NTFS     6

/* Function prototypes */
int fs_detect_partition(uint32_t dev_id, uint32_t start_lba, uint32_t sector_count);
int fs_detect_and_mount(uint32_t dev_id, uint32_t start_lba);
int fs_boot_mount(void);
int fs_create_default(struct block_device *dev);
const char *fs_type_to_string(int fs_type);

#endif /* LETHAL_FS_DETECT_H */