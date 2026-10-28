/*
 * Lethal Kernel - Physical Memory Manager
 *
 * A frame-bitmap allocator (1 bit per 4 KiB physical frame). The bitmap
 * itself lives in a fixed static array sized for up to 4 GiB of physical
 * RAM (128 KiB of bitmap) - enough for v0.3's target of small/QEMU-style
 * VMs; a dynamically-sized bitmap allocated from a bump region is future
 * work once a heap exists.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "pmm.h"
#include "../core/console.h"

#define MAX_PHYS_BYTES   (4ULL * 1024 * 1024 * 1024) /* 4 GiB ceiling for v0.3 */
#define MAX_FRAMES       (MAX_PHYS_BYTES / PMM_FRAME_SIZE)
#define BITMAP_WORDS     (MAX_FRAMES / 64)

static uint64_t bitmap[BITMAP_WORDS];
static uint64_t highest_frame;     /* highest frame index we will ever hand out */
static uint64_t total_bytes_seen;  /* sum of all USABLE E820 region lengths   */
static uint64_t used_frames;
static uint32_t refcounts[MAX_FRAMES];

static inline void bitmap_set(uint64_t frame)
{
    bitmap[frame / 64] |= (1ULL << (frame % 64));
}

static inline void bitmap_clear(uint64_t frame)
{
    bitmap[frame / 64] &= ~(1ULL << (frame % 64));
}

static inline int bitmap_test(uint64_t frame)
{
    return (bitmap[frame / 64] >> (frame % 64)) & 1;
}

static void mark_range_used(uint64_t start, uint64_t end)
{
    uint64_t first_frame = start / PMM_FRAME_SIZE;
    uint64_t last_frame  = (end + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    if (last_frame > highest_frame) {
        last_frame = highest_frame;
    }
    for (uint64_t f = first_frame; f < last_frame; f++) {
        bitmap_set(f);
    }
}

static void mark_range_free(uint64_t start, uint64_t end)
{
    uint64_t first_frame = (start + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    uint64_t last_frame  = end / PMM_FRAME_SIZE;
    if (last_frame > highest_frame) {
        last_frame = highest_frame;
    }
    for (uint64_t f = first_frame; f < last_frame; f++) {
        bitmap_clear(f);
    }
}

void pmm_init(const struct e820_entry *map, uint16_t entry_count,
              uint64_t kernel_start, uint64_t kernel_end)
{
    /* Start fully "used": every frame is reserved until an E820 entry
     * explicitly proves it usable. This is safer than the opposite
     * default, since an unrecognized/garbage E820 entry should never
     * accidentally make reserved (e.g. MMIO) memory look allocatable. */
    for (uint64_t i = 0; i < BITMAP_WORDS; i++) {
        bitmap[i] = ~0ULL;
    }

    highest_frame = 0;
    total_bytes_seen = 0;
    used_frames = 0;
    for (uint64_t i = 0; i < MAX_FRAMES; i++) refcounts[i] = 0;

    if (entry_count == 0 || map == NULL) {
        /* No usable E820 data (very old/unusual BIOS). Fall back to a
         * conservative fixed amount so the kernel can still allocate a
         * few pages rather than nothing at all - documented as a known
         * v0.3 limitation, see README.md. */
        uint64_t fallback_bytes = 16ULL * 1024 * 1024; /* 16 MiB */
        highest_frame = fallback_bytes / PMM_FRAME_SIZE;
        mark_range_free(1 * 1024 * 1024, fallback_bytes);
        total_bytes_seen = fallback_bytes - (1 * 1024 * 1024);
        serial_print("[pmm] WARNING: no E820 map, using 16 MiB fallback\n");
    } else {
        /* First pass: find the highest usable address so we know how much
         * of the static bitmap actually applies. */
        uint64_t highest_addr = 0;
        for (uint16_t i = 0; i < entry_count; i++) {
            if (map[i].type != E820_TYPE_USABLE) {
                continue;
            }
            uint64_t end = map[i].base + map[i].length;
            if (end > highest_addr) {
                highest_addr = end;
            }
        }
        if (highest_addr > MAX_PHYS_BYTES) {
            highest_addr = MAX_PHYS_BYTES;
        }
        highest_frame = highest_addr / PMM_FRAME_SIZE;

        /* Second pass: free every usable region. */
        for (uint16_t i = 0; i < entry_count; i++) {
            if (map[i].type != E820_TYPE_USABLE) {
                continue;
            }
            uint64_t base = map[i].base;
            uint64_t end  = map[i].base + map[i].length;
            if (end > MAX_PHYS_BYTES) {
                end = MAX_PHYS_BYTES;
            }
            if (base >= end) {
                continue;
            }
            mark_range_free(base, end);
            total_bytes_seen += (end - base);
        }
    }

    /* Always reserve the first 1 MiB (real-mode IVT, BDA, video memory,
     * our bootloader stages) regardless of what E820 claims, plus the
     * kernel image itself, plus frame 0 (kept permanently reserved so a
     * returned physical address of 0 can safely mean "allocation
     * failed"). */
    mark_range_used(0, 1 * 1024 * 1024);
    mark_range_used(kernel_start, kernel_end);
    bitmap_set(0);

    used_frames = 0;
    for (uint64_t f = 0; f < highest_frame; f++) {
        if (bitmap_test(f)) {
            used_frames++;
            refcounts[f] = 1;
        }
    }

    serial_print("[pmm] initialized\n");
}

uint64_t pmm_alloc_frame(void)
{
    for (uint64_t f = 1; f < highest_frame; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            used_frames++;
            refcounts[f] = 1;
            return f * PMM_FRAME_SIZE;
        }
    }
    return 0; /* out of memory */
}

void pmm_ref_frame(uint64_t phys_addr)
{
    uint64_t frame = phys_addr / PMM_FRAME_SIZE;
    if (frame == 0 || frame >= highest_frame || !bitmap_test(frame)) return;
    if (refcounts[frame] != UINT32_MAX) refcounts[frame]++;
}

uint32_t pmm_frame_refs(uint64_t phys_addr)
{
    uint64_t frame = phys_addr / PMM_FRAME_SIZE;
    if (frame >= highest_frame) return 0;
    return refcounts[frame];
}

void pmm_free_frame(uint64_t phys_addr)
{
    uint64_t frame = phys_addr / PMM_FRAME_SIZE;
    if (frame == 0 || frame >= highest_frame) return;
    if (!bitmap_test(frame)) return;

    /* The function is now a release operation. Existing v0.7-v0.10 callers
     * still work because newly allocated frames start with refcount=1. */
    if (refcounts[frame] > 1) {
        refcounts[frame]--;
        return;
    }

    refcounts[frame] = 0;
    bitmap_clear(frame);
    used_frames--;
}

uint64_t pmm_total_bytes(void)
{
    return total_bytes_seen;
}

uint64_t pmm_used_bytes(void)
{
    return used_frames * (uint64_t)PMM_FRAME_SIZE;
}

uint64_t pmm_free_bytes(void)
{
    return (highest_frame * (uint64_t)PMM_FRAME_SIZE) - pmm_used_bytes();
}
