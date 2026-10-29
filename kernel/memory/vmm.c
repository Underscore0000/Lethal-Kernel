/*
 * Lethal Kernel - Virtual Memory Manager
 *
 * x86-64 4-level paging
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */

#include "vmm.h"
#include "pmm.h"
#include "../core/console.h"
#include "../core/panic.h"
#include "heap.h"

#include <stdint.h>
#include <stddef.h>

#define LETHAL_VMM_PAGE_SIZE 4096ULL

#define LETHAL_VMM_ENTRIES_PER_TABLE 512ULL

#define LETHAL_VMM_USER_MIN 0x0000000040000000ULL
#define LETHAL_VMM_USER_MAX 0x0000007FFFFFFFFFULL

#define LETHAL_VMM_KERNEL_IDENTITY_LIMIT 0x0000000100000000ULL

#define LETHAL_VMM_APIC_PHYS 0xFEE00000ULL
#define LETHAL_VMM_APIC_VIRT 0xFEE00000ULL

extern char _text_start[];
extern char _text_end[];
extern char _rodata_start[];
extern char _rodata_end[];
extern char _data_start[];
extern char _data_end[];
extern char __bss_start[];
extern char __bss_end[];

extern char kernel_stack_guard_start[];
extern char kernel_stack_guard_end[];

static uint64_t *kernel_pml4;
static struct vm_space kernel_space;
static struct vm_space *active_space;

uint64_t kernel_pml4_phys = 0;

static inline uint64_t vmm_idx4(uint64_t address)
{
    return (address >> 39) & 0x1FFULL;
}

static inline uint64_t vmm_idx3(uint64_t address)
{
    return (address >> 30) & 0x1FFULL;
}

static inline uint64_t vmm_idx2(uint64_t address)
{
    return (address >> 21) & 0x1FFULL;
}

static inline uint64_t vmm_idx1(uint64_t address)
{
    return (address >> 12) & 0x1FFULL;
}

static inline uint64_t vmm_page_base(uint64_t address)
{
    return address & ~(LETHAL_VMM_PAGE_SIZE - 1ULL);
}

static inline uint64_t vmm_entry_phys(uint64_t entry)
{
    return entry & 0x000FFFFFFFFFF000ULL;
}

static inline void vmm_invlpg(uint64_t address)
{
    __asm__ volatile (
        "invlpg (%0)"
        :
        : "r" ((void *)(uintptr_t)address)
        : "memory"
    );
}

static void vmm_zero_table(uint64_t *table)
{
    if (!table) return;
    for (uint64_t i = 0; i < LETHAL_VMM_ENTRIES_PER_TABLE; ++i)
        table[i] = 0;
}

static uint64_t *vmm_table_from_entry(uint64_t entry)
{
    return (uint64_t *)(uintptr_t)vmm_entry_phys(entry);
}

static uint64_t *vmm_root_for(struct vm_space *space)
{
    if (!space) return NULL;
    return (uint64_t *)(uintptr_t)space->pml4_phys;
}

static uint64_t *vmm_ensure_table(
    uint64_t *parent,
    uint64_t index,
    uint64_t flags,
    int *ok
)
{
    uint64_t frame;
    uint64_t *table;

    if (!parent || !ok) return NULL;

    if (!(parent[index] & PTE_PRESENT)) {
        frame = pmm_alloc_frame();
        if (!frame) {
            *ok = 0;
            return NULL;
        }
        table = (uint64_t *)(uintptr_t)frame;
        vmm_zero_table(table);
        parent[index] = frame | PTE_PRESENT | PTE_WRITE;
        if (flags & PTE_USER) parent[index] |= PTE_USER;
        return table;
    }

    if (flags & PTE_USER) parent[index] |= PTE_USER;

    return vmm_table_from_entry(parent[index]);
}

int vmm_map_page_in(
    struct vm_space *space,
    uint64_t virt_addr,
    uint64_t phys_addr,
    uint64_t flags
)
{
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *pd;
    uint64_t *pt;
    int ok = 1;

    if (!space) return -1;
    if ((virt_addr & (LETHAL_VMM_PAGE_SIZE - 1ULL)) != 0) return -1;
    if ((phys_addr & (LETHAL_VMM_PAGE_SIZE - 1ULL)) != 0) return -1;

    pml4 = vmm_root_for(space);
    if (!pml4) return -1;

    pdpt = vmm_ensure_table(pml4, vmm_idx4(virt_addr), flags, &ok);
    if (!ok || !pdpt) return -1;

    pd = vmm_ensure_table(pdpt, vmm_idx3(virt_addr), flags, &ok);
    if (!ok || !pd) return -1;

    pt = vmm_ensure_table(pd, vmm_idx2(virt_addr), flags, &ok);
    if (!ok || !pt) return -1;

    pt[vmm_idx1(virt_addr)] = phys_addr | (flags & 0xFFFULL) | PTE_PRESENT;

    if (space == active_space) vmm_invlpg(virt_addr);

    return 0;
}

int vmm_map_page(
    uint64_t virt_addr,
    uint64_t phys_addr,
    uint64_t flags
)
{
    struct vm_space *space = active_space ? active_space : &kernel_space;
    return vmm_map_page_in(space, virt_addr, phys_addr, flags);
}

static uint64_t *vmm_get_pt(struct vm_space *space, uint64_t virt_addr)
{
    uint64_t *pml4;
    uint64_t *pdpt;
    uint64_t *pd;

    if (!space) return NULL;

    pml4 = vmm_root_for(space);
    if (!pml4) return NULL;
    if (!(pml4[vmm_idx4(virt_addr)] & PTE_PRESENT)) return NULL;

    pdpt = vmm_table_from_entry(pml4[vmm_idx4(virt_addr)]);
    if (!(pdpt[vmm_idx3(virt_addr)] & PTE_PRESENT)) return NULL;

    pd = vmm_table_from_entry(pdpt[vmm_idx3(virt_addr)]);
    if (!(pd[vmm_idx2(virt_addr)] & PTE_PRESENT)) return NULL;

    return vmm_table_from_entry(pd[vmm_idx2(virt_addr)]);
}

void vmm_unmap_page_in(struct vm_space *space, uint64_t virt_addr, int release_frame)
{
    uint64_t *pt;
    uint64_t entry;

    if (!space) return;
    if ((virt_addr & (LETHAL_VMM_PAGE_SIZE - 1ULL)) != 0) return;

    pt = vmm_get_pt(space, virt_addr);
    if (!pt) return;

    entry = pt[vmm_idx1(virt_addr)];
    if (!(entry & PTE_PRESENT)) return;

    pt[vmm_idx1(virt_addr)] = 0;

    if (release_frame && (entry & PTE_USER))
        pmm_free_frame(vmm_entry_phys(entry));

    if (space == active_space) vmm_invlpg(virt_addr);
}

void vmm_unmap_page(uint64_t virt_addr)
{
    struct vm_space *space = active_space ? active_space : &kernel_space;
    vmm_unmap_page_in(space, virt_addr, 0);
}

uint64_t vmm_get_phys_in(struct vm_space *space, uint64_t virt_addr)
{
    uint64_t *pt = vmm_get_pt(space, virt_addr);
    if (!pt) return 0;
    uint64_t entry = pt[vmm_idx1(virt_addr)];
    if (!(entry & PTE_PRESENT)) return 0;
    return vmm_entry_phys(entry) | (virt_addr & (LETHAL_VMM_PAGE_SIZE - 1ULL));
}

uint64_t vmm_get_phys(uint64_t virt_addr)
{
    struct vm_space *space = active_space ? active_space : &kernel_space;
    return vmm_get_phys_in(space, virt_addr);
}

uint64_t vmm_get_flags_in(struct vm_space *space, uint64_t virt_addr)
{
    uint64_t *pt = vmm_get_pt(space, virt_addr);
    if (!pt) return 0;
    return pt[vmm_idx1(virt_addr)];
}

uint64_t vmm_get_flags(uint64_t virt_addr)
{
    struct vm_space *space = active_space ? active_space : &kernel_space;
    return vmm_get_flags_in(space, virt_addr);
}

int vmm_user_range_valid_in(struct vm_space *space, uint64_t start, uint64_t length, int write)
{
    uint64_t end, page, last_page;

    if (!space) return 0;
    if (length == 0) return 1;

    end = start + length - 1ULL;
    if (end < start) return 0;
    if (start < LETHAL_VMM_USER_MIN) return 0;
    if (end > LETHAL_VMM_USER_MAX) return 0;

    page = start & ~(LETHAL_VMM_PAGE_SIZE - 1ULL);
    last_page = end & ~(LETHAL_VMM_PAGE_SIZE - 1ULL);

    for (;;) {
        uint64_t flags = vmm_get_flags_in(space, page);
        if (!(flags & PTE_PRESENT)) return 0;
        if (!(flags & PTE_USER)) return 0;
        if (write && !(flags & PTE_WRITE)) return 0;
        if (page == last_page) break;
        if (page > LETHAL_VMM_USER_MAX - LETHAL_VMM_PAGE_SIZE) return 0;
        page += LETHAL_VMM_PAGE_SIZE;
    }

    return 1;
}

int vmm_user_range_valid(uint64_t start, uint64_t length, int write)
{
    struct vm_space *space = active_space ? active_space : &kernel_space;
    return vmm_user_range_valid_in(space, start, length, write);
}

static int vmm_clone_tables(uint64_t *dst, uint64_t *src, int level, int cow_users)
{
    if (!dst || !src || level < 1) return -1;

    for (uint64_t i = 0; i < LETHAL_VMM_ENTRIES_PER_TABLE; ++i) {
        uint64_t entry = src[i];
        if (!(entry & PTE_PRESENT)) continue;

        if (level == 1) {
            if (cow_users && (entry & PTE_USER)) {
                uint64_t phys = vmm_entry_phys(entry);
                pmm_ref_frame(phys);
                entry &= ~PTE_WRITE;
                entry |= PTE_COW;
                src[i] = entry;
            }
            dst[i] = entry;
            continue;
        }

        uint64_t frame = pmm_alloc_frame();
        if (!frame) return -1;
        uint64_t *child = (uint64_t *)(uintptr_t)frame;
        vmm_zero_table(child);
        dst[i] = frame | (entry & 0xFFFULL);
        if (vmm_clone_tables(child, vmm_table_from_entry(entry), level - 1, cow_users) < 0) {
            pmm_free_frame(frame);
            return -1;
        }
    }

    return 0;
}

struct vm_space *vmm_create_user_space(void)
{
    uint64_t root_frame = pmm_alloc_frame();
    if (!root_frame) return NULL;
    uint64_t *root = (uint64_t *)(uintptr_t)root_frame;
    vmm_zero_table(root);

    struct vm_space *space = (struct vm_space *)kmalloc(sizeof(struct vm_space));
    if (!space) {
        pmm_free_frame(root_frame);
        return NULL;
    }

    space->pml4_phys = root_frame;
    space->brk = 0x0000000060000000ULL;
    space->brk_max = 0x0000000070000000ULL;
    space->mmap_next = 0x0000000100000000ULL;

    if (vmm_clone_tables(root, kernel_pml4, 4, 0) < 0) {
        pmm_free_frame(root_frame);
        kfree(space);
        return NULL;
    }

    return space;
}

struct vm_space *vmm_clone_cow(struct vm_space *parent)
{
    if (!parent) return NULL;

    uint64_t root_frame = pmm_alloc_frame();
    if (!root_frame) return NULL;
    uint64_t *root = (uint64_t *)(uintptr_t)root_frame;
    vmm_zero_table(root);

    struct vm_space *child = (struct vm_space *)kmalloc(sizeof(struct vm_space));
    if (!child) {
        pmm_free_frame(root_frame);
        return NULL;
    }

    *child = *parent;
    child->pml4_phys = root_frame;

    if (vmm_clone_tables(root, vmm_root_for(parent), 4, 1) < 0) {
        pmm_free_frame(root_frame);
        kfree(child);
        return NULL;
    }

    if (parent == active_space) {
        uint64_t page = LETHAL_VMM_USER_MIN;
        for (;;) {
            if (vmm_get_flags_in(parent, page) & PTE_COW) vmm_invlpg(page);
            if (page >= LETHAL_VMM_USER_MAX - LETHAL_VMM_PAGE_SIZE) break;
            page += LETHAL_VMM_PAGE_SIZE;
        }
    }

    return child;
}

static void vmm_destroy_tables(uint64_t *table, int level)
{
    if (!table || level < 1) return;

    for (uint64_t i = 0; i < LETHAL_VMM_ENTRIES_PER_TABLE; ++i) {
        uint64_t entry = table[i];
        if (!(entry & PTE_PRESENT)) continue;

        if (level == 1) {
            if (entry & PTE_USER) pmm_free_frame(vmm_entry_phys(entry));
            continue;
        }

        vmm_destroy_tables(vmm_table_from_entry(entry), level - 1);
        pmm_free_frame(vmm_entry_phys(entry));
    }
}

void vmm_destroy_user_space(struct vm_space *space)
{
    if (!space || space == &kernel_space) return;
    uint64_t *root = vmm_root_for(space);
    if (!root) return;
    vmm_destroy_tables(root, 4);
    pmm_free_frame(space->pml4_phys);
    kfree(space);
}

void vmm_activate_space(struct vm_space *space)
{
    if (!space) space = &kernel_space;
    active_space = space;
    __asm__ volatile ("mov %0, %%cr3" :: "r"(space->pml4_phys) : "memory");
}

struct vm_space *vmm_current_space(void)
{
    return active_space ? active_space : &kernel_space;
}

struct vm_space *vmm_kernel_space(void)
{
    return &kernel_space;
}

int vmm_handle_cow_fault(struct vm_space *space, uint64_t fault_addr, uint64_t error_code)
{
    if (!space) return 0;
    if (!(error_code & 0x2ULL)) return 0;

    uint64_t page = fault_addr & ~(LETHAL_VMM_PAGE_SIZE - 1ULL);
    uint64_t flags = vmm_get_flags_in(space, page);
    if (!(flags & PTE_PRESENT)) return 0;
    if (!(flags & PTE_USER)) return 0;
    if (!(flags & PTE_COW)) return 0;

    uint64_t old_phys = vmm_get_phys_in(space, page) & ~(LETHAL_VMM_PAGE_SIZE - 1ULL);
    if (!old_phys) return 0;

    uint32_t refs = pmm_frame_refs(old_phys);
    if (refs > 1) {
        uint64_t new_phys = pmm_alloc_frame();
        if (!new_phys) return 0;
        uint8_t *src = (uint8_t *)(uintptr_t)old_phys;
        uint8_t *dst = (uint8_t *)(uintptr_t)new_phys;
        for (uint64_t i = 0; i < LETHAL_VMM_PAGE_SIZE; ++i) dst[i] = src[i];
        if (vmm_map_page_in(space, page, new_phys, (flags | PTE_WRITE) & ~PTE_COW) < 0) {
            pmm_free_frame(new_phys);
            return 0;
        }
        pmm_free_frame(old_phys);
    } else {
        uint64_t *pt = vmm_get_pt(space, page);
        if (!pt) return 0;
        pt[vmm_idx1(page)] = (old_phys | (flags | PTE_WRITE)) & ~PTE_COW;
        if (space == active_space) vmm_invlpg(page);
    }

    return 1;
}

int vmm_space_map_anonymous(struct vm_space *space, uint64_t start, uint64_t length, uint64_t flags, int zero)
{
    if (!space || !length) return -1;
    if (start & (LETHAL_VMM_PAGE_SIZE - 1ULL)) return -1;
    if (length & (LETHAL_VMM_PAGE_SIZE - 1ULL)) return -1;
    if (start < LETHAL_VMM_USER_MIN) return -1;

    uint64_t end = start + length;
    if (end <= start) return -1;
    if (end - 1ULL > LETHAL_VMM_USER_MAX) return -1;

    for (uint64_t page = start; page < end; page += LETHAL_VMM_PAGE_SIZE) {
        uint64_t frame = pmm_alloc_frame();
        if (!frame) return -1;
        if (zero) {
            uint8_t *memory = (uint8_t *)(uintptr_t)frame;
            for (uint64_t i = 0; i < LETHAL_VMM_PAGE_SIZE; ++i) memory[i] = 0;
        }
        if (vmm_map_page_in(space, page, frame, flags | PTE_USER | PTE_WRITE) < 0) {
            pmm_free_frame(frame);
            return -1;
        }
    }

    return 0;
}

int vmm_map_mmio(uint64_t phys_addr, uint64_t length)
{
    if (!length) return -1;
    if ((phys_addr & (LETHAL_VMM_PAGE_SIZE - 1ULL)) != 0) return -1;

    uint64_t pages = (length + LETHAL_VMM_PAGE_SIZE - 1ULL) / LETHAL_VMM_PAGE_SIZE;
    for (uint64_t offset = 0; offset < pages * LETHAL_VMM_PAGE_SIZE; offset += LETHAL_VMM_PAGE_SIZE) {
        uint64_t flags = PTE_WRITE | PTE_PRESENT | PTE_PWT | PTE_PCD | PTE_NX;
        if (vmm_map_page(phys_addr + offset, phys_addr + offset, flags) != 0) return -1;
    }

    return 0;
}

void vmm_map_apic(void)
{
    uint64_t flags = PTE_WRITE | PTE_PRESENT | PTE_PWT | PTE_PCD | PTE_NX;
    if (vmm_map_page(LETHAL_VMM_APIC_VIRT, LETHAL_VMM_APIC_PHYS, flags) != 0) {
        serial_print("[vmm] WARNING: failed to map APIC\n");
        panic("APIC mapping failed");
    }
}

void vmm_init(uint64_t highest_phys_addr)
{
    uint64_t root_frame = pmm_alloc_frame();
    if (!root_frame) panic("VMM: cannot allocate PML4");

    kernel_pml4_phys = root_frame;
    kernel_pml4 = (uint64_t *)(uintptr_t)root_frame;
    vmm_zero_table(kernel_pml4);

    kernel_space.pml4_phys = root_frame;
    kernel_space.brk = 0;
    kernel_space.brk_max = 0;
    kernel_space.mmap_next = 0;

    uint64_t text_start   = (uint64_t)(uintptr_t)_text_start;
    uint64_t text_end     = (uint64_t)(uintptr_t)_text_end;
    uint64_t guard_start  = (uint64_t)(uintptr_t)kernel_stack_guard_start;
    uint64_t guard_end    = (uint64_t)(uintptr_t)kernel_stack_guard_end;
    uint64_t mapped = 0;

    for (uint64_t address = 0; address < highest_phys_addr; address += LETHAL_VMM_PAGE_SIZE) {
        /* Skip the guard page: unmapped on purpose so that any access faults. */
        if (address >= guard_start && address < guard_end) continue;

        uint64_t flags = PTE_WRITE;
        if (address < text_start || address >= text_end) {
            flags |= PTE_NX;
        }

        if (vmm_map_page_in(&kernel_space, address, address, flags) != 0) {
            break;
        }
        mapped += LETHAL_VMM_PAGE_SIZE;
    }

    active_space = &kernel_space;
    __asm__ volatile ("mov %0, %%cr3" :: "r"(root_frame) : "memory");

    serial_print("[vmm] initialized, identity-mapped ");
    serial_print_dec(mapped / 1024ULL / 1024ULL);
    serial_print(" MiB, kernel address space active\n");
}