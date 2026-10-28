/*
 * Lethal Kernel - Block Device Layer Implementation
 *
 * v4.0: Generic block device interface.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "block.h"
#include "../storage/ata.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>

static struct block_device devices[BLOCK_DEVICE_MAX];
static uint32_t device_count = 0;

static struct partition partitions[PARTITION_MAX];
static uint32_t partition_count_static = 0;

/* Initialize block device layer */
void block_init(void)
{
    serial_print("[block] Initializing block devices...\n");
    
    memset(devices, 0, sizeof(devices));
    device_count = 0;
    partition_count_static = 0;
    
    /* Register ATA devices */
    uint32_t ata_count = ata_device_count();
    for (uint32_t i = 0; i < ata_count && i < BLOCK_DEVICE_MAX; i++) {
        struct block_device dev;
        memset(&dev, 0, sizeof(dev));
        dev.id = i;
        dev.type = BLOCK_TYPE_ATA;
        dev.sector_size = 512;
        dev.sector_count = ata_get_device(i)->sectors;
        dev.present = 1;
        /* Manual string construction without snprintf */
        const char *prefix = "sd";
        char *name = dev.name;
        while (*prefix) *name++ = *prefix++;
        *name++ = 'a' + i;
        *name = '\0';
        dev.read = ata_read28;
        dev.write = ata_write28;
        
        block_device_register(&dev);
    }
    
    serial_print("[block] Found ");
    serial_print_dec(device_count);
    serial_print(" block devices\n");
}

/* Register a block device */
int block_device_register(struct block_device *dev)
{
    if (!dev || device_count >= BLOCK_DEVICE_MAX) return -1;
    
    memcpy(&devices[device_count], dev, sizeof(struct block_device));
    device_count++;
    
    serial_print("[block] Registered device: ");
    serial_print(dev->name);
    serial_print(" (");
    serial_print_dec(dev->sector_count);
    serial_print(" sectors)\n");
    
    return 0;
}

/* Read from a block device */
int block_device_read(uint32_t dev_id, uint32_t lba, void *buffer)
{
    if (dev_id >= device_count || !devices[dev_id].present) return -1;
    if (!devices[dev_id].read) return -1;
    return devices[dev_id].read(dev_id, lba, buffer);
}

/* Write to a block device */
int block_device_write(uint32_t dev_id, uint32_t lba, const void *buffer)
{
    if (dev_id >= device_count || !devices[dev_id].present) return -1;
    if (!devices[dev_id].write) return -1;
    return devices[dev_id].write(dev_id, lba, buffer);
}

/* Get number of block devices */
int block_device_count(void)
{
    return device_count;
}

/* Get a block device by index */
struct block_device *block_device_get(uint32_t dev_id)
{
    if (dev_id >= device_count) return NULL;
    return &devices[dev_id];
}

/* Scan partitions on a device */
int partition_scan(uint32_t dev_id)
{
    if (dev_id >= device_count || !devices[dev_id].present) return -1;
    
    struct mbr mbr;
    if (block_device_read(dev_id, 0, &mbr) != 0) {
        serial_print("[block] Failed to read MBR from device ");
        serial_print_dec(dev_id);
        serial_print("\n");
        return -1;
    }
    
    if (mbr.signature != 0xAA55) {
        serial_print("[block] Invalid MBR signature on device ");
        serial_print_dec(dev_id);
        serial_print("\n");
        return -1;
    }
    
    serial_print("[block] Scanning partitions on device ");
    serial_print_dec(dev_id);
    serial_print("\n");
    
    uint32_t count = 0;
    for (int i = 0; i < 4; i++) {
        if (mbr.partitions[i].type != 0) {
            if (partition_count_static >= PARTITION_MAX) break;
            
            struct partition *p = &partitions[partition_count_static];
            p->dev_id = dev_id;
            p->partition_num = i + 1;
            p->start_lba = mbr.partitions[i].start_lba;
            p->sector_count = mbr.partitions[i].sector_count;
            p->type = mbr.partitions[i].type;
            p->bootable = mbr.partitions[i].bootable;
            /* Manual string construction */
            const char *prefix = devices[dev_id].name;
            char *name = p->name;
            while (*prefix) *name++ = *prefix++;
            *name++ = '0' + (i + 1);
            *name = '\0';
            
            serial_print("[block] Partition ");
            serial_print_dec(partition_count_static);
            serial_print(": ");
            serial_print(p->name);
            serial_print(" type=0x");
            serial_print_hex(p->type);
            serial_print(" start=");
            serial_print_dec(p->start_lba);
            serial_print(" size=");
            serial_print_dec(p->sector_count);
            serial_print(" sectors\n");
            
            partition_count_static++;
            count++;
        }
    }
    
    serial_print("[block] Found ");
    serial_print_dec(count);
    serial_print(" partitions\n");
    
    return count;
}

/* Get number of partitions */
int partition_count(void)
{
    return (int)partition_count_static;
}

/* Get a partition by index */
struct partition *partition_get(uint32_t index)
{
    if (index >= partition_count_static) return NULL;
    return &partitions[index];
}

/* Get partition device ID */
uint32_t partition_get_device(uint32_t index)
{
    if (index >= partition_count_static) return 0;
    return partitions[index].dev_id;
}

/* Get partition start LBA */
uint32_t partition_get_start_lba(uint32_t index)
{
    if (index >= partition_count_static) return 0;
    return partitions[index].start_lba;
}

/* Get partition sector count */
uint32_t partition_get_sector_count(uint32_t index)
{
    if (index >= partition_count_static) return 0;
    return partitions[index].sector_count;
}

/* Get partition type */
uint32_t partition_get_type(uint32_t index)
{
    if (index >= partition_count_static) return 0;
    return partitions[index].type;
}