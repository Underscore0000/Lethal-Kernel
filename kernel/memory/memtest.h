/*
 * Lethal Kernel - Memory manager stress tests
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_MEMTEST_H
#define LETHAL_MEMTEST_H

#include <stdint.h>

/* PMM: alloc 100k frames, write pattern, free, realloc, verify. */
int pmm_stress_test(void);

/* VMM: map 10k user pages, write, verify phys, unmap, destroy space. */
int vmm_stress_test(void);

/* Heap: 100k allocs of random size, write pattern, free, realloc, verify. */
int heap_stress_test(void);

/* Run all three tests, print results, return 1 if all pass. */
int memtest_run_all(void);

#endif /* LETHAL_MEMTEST_H */