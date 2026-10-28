/*
 * Lethal Kernel - Virtual Memory Manager
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef LETHAL_KERNEL_VMM_H
#define LETHAL_KERNEL_VMM_H

#include <stdint.h>
#include <stddef.h>

/*
 * Use a uniquely named page-size macro.
 *
 * Do NOT use PAGE_SIZE here: heap.c and other kernel components
 * may already define it.
 */
#define LETHAL_VMM_PAGE_SIZE 4096ULL

#define LETHAL_VMM_ENTRIES 512ULL

#define USER_MIN 0x0000000040000000ULL
#define USER_MAX 0x000000007FFFFFFFFFULL

#define KERNEL_IDENTITY_LIMIT 0x0000000100000000ULL

/*
 * x86-64 page-table flags.
 */
#define PTE_PRESENT   (1ULL << 0)
#define PTE_WRITE     (1ULL << 1)
#define PTE_USER      (1ULL << 2)
#define PTE_PWT       (1ULL << 3)
#define PTE_PCD       (1ULL << 4)
#define PTE_ACCESSED  (1ULL << 5)
#define PTE_DIRTY     (1ULL << 6)
#define PTE_PAT       (1ULL << 7)
#define PTE_GLOBAL    (1ULL << 8)

/*
 * Software-defined bit.
 */
#define PTE_COW       (1ULL << 9)

/*
 * No-execute.
 */
#define PTE_NX        (1ULL << 63)

struct vm_space {
    uint64_t pml4_phys;

    uint64_t brk;
    uint64_t brk_max;
    uint64_t mmap_next;
};

extern uint64_t kernel_pml4_phys;

/*
 * Initialization.
 */
void vmm_init(uint64_t highest_phys_addr);

/*
 * Kernel/MMIO mappings.
 *
 * Maps:
 *
 *     physical [phys_addr, phys_addr + length)
 *
 * into the same virtual address range.
 *
 * This is intentionally a two-argument API because the e1000 driver
 * calls:
 *
 *     vmm_map_mmio(bar, 0x10000);
 */
int vmm_map_mmio(uint64_t phys_addr, uint64_t length);

void vmm_map_apic(void);

/*
 * Address-space access.
 */
struct vm_space *vmm_kernel_space(void);
struct vm_space *vmm_current_space(void);

void vmm_activate_space(struct vm_space *space);

/*
 * User address spaces.
 */
struct vm_space *vmm_create_user_space(void);
struct vm_space *vmm_clone_cow(struct vm_space *parent);
void vmm_destroy_user_space(struct vm_space *space);

/*
 * Mapping.
 */
int vmm_map_page(
    uint64_t virt_addr,
    uint64_t phys_addr,
    uint64_t flags
);

int vmm_map_page_in(
    struct vm_space *space,
    uint64_t virt_addr,
    uint64_t phys_addr,
    uint64_t flags
);

/*
 * Unmapping.
 */
void vmm_unmap_page(uint64_t virt_addr);

void vmm_unmap_page_in(
    struct vm_space *space,
    uint64_t virt_addr,
    int release_frame
);

/*
 * Translation.
 */
uint64_t vmm_get_phys(uint64_t virt_addr);

uint64_t vmm_get_phys_in(
    struct vm_space *space,
    uint64_t virt_addr
);

/*
 * Page flags.
 */
uint64_t vmm_get_flags(uint64_t virt_addr);

uint64_t vmm_get_flags_in(
    struct vm_space *space,
    uint64_t virt_addr
);

/*
 * User pointer validation.
 */
int vmm_user_range_valid(
    uint64_t start,
    uint64_t length,
    int write
);

int vmm_user_range_valid_in(
    struct vm_space *space,
    uint64_t start,
    uint64_t length,
    int write
);

/*
 * Copy-on-write page fault handler.
 *
 * Returns:
 *
 *     1 = fault handled
 *     0 = not a COW fault / unable to handle
 */
int vmm_handle_cow_fault(
    struct vm_space *space,
    uint64_t fault_addr,
    uint64_t error_code
);

/*
 * Anonymous user memory.
 */
int vmm_space_map_anonymous(
    struct vm_space *space,
    uint64_t start,
    uint64_t length,
    uint64_t flags,
    int zero
);

#endif /* LETHAL_KERNEL_VMM_H */