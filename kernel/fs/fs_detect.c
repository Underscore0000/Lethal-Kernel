/*
 * Lethal Kernel - Filesystem Auto-Detection
 *
 * v4.0: Automatically detect filesystem type from disk.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "fs_detect.h"
#include "vfs.h"
#include "../drivers/block/block.h"
#include "../core/console.h"
#include "../memory/heap.h"
#include <string.h>

/* Convert filesystem type to string */
const char *fs_type_to_string(int fs_type)
{
    switch(fs_type) {
        case FS_TYPE_EXT2:  return "EXT2";
        case FS_TYPE_EXT3:  return "EXT3";
        case FS_TYPE_EXT4:  return "EXT4";
        case FS_TYPE_FAT16: return "FAT16";
        case FS_TYPE_FAT32: return "FAT32";
        case FS_TYPE_NTFS:  return "NTFS";
        default:            return "UNKNOWN";
    }
}

/* Try to detect filesystem on a partition */
int fs_detect_partition(uint32_t dev_id, uint32_t start_lba, uint32_t sector_count)
{
    (void)sector_count;
    
    uint8_t *buffer = (uint8_t *)kmalloc(512);
    if (!buffer) return FS_TYPE_UNKNOWN;
    
    /* Read first sector (boot sector) */
    if (block_device_read(dev_id, start_lba, buffer) != 0) {
        kfree(buffer);
        return FS_TYPE_UNKNOWN;
    }
    
    int result = FS_TYPE_UNKNOWN;
    
    /* Check for FAT16 boot sector */
    uint16_t bps = *(uint16_t *)(buffer + 11);
    uint8_t spc = *(uint8_t *)(buffer + 13);
    uint16_t reserved = *(uint16_t *)(buffer + 14);
    uint8_t fats = *(uint8_t *)(buffer + 16);
    uint16_t root = *(uint16_t *)(buffer + 17);
    uint16_t spf = *(uint16_t *)(buffer + 22);
    
    /* Check boot sector signature */
    if (buffer[510] == 0x55 && buffer[511] == 0xAA) {
        if (bps == 512 && spc > 0 && reserved > 0 && fats > 0 && spf > 0) {
            if (root > 0) {
                result = FS_TYPE_FAT16;
                serial_print("[fs_detect] Detected FAT16 filesystem\n");
            } else {
                result = FS_TYPE_FAT32;
                serial_print("[fs_detect] Detected FAT32 filesystem\n");
            }
            kfree(buffer);
            return result;
        }
    }
    
    /* Check for EXT2/3/4 superblock (at offset 1024) - need to read more */
    uint8_t *ext_buffer = (uint8_t *)kmalloc(4096);
    if (!ext_buffer) {
        kfree(buffer);
        return FS_TYPE_UNKNOWN;
    }
    
    if (block_device_read(dev_id, start_lba, ext_buffer) == 0) {
        uint16_t magic = *(uint16_t *)(ext_buffer + 1024 + 56);
        if (magic == 0xEF53) {
            result = FS_TYPE_EXT2;
            serial_print("[fs_detect] Detected EXT2 filesystem\n");
        }
    }
    
    kfree(ext_buffer);
    kfree(buffer);
    return result;
}

/* Create default FAT16 filesystem */
int fs_create_default(struct block_device *dev)
{
    if (!dev) return -1;
    
    serial_print("[fs_detect] Creating default FAT16 filesystem on ");
    serial_print(dev->name);
    serial_print("\n");
    
    uint8_t *buffer;
    uint32_t total_sectors = dev->sector_count;
    uint32_t sectors_per_fat = 32; /* FAT16 standard */
    uint32_t root_entries = 512;
    uint32_t root_sectors = (root_entries * 32 + 511) / 512;
    uint32_t reserved_sectors = 1;
    uint32_t fat_count = 2;
    uint32_t data_sectors = total_sectors - reserved_sectors - (fat_count * sectors_per_fat) - root_sectors;
    uint32_t clusters = data_sectors / 1; /* SPC = 1 */
    
    serial_print("[fs_detect] Total sectors: ");
    serial_print_dec(total_sectors);
    serial_print("\n");
    serial_print("[fs_detect] Clusters: ");
    serial_print_dec(clusters);
    serial_print("\n");
    
    /* ====================================================================
     * 1. Boot Sector
     * ==================================================================== */
    buffer = (uint8_t *)kmalloc(512);
    if (!buffer) return -1;
    memset(buffer, 0, 512);
    
    /* Jump code */
    buffer[0] = 0xEB;
    buffer[1] = 0x3C;
    buffer[2] = 0x90;
    memcpy(buffer + 3, "LETHALOS", 8);
    
    /* BPB */
    *(uint16_t *)(buffer + 11) = 512;          /* Bytes per sector */
    buffer[13] = 1;                            /* Sectors per cluster */
    *(uint16_t *)(buffer + 14) = reserved_sectors; /* Reserved sectors */
    buffer[16] = fat_count;                    /* Number of FATs */
    *(uint16_t *)(buffer + 17) = root_entries; /* Root entries */
    *(uint16_t *)(buffer + 19) = total_sectors & 0xFFFF; /* Total sectors (low) */
    buffer[21] = 0xF8;                         /* Media descriptor */
    *(uint16_t *)(buffer + 22) = sectors_per_fat; /* Sectors per FAT */
    *(uint16_t *)(buffer + 24) = 32;           /* Sectors per track */
    *(uint16_t *)(buffer + 26) = 64;           /* Number of heads */
    *(uint32_t *)(buffer + 28) = 0;            /* Hidden sectors */
    *(uint32_t *)(buffer + 32) = total_sectors; /* Total sectors (32-bit) */
    
    buffer[36] = 0x80;                         /* Drive number */
    buffer[38] = 0x29;                         /* Extended boot signature */
    *(uint32_t *)(buffer + 39) = 0x4C4B3130;   /* Volume ID (LK10) */
    memcpy(buffer + 43, "LETHAL OS  ", 11);    /* Volume label */
    memcpy(buffer + 54, "FAT16   ", 8);        /* Filesystem type */
    buffer[510] = 0x55;                        /* Boot signature */
    buffer[511] = 0xAA;
    
    block_device_write(dev->id, 0, buffer);
    kfree(buffer);
    
    /* ====================================================================
     * 2. FAT (File Allocation Table)
     * ==================================================================== */
    buffer = (uint8_t *)kmalloc(512);
    if (!buffer) return -1;
    memset(buffer, 0, 512);
    
    /* FAT[0] = 0xFFF8 (media descriptor) */
    buffer[0] = 0xF8;
    buffer[1] = 0xFF;
    
    /* FAT[1] = 0xFFFF (end of cluster chain marker) */
    buffer[2] = 0xFF;
    buffer[3] = 0xFF;
    
    /* Write both FAT copies */
    for (uint32_t i = 0; i < fat_count; i++) {
        block_device_write(dev->id, reserved_sectors + i * sectors_per_fat, buffer);
    }
    kfree(buffer);
    
    /* ====================================================================
     * 3. Root Directory (empty)
     * ==================================================================== */
    buffer = (uint8_t *)kmalloc(512);
    if (!buffer) return -1;
    memset(buffer, 0, 512);
    
    /* Write root directory */
    block_device_write(dev->id, reserved_sectors + fat_count * sectors_per_fat, buffer);
    kfree(buffer);
    
    serial_print("[fs_detect] Default FAT16 filesystem created\n");
    return 0;
}

/* Mount the detected filesystem */
int fs_detect_and_mount(uint32_t dev_id, uint32_t start_lba)
{
    int fs_type = fs_detect_partition(dev_id, start_lba, 0);
    
    serial_print("[fs_detect] Mounting ");
    serial_print(fs_type_to_string(fs_type));
    serial_print(" on device ");
    serial_print_dec(dev_id);
    serial_print(" at LBA ");
    serial_print_dec(start_lba);
    serial_print("\n");
    
    switch(fs_type) {
        case FS_TYPE_EXT2:
            return vfs_mount_ext2_at(dev_id, start_lba);
        case FS_TYPE_EXT3:
            return vfs_mount_ext3_at(dev_id, start_lba);
        case FS_TYPE_EXT4:
            return vfs_mount_ext4_at(dev_id, start_lba);
        case FS_TYPE_FAT16:
        case FS_TYPE_FAT32:
            return vfs_mount_fat16_at(dev_id, start_lba);
        default:
            serial_print("[fs_detect] Unknown filesystem\n");
            return -1;
    }
}

/* Mount the first bootable partition */
int fs_boot_mount(void)
{
    serial_print("[fs_detect] Looking for bootable filesystem...\n");
    
    /* Scan all block devices for partitions */
    for (uint32_t dev = 0; dev < (uint32_t)block_device_count(); dev++) {
        partition_scan(dev);
    }
    
    /* Try to mount first partition */
    if (partition_count() > 0) {
        struct partition *p = partition_get(0);
        if (p) {
            serial_print("[fs_detect] Trying to mount partition 0 (");
            serial_print(p->name);
            serial_print(")\n");
            
            int result = fs_detect_and_mount(p->dev_id, p->start_lba);
            if (result == 0) {
                serial_print("[fs_detect] Boot filesystem mounted from ");
                serial_print(p->name);
                serial_print("\n");
                return 0;
            }
        }
    }
    
    /* If no partitions, try whole device */
    serial_print("[fs_detect] No partitions found, trying whole device 0\n");
    int result = fs_detect_and_mount(0, 0);
    
    /* If mount fails, create default filesystem */
    if (result != 0) {
        struct block_device *dev = block_device_get(0);
        if (dev) {
            serial_print("[fs_detect] No filesystem found, creating default...\n");
            fs_create_default(dev);
            /* Try to mount again */
            result = fs_detect_and_mount(0, 0);
            if (result == 0) {
                serial_print("[fs_detect] Boot filesystem created and mounted\n");
            }
        }
    }
    
    return result;
}