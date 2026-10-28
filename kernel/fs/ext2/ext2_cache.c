/*
 * Lethal Kernel - EXT2 Buffer Cache
 *
 * v2.1: In-memory cache for disk blocks.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext2.h"
#include "../../memory/heap.h"
#include "../../core/mutex.h"
#include <string.h>

#define CACHE_SIZE 64
#define CACHE_DIRTY 0x01
#define CACHE_VALID 0x02

struct cache_entry {
    uint32_t block;
    uint32_t dev_id;
    uint8_t *data;
    uint32_t flags;
    uint32_t refcount;
    uint32_t access_time;
};

static struct cache_entry cache[CACHE_SIZE];
static mutex_t cache_lock = MUTEX_INIT;

/* Initialize the cache */
void ext2_cache_init(void)
{
    mutex_init(&cache_lock);
    for (int i = 0; i < CACHE_SIZE; i++) {
        cache[i].block = 0;
        cache[i].dev_id = 0;
        cache[i].data = NULL;
        cache[i].flags = 0;
        cache[i].refcount = 0;
        cache[i].access_time = 0;
    }
}

/* Find a block in the cache */
static struct cache_entry *ext2_cache_find(uint32_t dev_id, uint32_t block)
{
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].dev_id == dev_id && cache[i].block == block && 
            (cache[i].flags & CACHE_VALID)) {
            return &cache[i];
        }
    }
    return NULL;
}

/* Find a free cache entry */
static struct cache_entry *ext2_cache_find_free(void)
{
    uint32_t oldest_time = 0xFFFFFFFF;
    int oldest_idx = 0;
    
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].refcount == 0 && !(cache[i].flags & CACHE_DIRTY)) {
            if (cache[i].access_time < oldest_time) {
                oldest_time = cache[i].access_time;
                oldest_idx = i;
            }
        }
    }
    
    /* Flush if dirty */
    if (cache[oldest_idx].flags & CACHE_DIRTY) {
        /* Write back dirty block */
        /* ext2_write_block would be called here */
        cache[oldest_idx].flags &= ~CACHE_DIRTY;
    }
    
    if (cache[oldest_idx].data) {
        kfree(cache[oldest_idx].data);
        cache[oldest_idx].data = NULL;
    }
    
    return &cache[oldest_idx];
}

/* Read a block with caching */
int ext2_cache_read(struct ext2_filesystem *fs, uint32_t block, void *buffer)
{
    mutex_lock(&cache_lock);
    
    struct cache_entry *entry = ext2_cache_find(fs->dev_id, block);
    
    if (entry && (entry->flags & CACHE_VALID)) {
        /* Cache hit */
        memcpy(buffer, entry->data, fs->block_size);
        entry->access_time++;
        entry->refcount++;
        mutex_unlock(&cache_lock);
        return 0;
    }
    
    /* Cache miss - read from disk */
    entry = ext2_cache_find_free();
    if (!entry) {
        mutex_unlock(&cache_lock);
        return -1;
    }
    
    if (!entry->data) {
        entry->data = (uint8_t *)kmalloc(fs->block_size);
        if (!entry->data) {
            mutex_unlock(&cache_lock);
            return -1;
        }
    }
    
    /* Read from disk */
    if (ext2_read_block(fs, block, entry->data) != 0) {
        mutex_unlock(&cache_lock);
        return -1;
    }
    
    entry->dev_id = fs->dev_id;
    entry->block = block;
    entry->flags = CACHE_VALID;
    entry->refcount = 1;
    entry->access_time = 0;
    
    memcpy(buffer, entry->data, fs->block_size);
    mutex_unlock(&cache_lock);
    return 0;
}

/* Write a block with caching */
int ext2_cache_write(struct ext2_filesystem *fs, uint32_t block, void *buffer)
{
    mutex_lock(&cache_lock);
    
    struct cache_entry *entry = ext2_cache_find(fs->dev_id, block);
    
    if (entry && (entry->flags & CACHE_VALID)) {
        /* Update cache */
        memcpy(entry->data, buffer, fs->block_size);
        entry->flags |= CACHE_DIRTY;
        entry->access_time++;
        mutex_unlock(&cache_lock);
        return 0;
    }
    
    /* Write through if not in cache */
    int result = ext2_write_block(fs, block, buffer);
    mutex_unlock(&cache_lock);
    return result;
}

/* Flush all dirty blocks */
void ext2_cache_flush(struct ext2_filesystem *fs)
{
    mutex_lock(&cache_lock);
    for (int i = 0; i < CACHE_SIZE; i++) {
        if ((cache[i].flags & CACHE_DIRTY) && (cache[i].flags & CACHE_VALID)) {
            if (cache[i].dev_id == fs->dev_id) {
                ext2_write_block(fs, cache[i].block, cache[i].data);
                cache[i].flags &= ~CACHE_DIRTY;
            }
        }
    }
    mutex_unlock(&cache_lock);
}

/* Invalidate cache for a device */
void ext2_cache_invalidate(uint32_t dev_id)
{
    mutex_lock(&cache_lock);
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].dev_id == dev_id) {
            cache[i].flags &= ~CACHE_VALID;
            if (cache[i].data) {
                kfree(cache[i].data);
                cache[i].data = NULL;
            }
        }
    }
    mutex_unlock(&cache_lock);
}