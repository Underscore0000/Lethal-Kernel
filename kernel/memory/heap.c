/*
 * Lethal Kernel - kernel heap (kmalloc/kfree)
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "heap.h"
#include "vmm.h"
#include "pmm.h"
#include "../core/console.h"
#include "../core/panic.h"

#define PAGE_SIZE 4096
#define HEAP_MAGIC 0x4C4B484Du /* 'LKHM' */

struct block_header {
    uint32_t magic;
    uint32_t is_free;
    size_t   size;
    struct block_header *next;
    struct block_header *prev;
};

#define HEADER_SIZE (sizeof(struct block_header))
#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

static uint64_t heap_start;
static uint64_t heap_end;
static uint64_t heap_max;
static struct block_header *free_list;
static uint64_t used_bytes;
static int corrupt_reports = 0;

static int block_ok(const struct block_header *b)
{
    if (b->magic != HEAP_MAGIC) return 0;
    uint64_t end = (uint64_t)(uintptr_t)b + HEADER_SIZE + b->size;
    if (b->next) return end == (uint64_t)(uintptr_t)b->next;
    return end == heap_end;
}

static void report_corrupt(const struct block_header *b)
{
    if (corrupt_reports >= 8) return;
    corrupt_reports++;
    serial_print("[heap] CORRUPT block=");
    serial_print_hex((uint64_t)(uintptr_t)b);
    serial_print("\n");
}

static int heap_grow(size_t min_extra)
{
    uint64_t needed = ALIGN_UP(min_extra, PAGE_SIZE);
    uint64_t new_end = heap_end + needed;

    if (new_end > heap_max) {
        return -1;
    }

    for (uint64_t addr = heap_end; addr < new_end; addr += PAGE_SIZE) {
        uint64_t frame = pmm_alloc_frame();
        if (frame == 0) {
            return -1;
        }
        if (vmm_map_page(addr, frame, PTE_WRITE) != 0) {
            pmm_free_frame(frame);
            return -1;
        }
    }

    struct block_header *new_block = (struct block_header *)(uintptr_t)heap_end;
    new_block->magic = HEAP_MAGIC;
    new_block->is_free = 1;
    new_block->size = needed - HEADER_SIZE;
    new_block->next = NULL;
    new_block->prev = NULL;

    if (free_list == NULL) {
        free_list = new_block;
    } else {
        struct block_header *tail = free_list;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = new_block;
        new_block->prev = tail;
    }

    heap_end = new_end;
    return 0;
}

void heap_init(uint64_t virt_start, uint64_t max_size)
{
    heap_start = virt_start;
    heap_end = virt_start;
    heap_max = virt_start + max_size;
    free_list = NULL;
    used_bytes = 0;
    corrupt_reports = 0;

    if (heap_grow(PAGE_SIZE) != 0) {
        serial_print("[heap] WARNING: failed to map initial heap page\n");
    }

    serial_print("[heap] initialized at ");
    serial_print_hex(heap_start);
    serial_print(", max size ");
    serial_print_dec(max_size / 1024);
    serial_print(" KiB\n");
}

static void split_block(struct block_header *block, size_t size)
{
    size_t remaining = block->size - size;
    if (remaining <= HEADER_SIZE + 16) {
        return;
    }

    struct block_header *new_block =
        (struct block_header *)((uint8_t *)block + HEADER_SIZE + size);
    new_block->magic = HEAP_MAGIC;
    new_block->is_free = 1;
    new_block->size = remaining - HEADER_SIZE;
    new_block->next = block->next;
    new_block->prev = block;

    if (block->next) {
        block->next->prev = new_block;
    }
    block->next = new_block;
    block->size = size;
}

void *kmalloc(size_t size)
{
    if (size == 0) return NULL;
    size = ALIGN_UP(size, 16);

    struct block_header *block = free_list;
    while (block) {
        if (block->magic != HEAP_MAGIC) {
            report_corrupt(block);
            break;
        }
        if (block->is_free && block->size >= size) {
            if (!block_ok(block)) {
                report_corrupt(block);
                block = block->next;
                continue;
            }
            split_block(block, size);
            block->is_free = 0;
            used_bytes += block->size;
            return (void *)((uint8_t *)block + HEADER_SIZE);
        }
        block = block->next;
    }

    if (heap_grow(size + HEADER_SIZE) != 0) {
        return NULL;
    }
    return kmalloc(size);
}

static void coalesce(struct block_header *block)
{
    if (block->next && block->next->is_free) {
        block->size += HEADER_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    if (block->prev && block->prev->is_free) {
        coalesce(block->prev);
    }
}

void kfree(void *ptr)
{
    if (ptr == NULL) return;
    struct block_header *block =
        (struct block_header *)((uint8_t *)ptr - HEADER_SIZE);

    if (block->magic != HEAP_MAGIC || block->is_free) {
        panic("kfree(): invalid or double-free pointer");
        return;
    }

    used_bytes -= block->size;
    block->is_free = 1;
    coalesce(block);
}

void *kcalloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    if (size != 0 && total / size != nmemb) return NULL; /* overflow */
    void *ptr = kmalloc(total);
    if (ptr) {
        uint8_t *p = (uint8_t *)ptr;
        for (size_t i = 0; i < total; i++) p[i] = 0;
    }
    return ptr;
}

void *krealloc(void *ptr, size_t size)
{
    if (ptr == NULL) return kmalloc(size);
    if (size == 0) { kfree(ptr); return NULL; }

    struct block_header *block =
        (struct block_header *)((uint8_t *)ptr - HEADER_SIZE);
    if (block->size >= size) return ptr;

    void *new_ptr = kmalloc(size);
    if (!new_ptr) return NULL;
    uint8_t *src = (uint8_t *)ptr;
    uint8_t *dst = (uint8_t *)new_ptr;
    for (size_t i = 0; i < block->size; i++) dst[i] = src[i];
    kfree(ptr);
    return new_ptr;
}

size_t heap_block_size(const void *p)
{
    if (!p) return 0;
    const struct block_header *b =
        (const struct block_header *)((const uint8_t *)p - HEADER_SIZE);
    if (b->magic != HEAP_MAGIC || b->is_free) return 0;
    return b->size;
}

void *heap_find_used_block(size_t min_size)
{
    void *best = NULL;
    size_t best_size = 0;
    uintptr_t addr = (uintptr_t)heap_start;

    while (addr < (uintptr_t)heap_end) {
        struct block_header *b = (struct block_header *)addr;
        if (b->magic != HEAP_MAGIC) break;

        if (!b->is_free && b->size >= min_size &&
            (best == NULL || b->size < best_size)) {
            best = (void *)((uint8_t *)b + HEADER_SIZE);
            best_size = b->size;
        }

        addr += HEADER_SIZE + b->size;
    }

    return best;
}

uint64_t heap_used_bytes(void) { return used_bytes; }
uint64_t heap_total_bytes(void) { return heap_end - heap_start; }