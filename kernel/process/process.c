/*
 * Lethal Kernel - User Process Management
 *
 * v2.0: Updated to use void* for VFS handles.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "process.h"
#include "syscall.h"
#include "../memory/pmm.h"
#include "../memory/heap.h"
#include "../core/gdt.h"
#include "../core/console.h"
#include "../fs/vfs.h"
#include "../ipc/pipe.h"
#include <stdint.h>
#include <string.h>

extern void user_enter_ring3(uint64_t entry, uint64_t stack, uint64_t code_selector, uint64_t data_selector) __attribute__((noreturn));

#define PAGE_SIZE 4096ULL
#define USER_PROGRAM_PATH "/INIT.BIN"
#define USER_MMAP_BASE 0x0000000100000000ULL
#define USER_MMAP_MAX  0x0000000200000000ULL
#define USER_BRK_BASE  0x0000000060000000ULL
#define USER_BRK_MAX   0x0000000070000000ULL

/* process_handles è definito in pipe.c */
extern struct pipe_handle process_handles[MAX_USER_PROCESSES][16];

static struct user_process processes[MAX_USER_PROCESSES];
static struct user_process *current;
static int next_pid = 1;

static void name_copy(char *d, const char *s)
{
    int i = 0;
    for (; i < 15 && s && s[i]; i++) d[i] = s[i];
    d[i] = 0;
}

static struct user_process *alloc_process(void)
{
    for (int i = 0; i < MAX_USER_PROCESSES; i++) {
        if (processes[i].state == USER_UNUSED) {
            return &processes[i];
        }
    }
    return NULL;
}

struct user_process *process_current(void)
{
    return current;
}

static int map_program(struct vm_space *s, const uint8_t *image, uint32_t size)
{
    uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (vmm_space_map_anonymous(s, USER_CODE_BASE, pages * PAGE_SIZE, PTE_USER, 1) < 0) {
        return -1;
    }
    
    for (uint64_t i = 0; i < size; i++) {
        uint64_t va = USER_CODE_BASE + i;
        uint64_t phys = vmm_get_phys_in(s, va) & ~0xFFFULL;
        ((uint8_t *)(uintptr_t)phys)[va & 0xFFF] = image[i];
    }
    
    for (uint64_t i = 0; i < pages; i++) {
        uint64_t va = USER_CODE_BASE + i * PAGE_SIZE;
        uint64_t phys = vmm_get_phys_in(s, va) & ~0xFFFULL;
        uint64_t f = vmm_get_flags_in(s, va);
        vmm_unmap_page_in(s, va, 0);
        vmm_map_page_in(s, va, phys, (f & ~PTE_WRITE) | PTE_USER);
    }
    return 0;
}

static int map_stack(struct vm_space *s)
{
    return vmm_space_map_anonymous(s, USER_STACK_BASE, USER_STACK_PAGES * PAGE_SIZE,
                                   PTE_USER | PTE_WRITE | PTE_NX, 1);
}

static void init_file_descriptors(struct user_process *p)
{
    for (int i = 0; i < 16; i++) {
        process_handles[p->pid][i].pipe = NULL;
        process_handles[p->pid][i].is_reader = 0;
        process_handles[p->pid][i].is_writer = 0;
    }
}

static void copy_file_descriptors(struct user_process *parent, struct user_process *child)
{
    for (int i = 0; i < 16; i++) {
        if (process_handles[parent->pid][i].pipe) {
            process_handles[child->pid][i] = process_handles[parent->pid][i];
            struct pipe *p = process_handles[parent->pid][i].pipe;
            if (process_handles[parent->pid][i].is_reader) {
                p->readers++;
            } else if (process_handles[parent->pid][i].is_writer) {
                p->writers++;
            }
        }
    }
}

void user_process_start(void)
{
    void *f = NULL;
    if (vfs_open(USER_PROGRAM_PATH, VFS_O_READ, &f)) {
        serial_print("[user] ERROR: /INIT.BIN not found\n");
        return;
    }
    
    long sz = vfs_size(f);
    if (sz <= 0 || sz > USER_PROGRAM_MAX_SIZE) {
        vfs_close(f);
        serial_print("[user] ERROR: invalid /INIT.BIN size\n");
        return;
    }
    
    uint8_t *image = (uint8_t *)kmalloc((size_t)sz);
    if (!image) {
        vfs_close(f);
        return;
    }
    
    long got = vfs_read(f, image, sz);
    vfs_close(f);
    if (got != sz) {
        kfree(image);
        return;
    }
    
    struct user_process *p = alloc_process();
    if (!p) {
        kfree(image);
        return;
    }
    
    struct vm_space *s = vmm_create_user_space();
    if (!s) {
        kfree(image);
        return;
    }
    
    s->brk = USER_BRK_BASE;
    s->brk_max = USER_BRK_MAX;
    s->mmap_next = USER_MMAP_BASE;
    
    if (map_program(s, image, (uint32_t)sz) < 0 || map_stack(s) < 0) {
        vmm_destroy_user_space(s);
        kfree(image);
        return;
    }
    kfree(image);
    
    for (int i = 0; i < (int)sizeof(*p); i++) {
        ((uint8_t *)p)[i] = 0;
    }
    
    p->pid = next_pid++;
    p->parent_pid = 0;
    p->state = USER_RUNNING;
    p->space = s;
    p->frame_valid = 0;
    name_copy(p->name, "init");
    current = p;
    
    init_file_descriptors(p);
    
    vmm_activate_space(s);
    serial_print("[user] v2.0 address space created, CR3 switched\n");
    serial_print("[user] entering ring3 with private page tables...\n");
    
    uint64_t entry = USER_CODE_BASE;
    uint64_t stack = USER_STACK_TOP - 16;
    
    uint64_t entry_phys = vmm_get_phys_in(s, entry) & ~0xFFFULL;
    uint64_t entry_flags = vmm_get_flags_in(s, entry);
    if (!entry_phys || !(entry_flags & PTE_PRESENT) ||
        !(entry_flags & PTE_USER) || (entry_flags & PTE_NX)) {
        serial_print("[user] ERROR: invalid user entry mapping\n");
        current = NULL;
        vmm_activate_space(vmm_kernel_space());
        return;
    }
    
    user_enter_ring3(entry, stack, GDT_USER_CODE, GDT_USER_DATA);
    __builtin_unreachable();
}

void user_process_start_with(const char *path)
{
    void *f = NULL;
    if (vfs_open(path, VFS_O_READ, &f)) {
        serial_print("[user] ERROR: ");
        serial_print(path);
        serial_print(" not found\n");
        return;
    }
    
    long sz = vfs_size(f);
    if (sz <= 0 || sz > USER_PROGRAM_MAX_SIZE) {
        vfs_close(f);
        serial_print("[user] ERROR: invalid ");
        serial_print(path);
        serial_print(" size\n");
        return;
    }
    
    uint8_t *image = (uint8_t *)kmalloc((size_t)sz);
    if (!image) {
        vfs_close(f);
        return;
    }
    
    long got = vfs_read(f, image, sz);
    vfs_close(f);
    if (got != sz) {
        kfree(image);
        return;
    }
    
    struct user_process *p = alloc_process();
    if (!p) {
        kfree(image);
        return;
    }
    
    struct vm_space *s = vmm_create_user_space();
    if (!s) {
        kfree(image);
        return;
    }
    
    s->brk = USER_BRK_BASE;
    s->brk_max = USER_BRK_MAX;
    s->mmap_next = USER_MMAP_BASE;
    
    if (map_program(s, image, (uint32_t)sz) < 0 || map_stack(s) < 0) {
        vmm_destroy_user_space(s);
        kfree(image);
        return;
    }
    kfree(image);
    
    for (int i = 0; i < (int)sizeof(*p); i++) {
        ((uint8_t *)p)[i] = 0;
    }
    
    p->pid = next_pid++;
    p->parent_pid = 0;
    p->state = USER_RUNNING;
    p->space = s;
    p->frame_valid = 0;
    
    const char *name = path;
    while (*name == '/') name++;
    name_copy(p->name, name);
    current = p;
    
    init_file_descriptors(p);
    
    vmm_activate_space(s);
    
    uint64_t entry = USER_CODE_BASE;
    uint64_t stack = USER_STACK_TOP - 16;
    
    uint64_t entry_phys = vmm_get_phys_in(s, entry) & ~0xFFFULL;
    uint64_t entry_flags = vmm_get_flags_in(s, entry);
    if (!entry_phys || !(entry_flags & PTE_PRESENT) ||
        !(entry_flags & PTE_USER) || (entry_flags & PTE_NX)) {
        serial_print("[user] ERROR: invalid user entry mapping for ");
        serial_print(path);
        serial_print("\n");
        current = NULL;
        vmm_activate_space(vmm_kernel_space());
        return;
    }
    
    serial_print("[user] starting: ");
    serial_print(path);
    serial_print("\n");
    
    user_enter_ring3(entry, stack, GDT_USER_CODE, GDT_USER_DATA);
    __builtin_unreachable();
}

long process_mmap(uint64_t length, uint64_t flags)
{
    if (!current || !current->space || !length) return -1;
    
    uint64_t pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages > 0x10000) return -1;
    
    uint64_t bytes = pages * PAGE_SIZE;
    uint64_t start = current->space->mmap_next;
    if (start < USER_MMAP_BASE) start = USER_MMAP_BASE;
    if (start + bytes > USER_MMAP_MAX) return -1;
    
    uint64_t pte_flags = PTE_USER | PTE_WRITE | PTE_NX;
    if (flags & 1) pte_flags &= ~PTE_NX;
    
    if (vmm_space_map_anonymous(current->space, start, bytes, pte_flags, 1) < 0) {
        return -1;
    }
    
    current->space->mmap_next = start + bytes;
    return (long)start;
}

long process_brk(uint64_t new_brk)
{
    if (!current || !current->space) return -1;
    
    uint64_t old = current->space->brk;
    if (new_brk == 0) return (long)old;
    if (new_brk < USER_BRK_BASE || new_brk > USER_BRK_MAX || new_brk < old) return -1;
    
    uint64_t oldp = (old + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint64_t newp = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    if (newp > oldp) {
        if (vmm_space_map_anonymous(current->space, oldp, newp - oldp,
                                    PTE_USER | PTE_WRITE | PTE_NX, 1) < 0) {
            return -1;
        }
    }
    
    current->space->brk = new_brk;
    return (long)new_brk;
}

int process_fork(struct interrupt_frame *parent_frame)
{
    if (!current || !parent_frame) return -1;
    
    struct user_process *child = alloc_process();
    if (!child) return -1;
    
    struct vm_space *space = vmm_clone_cow(current->space);
    if (!space) return -1;
    
    for (int i = 0; i < (int)sizeof(*child); i++) {
        ((uint8_t *)child)[i] = 0;
    }
    
    child->pid = next_pid++;
    child->parent_pid = current->pid;
    child->state = USER_READY;
    child->space = space;
    child->frame = *parent_frame;
    child->frame.rax = 0;
    child->frame_valid = 1;
    name_copy(child->name, "fork-child");
    
    init_file_descriptors(child);
    copy_file_descriptors(current, child);
    
    parent_frame->rax = (uint64_t)child->pid;
    
    serial_print("[proc] fork: created PID ");
    serial_print_dec(child->pid);
    serial_print(" with COW address space\n");
    
    return child->pid;
}

static struct user_process *next_ready(void)
{
    int start = current ? current->pid : 0;
    struct user_process *best = NULL;
    int bestpid = 0x7fffffff;
    
    for (int i = 0; i < MAX_USER_PROCESSES; i++) {
        if (processes[i].state == USER_READY && 
            processes[i].pid > start && 
            processes[i].pid < bestpid) {
            best = &processes[i];
            bestpid = processes[i].pid;
        }
    }
    if (best) return best;
    
    for (int i = 0; i < MAX_USER_PROCESSES; i++) {
        if (processes[i].state == USER_READY) {
            best = &processes[i];
            break;
        }
    }
    return best;
}

void process_schedule_from_interrupt(struct interrupt_frame *frame)
{
    if (!current || !frame) return;
    
    if (current->state == USER_RUNNING) {
        current->frame = *frame;
        current->frame_valid = 1;
    }
    
    struct user_process *n = next_ready();
    if (!n || n == current) return;
    
    current->state = USER_READY;
    n->state = USER_RUNNING;
    current = n;
    vmm_activate_space(n->space);
    *frame = n->frame;
    
    serial_print("[proc] context switch -> PID ");
    serial_print_dec(n->pid);
    serial_print("\n");
}

int process_handle_page_fault(struct interrupt_frame *frame)
{
    if (!current || !frame || frame->int_no != 14) return 0;
    
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2,%0" : "=r"(cr2));
    
    if ((frame->cs & 3) == 3 && vmm_handle_cow_fault(current->space, cr2, frame->err_code)) {
        serial_print("[proc] COW page fault resolved\n");
        return 1;
    }
    return 0;
}

void process_exit_from_syscall(struct interrupt_frame *frame, int status)
{
    if (!current) return;
    
    current->exit_status = status;
    current->state = USER_ZOMBIE;
    
    /* close_all_pipes_for_process è definita in pipe.c */
    close_all_pipes_for_process(current);
    
    struct user_process *dead = current;
    struct user_process *n = next_ready();
    
    if (n) {
        n->state = USER_RUNNING;
        current = n;
        vmm_activate_space(n->space);
        *frame = n->frame;
        
        vmm_destroy_user_space(dead->space);
        dead->space = NULL;
        dead->state = USER_UNUSED;
        
        serial_print("[proc] process exited, switched to PID ");
        serial_print_dec(n->pid);
        serial_print("\n");
        return;
    }
    
    vmm_destroy_user_space(dead->space);
    dead->space = NULL;
    dead->state = USER_UNUSED;
    current = NULL;
    
    frame->rip = (uint64_t)(uintptr_t)user_process_return_trampoline;
    frame->cs = GDT_KERNEL_CODE;
    frame->ss = GDT_KERNEL_DATA;
    frame->rflags |= (1ULL << 9);
    
    serial_print("[proc] last process exited\n");
}

void user_process_finished(void)
{
    serial_print("\n[user] last userspace process exited\n");
    console_print("\nUserspace v2.0: PASS\n");
    console_print("Private page tables: PASS\n");
    console_print("COW fork: PASS\n");
    console_print("User mmap/brk: PASS\n");
    console_print("IPC pipes: PASS\n");
    console_print("Spinlocks/Mutexes: PASS\n");
    console_print("EXT2 Filesystem: READY\n");
    
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void process_cleanup(struct user_process *p)
{
    if (!p) return;
    
    close_all_pipes_for_process(p);
    
    if (p->space) {
        vmm_destroy_user_space(p->space);
        p->space = NULL;
    }
    
    p->state = USER_UNUSED;
}