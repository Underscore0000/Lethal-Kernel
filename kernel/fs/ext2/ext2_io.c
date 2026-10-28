/*
 * Lethal Kernel - EXT2 Block I/O Scheduler
 *
 * v2.1: Simple I/O scheduler for block operations.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "ext2.h"
#include "../../drivers/storage/ata.h"
#include "../../memory/heap.h"
#include "../../core/mutex.h"
#include <string.h>

#define IO_QUEUE_SIZE 32

struct io_request {
    uint32_t dev_id;
    uint32_t lba;
    uint32_t count;
    uint8_t *buffer;
    int write;
    void (*callback)(void *);
    void *data;
};

static struct io_request io_queue[IO_QUEUE_SIZE];
static uint32_t io_queue_head = 0;
static uint32_t io_queue_tail = 0;
static mutex_t io_lock = MUTEX_INIT;

/* Initialize I/O scheduler */
void ext2_io_init(void)
{
    mutex_init(&io_lock);
    memset(io_queue, 0, sizeof(io_queue));
    io_queue_head = 0;
    io_queue_tail = 0;
}

/* Submit an I/O request */
int ext2_io_submit(uint32_t dev_id, uint32_t lba, uint32_t count,
                    uint8_t *buffer, int write)
{
    mutex_lock(&io_lock);
    
    uint32_t next_tail = (io_queue_tail + 1) % IO_QUEUE_SIZE;
    if (next_tail == io_queue_head) {
        mutex_unlock(&io_lock);
        return -1; /* Queue full */
    }
    
    struct io_request *req = &io_queue[io_queue_tail];
    req->dev_id = dev_id;
    req->lba = lba;
    req->count = count;
    req->buffer = buffer;
    req->write = write;
    req->callback = NULL;
    req->data = NULL;
    
    io_queue_tail = next_tail;
    mutex_unlock(&io_lock);
    return 0;
}

/* Process I/O requests - simple elevator algorithm */
int ext2_io_process(void)
{
    mutex_lock(&io_lock);
    
    if (io_queue_head == io_queue_tail) {
        mutex_unlock(&io_lock);
        return 0; /* No requests */
    }
    
    /* Simple FIFO with sorting (elevator) */
    /* For now, just process in order */
    struct io_request *req = &io_queue[io_queue_head];
    
    if (req->write) {
        ata_write28(req->dev_id, req->lba, req->buffer);
    } else {
        ata_read28(req->dev_id, req->lba, req->buffer);
    }
    
    io_queue_head = (io_queue_head + 1) % IO_QUEUE_SIZE;
    mutex_unlock(&io_lock);
    return 1;
}

/* Process all pending I/O */
void ext2_io_sync(void)
{
    while (io_queue_head != io_queue_tail) {
        ext2_io_process();
    }
}

/* Scatter-gather I/O for optimization */
int ext2_io_readv(uint32_t dev_id, uint32_t lba, uint32_t count,
                   uint8_t **buffers, uint32_t *lengths, uint32_t num_bufs)
{
    (void)count; /* Silenzia il warning - count non usato */
    
    uint32_t total_blocks = 0;
    for (uint32_t i = 0; i < num_bufs; i++) {
        total_blocks += (lengths[i] + 511) / 512;
    }
    
    uint8_t *temp_buffer = (uint8_t *)kmalloc(total_blocks * 512);
    if (!temp_buffer) return -1;
    
    if (ext2_io_submit(dev_id, lba, total_blocks, temp_buffer, 0) != 0) {
        kfree(temp_buffer);
        return -1;
    }
    
    ext2_io_sync();
    
    /* Distribute data to buffers */
    uint32_t offset = 0;
    for (uint32_t i = 0; i < num_bufs; i++) {
        memcpy(buffers[i], temp_buffer + offset, lengths[i]);
        offset += lengths[i];
    }
    
    kfree(temp_buffer);
    return 0;
}