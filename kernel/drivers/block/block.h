/*
 * Lethal Kernel - Block Device Layer
 *
 * v4.0: Generic block device interface for multiple storage devices.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_BLOCK_H
#define LETHAL_BLOCK_H

#include <stdint.h>
#include <stddef.h>

#define BLOCK_DEVICE_MAX 8
#define PARTITION_MAX 16

/* Block device types */
#define BLOCK_TYPE_ATA      1
#define BLOCK_TYPE_SATA     2
#define BLOCK_TYPE_NVME     3
#define BLOCK_TYPE_VIRTIO   4
#define BLOCK_TYPE_RAMDISK  5

/* Partition types */
#define PART_TYPE_EXT2      0x83
#define PART_TYPE_EXT3      0x83
#define PART_TYPE_EXT4      0x83
#define PART_TYPE_FAT16     0x06
#define PART_TYPE_FAT32     0x0B
#define PART_TYPE_LINUX     0x83
#define PART_TYPE_EFI       0xEF
#define PART_TYPE_SWAP      0x82

/* Block device structure */
struct block_device {
    uint32_t id;
    uint32_t type;
    uint32_t sector_size;
    uint64_t sector_count;
    uint32_t flags;
    char name[32];
    uint8_t present;
    int (*read)(uint32_t dev_id, uint32_t lba, void *buffer);
    int (*write)(uint32_t dev_id, uint32_t lba, const void *buffer);
};

/* Partition structure */
struct partition {
    uint32_t dev_id;
    uint32_t partition_num;
    uint32_t start_lba;
    uint32_t sector_count;
    uint32_t type;
    uint8_t bootable;
    char name[16];
};

/* MBR Partition entry */
struct mbr_partition {
    uint8_t bootable;
    uint8_t start_head;
    uint8_t start_sector;
    uint8_t start_cylinder;
    uint8_t type;
    uint8_t end_head;
    uint8_t end_sector;
    uint8_t end_cylinder;
    uint32_t start_lba;
    uint32_t sector_count;
} __attribute__((packed));

/* MBR structure */
struct mbr {
    uint8_t boot_code[446];
    struct mbr_partition partitions[4];
    uint16_t signature;
} __attribute__((packed));

/* Function prototypes */
void block_init(void);
int block_device_register(struct block_device *dev);
int block_device_read(uint32_t dev_id, uint32_t lba, void *buffer);
int block_device_write(uint32_t dev_id, uint32_t lba, const void *buffer);
int block_device_count(void);
struct block_device *block_device_get(uint32_t dev_id);

/* Partition functions */
int partition_scan(uint32_t dev_id);
int partition_count(void);
struct partition *partition_get(uint32_t index);
uint32_t partition_get_device(uint32_t index);
uint32_t partition_get_start_lba(uint32_t index);
uint32_t partition_get_sector_count(uint32_t index);
uint32_t partition_get_type(uint32_t index);

/* Filesystem detection */
int fs_detect(uint32_t dev_id, uint32_t start_lba);

#endif /* LETHAL_BLOCK_H */