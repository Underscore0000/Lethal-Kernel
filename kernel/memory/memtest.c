/*
 * Lethal Kernel - Memory manager stress tests
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "memtest.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "../core/console.h"

/* Max physical address we can safely touch: 128 MiB identity-mapped. */
#define SAFE_PHYS_LIMIT  (128ULL * 1024ULL * 1024ULL)

#define PMM_TEST_FRAMES  10000u
#define VMM_TEST_PAGES   1000u
#define VMM_TEST_BASE    0x0000000050000000ULL
#define HEAP_TEST_ALLOCS 10000u
#define HEAP_TEST_MAXSZ  4096u

static uint32_t rng_state = 0x12345678u;

static uint32_t rng_next(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

/* ------------------------------------------------------------------ */
/* PMM stress test                                                    */
/* ------------------------------------------------------------------ */

int pmm_stress_test(void)
{
    uint64_t *frames = (uint64_t *)kmalloc(
        sizeof(uint64_t) * PMM_TEST_FRAMES);
    if (!frames) {
        console_print("[memtest] PMM: cannot allocate frame list\n");
        return 0;
    }

    serial_print("[memtest] PMM: allocating up to ");
    serial_print_dec(PMM_TEST_FRAMES);
    serial_print(" frames...\n");

    uint64_t alloc_before = pmm_used_bytes();
    uint32_t allocated = 0;

    for (uint32_t i = 0; i < PMM_TEST_FRAMES; i++) {
        uint64_t f = pmm_alloc_frame();
        if (!f) break;
        if (f >= SAFE_PHYS_LIMIT) {
            /* Cannot touch it safely; give it back and stop. */
            pmm_free_frame(f);
            break;
        }
        frames[allocated++] = f;

        volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)f;
        p[0] = 0xDEADBEEFu + i;
        p[1023] = 0xCAFEBABEu + i;
    }

    serial_print("[memtest] PMM: allocated ");
    serial_print_dec(allocated);
    serial_print(" frames\n");

    int ok = 1;

    for (uint32_t i = 0; i < allocated; i++) {
        uint64_t f = frames[i];
        volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)f;
        if (p[0] != 0xDEADBEEFu + i || p[1023] != 0xCAFEBABEu + i) {
            serial_print("[memtest] PMM: data mismatch at ");
            serial_print_dec(i);
            serial_print("\n");
            ok = 0;
            break;
        }
    }

    for (uint32_t i = 0; i < allocated; i++)
        pmm_free_frame(frames[i]);

    uint64_t alloc_after = pmm_used_bytes();
    if (alloc_after != alloc_before) {
        serial_print("[memtest] PMM: leak (before=");
        serial_print_dec(alloc_before);
        serial_print(" after=");
        serial_print_dec(alloc_after);
        serial_print(")\n");
        ok = 0;
    }

    for (uint32_t i = 0; i < allocated; i++) {
        uint64_t f = pmm_alloc_frame();
        if (!f) { ok = 0; break; }
        if (f >= SAFE_PHYS_LIMIT) {
            pmm_free_frame(f);
            ok = 0;
            break;
        }
        volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)f;
        p[0] = 0xFEEDFACEu;
        if (p[0] != 0xFEEDFACEu) { ok = 0; pmm_free_frame(f); break; }
        pmm_free_frame(f);
    }

    kfree(frames);

    serial_print("[memtest] PMM: ");
    serial_print(ok ? "PASS\n" : "FAIL\n");
    return ok;
}

/* ------------------------------------------------------------------ */
/* VMM stress test                                                    */
/* ------------------------------------------------------------------ */

int vmm_stress_test(void)
{
    struct vm_space *space = vmm_create_user_space();
    if (!space) {
        console_print("[memtest] VMM: cannot create user space\n");
        return 0;
    }

    serial_print("[memtest] VMM: mapping up to ");
    serial_print_dec(VMM_TEST_PAGES);
    serial_print(" pages...\n");

    uint32_t mapped = 0;
    for (uint32_t i = 0; i < VMM_TEST_PAGES; i++) {
        uint64_t va = VMM_TEST_BASE + i * LETHAL_VMM_PAGE_SIZE;
        uint64_t pa = pmm_alloc_frame();
        if (!pa) break;
        if (pa >= SAFE_PHYS_LIMIT) {
            pmm_free_frame(pa);
            break;
        }
        if (vmm_map_page_in(space, va, pa,
                            PTE_PRESENT | PTE_WRITE | PTE_USER) != 0) {
            pmm_free_frame(pa);
            break;
        }
        mapped++;

        volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)pa;
        p[0] = 0xB0000000u + i;
        p[1023] = 0xE0000000u + i;
    }

    serial_print("[memtest] VMM: mapped ");
    serial_print_dec(mapped);
    serial_print(" pages\n");

    int ok = 1;
    for (uint32_t i = 0; i < mapped; i++) {
        uint64_t va = VMM_TEST_BASE + i * LETHAL_VMM_PAGE_SIZE;
        uint64_t pa = vmm_get_phys_in(space, va);
        if (!pa) { ok = 0; break; }
        volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)pa;
        if (p[0] != 0xB0000000u + i || p[1023] != 0xE0000000u + i) {
            serial_print("[memtest] VMM: data mismatch at ");
            serial_print_dec(i);
            serial_print("\n");
            ok = 0;
            break;
        }
    }

    for (uint32_t i = 0; i < mapped; i++) {
        uint64_t va = VMM_TEST_BASE + i * LETHAL_VMM_PAGE_SIZE;
        vmm_unmap_page_in(space, va, 1);
    }

    vmm_destroy_user_space(space);

    serial_print("[memtest] VMM: ");
    serial_print(ok ? "PASS\n" : "FAIL\n");
    return ok;
}

/* ------------------------------------------------------------------ */
/* Heap stress test                                                   */
/* ------------------------------------------------------------------ */

struct heap_rec {
    void    *ptr;
    uint32_t size;
    uint32_t pattern;
};

int heap_stress_test(void)
{
    struct heap_rec *recs = (struct heap_rec *)kmalloc(
        sizeof(struct heap_rec) * HEAP_TEST_ALLOCS);
    if (!recs) {
        console_print("[memtest] Heap: cannot allocate record list\n");
        return 0;
    }

    serial_print("[memtest] Heap: allocating up to ");
    serial_print_dec(HEAP_TEST_ALLOCS);
    serial_print(" blocks...\n");

    uint64_t used_before = heap_used_bytes();
    uint32_t allocated = 0;

    for (uint32_t i = 0; i < HEAP_TEST_ALLOCS; i++) {
        uint32_t sz = (rng_next() % HEAP_TEST_MAXSZ) + 1;
        void *p = kmalloc(sz);
        if (!p) break;

        uint32_t pat = 0xC0DE0000u + i;
        uint8_t *b = (uint8_t *)p;
        for (uint32_t k = 0; k < sz; k++)
            b[k] = (uint8_t)(pat + k);

        recs[allocated].ptr     = p;
        recs[allocated].size    = sz;
        recs[allocated].pattern = pat;
        allocated++;
    }

    serial_print("[memtest] Heap: allocated ");
    serial_print_dec(allocated);
    serial_print(" blocks\n");

    int ok = 1;
    for (uint32_t i = 0; i < allocated; i++) {
        uint8_t *b = (uint8_t *)recs[i].ptr;
        uint32_t pat = recs[i].pattern;
        for (uint32_t k = 0; k < recs[i].size; k++) {
            if (b[k] != (uint8_t)(pat + k)) {
                serial_print("[memtest] Heap: data mismatch at ");
                serial_print_dec(i);
                serial_print("\n");
                ok = 0;
                break;
            }
        }
        if (!ok) break;
    }

    for (uint32_t i = 0; i < allocated; i++)
        kfree(recs[i].ptr);

    uint64_t used_after = heap_used_bytes();
    if (used_after != used_before) {
        serial_print("[memtest] Heap: leak (before=");
        serial_print_dec(used_before);
        serial_print(" after=");
        serial_print_dec(used_after);
        serial_print(")\n");
        ok = 0;
    }

    for (uint32_t i = 0; i < 1000; i++) {
        uint32_t sz = (rng_next() % HEAP_TEST_MAXSZ) + 1;
        void *p = kmalloc(sz);
        if (!p) { ok = 0; break; }
        kfree(p);
    }

    kfree(recs);

    serial_print("[memtest] Heap: ");
    serial_print(ok ? "PASS\n" : "FAIL\n");
    return ok;
}

/* ------------------------------------------------------------------ */
/* Run all                                                            */
/* ------------------------------------------------------------------ */

int memtest_run_all(void)
{
    console_separator();
    console_info("Memory Manager Stress Tests");

    int p = pmm_stress_test();
    int v = vmm_stress_test();
    int h = heap_stress_test();

    console_separator();
    console_print("[memtest] Results: PMM=");
    console_print(p ? "PASS" : "FAIL");
    console_print(" VMM=");
    console_print(v ? "PASS" : "FAIL");
    console_print(" Heap=");
    console_print(h ? "PASS" : "FAIL");
    console_print("\n");
    console_separator();

    return (p && v && h) ? 1 : 0;
}