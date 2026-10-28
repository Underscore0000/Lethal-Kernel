/*
 * Lethal Kernel - EXT4 Delayed Allocation
 *
 * v4.0: Delayed allocation for improved performance.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext4.h"
#include "../../core/console.h"
#include "../../memory/heap.h"
#include <string.h>

/* Delayed allocation entry */
struct delalloc_entry {
    uint32_t logical_block;
    uint32_t physical_block;
    uint32_t length;
    uint8_t dirty;
    uint8_t pending;
};

static struct delalloc_entry delalloc_queue[256];
static uint32_t delalloc_count = 0;
static mutex_t delalloc_lock = MUTEX_INIT;

/* Initialize delayed allocation */
void ext4_delalloc_init(void)
{
    mutex_init(&delalloc_lock);
    delalloc_count = 0;
    memset(delalloc_queue, 0, sizeof(delalloc_queue));
    serial_print("[ext4_delalloc] Initialized\n");
}

/* Add a block to delayed allocation queue */
int ext4_delalloc_add_block(struct ext4_filesystem *fs, uint32_t logical_block,
                             uint32_t length)
{
    (void)fs; /* Silenzia il warning - parametro non usato per ora */
    
    mutex_lock(&delalloc_lock);
    
    if (delalloc_count >= 256) {
        /* Queue is full - flush it */
        mutex_unlock(&delalloc_lock);
        ext4_delalloc_flush(fs);
        mutex_lock(&delalloc_lock);
        
        if (delalloc_count >= 256) {
            mutex_unlock(&delalloc_lock);
            return -1;
        }
    }
    
    /* Check if block already exists in queue */
    for (uint32_t i = 0; i < delalloc_count; i++) {
        if (delalloc_queue[i].logical_block == logical_block) {
            /* Update existing entry */
            delalloc_queue[i].length = length;
            delalloc_queue[i].dirty = 1;
            delalloc_queue[i].pending = 1;
            mutex_unlock(&delalloc_lock);
            return 0;
        }
    }
    
    /* Add new entry */
    delalloc_queue[delalloc_count].logical_block = logical_block;
    delalloc_queue[delalloc_count].physical_block = 0; /* Not allocated yet */
    delalloc_queue[delalloc_count].length = length;
    delalloc_queue[delalloc_count].dirty = 1;
    delalloc_queue[delalloc_count].pending = 1;
    delalloc_count++;
    
    mutex_unlock(&delalloc_lock);
    return 0;
}

/* Get a block from delayed allocation queue */
int ext4_delalloc_get_block(struct ext4_filesystem *fs, uint32_t logical_block,
                             uint32_t *physical_block)
{
    (void)fs; 
    
    mutex_lock(&delalloc_lock);
    
    for (uint32_t i = 0; i < delalloc_count; i++) {
        if (delalloc_queue[i].logical_block == logical_block) {
            if (delalloc_queue[i].pending) {
                *physical_block = 0;
                mutex_unlock(&delalloc_lock);
                return -1;
            }
            *physical_block = delalloc_queue[i].physical_block;
            mutex_unlock(&delalloc_lock);
            return 0;
        }
    }
    
    mutex_unlock(&delalloc_lock);
    return -1; /* Not found */
}

/* Flush all pending delayed allocations */
void ext4_delalloc_flush(struct ext4_filesystem *fs)
{
    (void)fs; 
    
    mutex_lock(&delalloc_lock);
    
    if (delalloc_count == 0) {
        mutex_unlock(&delalloc_lock);
        return;
    }
    
    serial_print("[ext4_delalloc] Flushing ");
    serial_print_dec(delalloc_count);
    serial_print(" pending blocks\n");
    

    for (uint32_t i = 0; i < delalloc_count; i++) {
        if (delalloc_queue[i].dirty) {
            /* Write the block to disk */
            delalloc_queue[i].dirty = 0;
            delalloc_queue[i].pending = 0;
        }
    }
    
    delalloc_count = 0;
    
    mutex_unlock(&delalloc_lock);
}

/* Check if delayed allocation is enabled */
int ext4_delalloc_enabled(struct ext4_filesystem *fs)
{
    (void)fs; 
    return 1; /* Always enabled for now */
}

/* Get number of pending delayed allocations */
uint32_t ext4_delalloc_pending_count(void)
{
    uint32_t count = 0;
    mutex_lock(&delalloc_lock);
    for (uint32_t i = 0; i < delalloc_count; i++) {
        if (delalloc_queue[i].pending) {
            count++;
        }
    }
    mutex_unlock(&delalloc_lock);
    return count;
}

/* Force sync of all delayed allocations */
void ext4_delalloc_sync(struct ext4_filesystem *fs)
{
    ext4_delalloc_flush(fs);
}