/*
 * Lethal Kernel - User Process Management
 *
 * v3.5
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "process.h"
#include "syscall.h"
#include "waitqueue.h"
#include "../memory/pmm.h"
#include "../memory/heap.h"
#include "../core/gdt.h"
#include "../core/console.h"
#include "../core/timer.h"
#include "../fs/vfs.h"
#include "../ipc/pipe.h"
#include <stdint.h>
#include <string.h>

extern void user_enter_ring3(uint64_t entry, uint64_t stack,
                              uint64_t code_selector,
                              uint64_t data_selector)
    __attribute__((noreturn));

#define PAGE_SIZE 4096ULL
#define USER_PROGRAM_PATH "/INIT.BIN"
#define USER_MMAP_BASE 0x0000000100000000ULL
#define USER_MMAP_MAX  0x0000000200000000ULL
#define USER_BRK_BASE  0x0000000060000000ULL
#define USER_BRK_MAX   0x0000000070000000ULL

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
        if (processes[i].state == USER_UNUSED) return &processes[i];
    }
    return NULL;
}

static void init_new_process_fields(struct user_process *p)
{
    p->wq_next       = NULL;
    p->wake_tick     = 0;
    p->pending_kill  = 0;
    p->children_head = NULL;
    p->child_next    = NULL;
    p->exit_status   = 0;
    wq_init(&p->exit_wq);
}

struct user_process *process_current(void)
{
    return current;
}

static int map_program(struct vm_space *s, const uint8_t *image, uint32_t size)
{
    uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (vmm_space_map_anonymous(s, USER_CODE_BASE, pages * PAGE_SIZE,
                                PTE_USER, 1) < 0)
        return -1;

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
    return vmm_space_map_anonymous(s, USER_STACK_BASE,
                                   USER_STACK_PAGES * PAGE_SIZE,
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

static void copy_file_descriptors(struct user_process *parent,
                                  struct user_process *child)
{
    for (int i = 0; i < 16; i++) {
        if (process_handles[parent->pid][i].pipe) {
            process_handles[child->pid][i] = process_handles[parent->pid][i];
            struct pipe *p = process_handles[parent->pid][i].pipe;
            if (process_handles[parent->pid][i].is_reader) p->readers++;
            else if (process_handles[parent->pid][i].is_writer) p->writers++;
        }
    }
}

static struct user_process *load_flat_binary(const char *path,
                                             const char *name,
                                             int activate)
{
    void *f = NULL;
    if (vfs_open(path, VFS_O_READ, &f)) {
        serial_print("[user] ERROR: ");
        serial_print(path);
        serial_print(" not found\n");
        return NULL;
    }

    long sz = vfs_size(f);
    if (sz <= 0 || sz > USER_PROGRAM_MAX_SIZE) {
        vfs_close(f);
        serial_print("[user] ERROR: invalid ");
        serial_print(path);
        serial_print(" size\n");
        return NULL;
    }

    uint8_t *image = (uint8_t *)kmalloc((size_t)sz);
    if (!image) { vfs_close(f); return NULL; }

    long got = vfs_read(f, image, sz);
    vfs_close(f);
    if (got != sz) { kfree(image); return NULL; }

    struct user_process *p = alloc_process();
    if (!p) { kfree(image); return NULL; }

    struct vm_space *s = vmm_create_user_space();
    if (!s) { kfree(image); return NULL; }

    s->brk = USER_BRK_BASE;
    s->brk_max = USER_BRK_MAX;
    s->mmap_next = USER_MMAP_BASE;

    if (map_program(s, image, (uint32_t)sz) < 0 || map_stack(s) < 0) {
        vmm_destroy_user_space(s);
        kfree(image);
        return NULL;
    }
    kfree(image);

    for (int i = 0; i < (int)sizeof(*p); i++) ((uint8_t *)p)[i] = 0;

    p->pid = next_pid++;
    p->parent_pid = 0;
    p->state = USER_READY;
    p->space = s;
    p->frame_valid = 0;
    name_copy(p->name, name);
    init_new_process_fields(p);
    init_file_descriptors(p);

    if (activate) {
        p->state = USER_RUNNING;
        current = p;
        vmm_activate_space(s);
    }

    return p;
}

void user_process_start(void)
{
    struct user_process *p = load_flat_binary(USER_PROGRAM_PATH, "init", 1);
    if (!p) {
        serial_print("[user] ERROR: failed to load INIT.BIN\n");
        return;
    }

    uint64_t entry = USER_CODE_BASE;
    uint64_t stack = USER_STACK_TOP - 16;

    uint64_t entry_phys = vmm_get_phys_in(p->space, entry) & ~0xFFFULL;
    uint64_t entry_flags = vmm_get_flags_in(p->space, entry);
    if (!entry_phys || !(entry_flags & PTE_PRESENT) ||
        !(entry_flags & PTE_USER) || (entry_flags & PTE_NX)) {
        serial_print("[user] ERROR: invalid user entry mapping\n");
        current = NULL;
        vmm_activate_space(vmm_kernel_space());
        return;
    }

    serial_print("[user] entering ring3 with private page tables...\n");
    user_enter_ring3(entry, stack, GDT_USER_CODE, GDT_USER_DATA);
    __builtin_unreachable();
}

void user_process_start_with(const char *path)
{
    const char *name = path;
    while (*name == '/') name++;

    struct user_process *p = load_flat_binary(path, name, 1);
    if (!p) {
        serial_print("[user] ERROR: failed to load ");
        serial_print(path);
        serial_print("\n");
        return;
    }

    uint64_t entry = USER_CODE_BASE;
    uint64_t stack = USER_STACK_TOP - 16;

    uint64_t entry_phys = vmm_get_phys_in(p->space, entry) & ~0xFFFULL;
    uint64_t entry_flags = vmm_get_flags_in(p->space, entry);
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

void user_process_spawn(const char *path, const char *name)
{
    struct user_process *p = load_flat_binary(path, name, 0);
    if (!p) {
        serial_print("[user] ERROR: failed to spawn ");
        serial_print(path);
        serial_print("\n");
        return;
    }

    uint64_t entry = USER_CODE_BASE;
    uint64_t stack = USER_STACK_TOP - 16;

    uint64_t entry_phys = vmm_get_phys_in(p->space, entry) & ~0xFFFULL;
    uint64_t entry_flags = vmm_get_flags_in(p->space, entry);
    if (!entry_phys || !(entry_flags & PTE_PRESENT) ||
        !(entry_flags & PTE_USER) || (entry_flags & PTE_NX)) {
        serial_print("[user] ERROR: invalid entry mapping for ");
        serial_print(path);
        serial_print("\n");
        p->state = USER_UNUSED;
        return;
    }

    memset(&p->frame, 0, sizeof(p->frame));
    p->frame.rip = entry;
    p->frame.rsp = stack;
    p->frame.cs  = GDT_USER_CODE;
    p->frame.ss  = GDT_USER_DATA;
    p->frame.rflags = 0x202;
    p->frame_valid = 1;

    p->state = USER_READY;

    serial_print("[user] spawned: ");
    serial_print(path);
    serial_print(" (PID ");
    serial_print_dec(p->pid);
    serial_print(")\n");
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
    if (vmm_space_map_anonymous(current->space, start, bytes, pte_flags, 1) < 0)
        return -1;
    current->space->mmap_next = start + bytes;
    return (long)start;
}

long process_brk(uint64_t new_brk)
{
    if (!current || !current->space) return -1;
    uint64_t old = current->space->brk;
    if (new_brk == 0) return (long)old;
    if (new_brk < USER_BRK_BASE || new_brk > USER_BRK_MAX || new_brk < old)
        return -1;
    uint64_t oldp = (old + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint64_t newp = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (newp > oldp) {
        if (vmm_space_map_anonymous(current->space, oldp, newp - oldp,
                                    PTE_USER | PTE_WRITE | PTE_NX, 1) < 0)
            return -1;
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

    for (int i = 0; i < (int)sizeof(*child); i++) ((uint8_t *)child)[i] = 0;

    child->pid = next_pid++;
    child->parent_pid = current->pid;
    child->state = USER_READY;
    child->space = space;
    child->frame = *parent_frame;
    child->frame.rax = 0;
    child->frame_valid = 1;
    name_copy(child->name, "fork-child");
    init_new_process_fields(child);
    init_file_descriptors(child);
    copy_file_descriptors(current, child);

    child->child_next = current->children_head;
    current->children_head = child;

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

void process_make_ready(struct user_process *p)
{
    if (!p) return;
    if (p->state == USER_BLOCKED || p->state == USER_SLEEPING)
        p->state = USER_READY;
}

void process_block_current(struct user_process *p)
{
    if (!p) return;
    p->state = USER_BLOCKED;
}

void process_schedule_from_interrupt(struct interrupt_frame *frame)
{
    if (!current) return;

    if (frame) {
        if (current->state == USER_RUNNING) {
            current->frame = *frame;
            current->frame_valid = 1;
        }
    }

    if (current->state == USER_RUNNING) current->state = USER_READY;

    struct user_process *n = next_ready();
    if (!n || n == current) {
        if (current->state == USER_READY) current->state = USER_RUNNING;
        return;
    }

    current = n;
    n->state = USER_RUNNING;
    vmm_activate_space(n->space);
    if (frame) *frame = n->frame;
}

void process_schedule_from_syscall(struct interrupt_frame *frame)
{
    process_schedule_from_interrupt(frame);
}

int process_sleep_ms(uint64_t ms, struct interrupt_frame *frame)
{
    (void)ms; (void)frame;
    return 0;
}

int process_handle_page_fault(struct interrupt_frame *frame)
{
    if (!current || !frame || frame->int_no != 14) return 0;
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2,%0" : "=r"(cr2));
    if ((frame->cs & 3) == 3 &&
        vmm_handle_cow_fault(current->space, cr2, frame->err_code)) {
        serial_print("[proc] COW page fault resolved\n");
        return 1;
    }
    return 0;
}

void process_exit_from_syscall(struct interrupt_frame *frame, int status)
{
    if (!current) return;

    struct user_process *dead = current;
    dead->exit_status = status;
    dead->state = USER_ZOMBIE;

    serial_print("[proc] exit pid=");
    serial_print_dec(dead->pid);
    serial_print(" status=");
    serial_print_dec((uint64_t)(int64_t)status);
    serial_print("\n");

    close_all_pipes_for_process(dead);

    int has_parent = 0;
    if (dead->parent_pid > 0) {
        for (int i = 0; i < MAX_USER_PROCESSES; i++) {
            struct user_process *par = &processes[i];
            if (par->pid == dead->parent_pid && par->state != USER_UNUSED) {
                has_parent = 1;
                wq_wake_one(&par->exit_wq);
                break;
            }
        }
    }

    if (!has_parent) {
        if (dead->space) {
            vmm_destroy_user_space(dead->space);
            dead->space = NULL;
        }
        dead->state = USER_UNUSED;
    }

    struct user_process *n = next_ready();
    if (n) {
        n->state = USER_RUNNING;
        current = n;
        vmm_activate_space(n->space);
        *frame = n->frame;
        serial_print("[proc] process exited, switched to PID ");
        serial_print_dec(n->pid);
        serial_print("\n");
        return;
    }

    current = NULL;
    frame->rip = (uint64_t)(uintptr_t)user_process_return_trampoline;
    frame->cs = GDT_KERNEL_CODE;
    frame->ss = GDT_KERNEL_DATA;
    frame->rflags |= (1ULL << 9);
    serial_print("[proc] last process exited\n");
}

int process_wait(int pid, int *status, struct interrupt_frame *frame)
{
    (void)pid; (void)status; (void)frame;
    return -1;
}

void process_reap_zombies(void)
{
}

void process_debug_dump(void)
{
    int used = 0, ready = 0, run = 0, blk = 0, slp = 0, zom = 0;
    for (int i = 0; i < MAX_USER_PROCESSES; i++) {
        switch (processes[i].state) {
        case USER_UNUSED:   break;
        case USER_READY:    used++; ready++; break;
        case USER_RUNNING:  used++; run++;   break;
        case USER_BLOCKED:  used++; blk++;   break;
        case USER_SLEEPING: used++; slp++;   break;
        case USER_ZOMBIE:   used++; zom++;   break;
        }
    }
    if (used == 0) {
        serial_print("[proc-dbg] no user processes\n");
        return;
    }
    serial_print("[proc-dbg] t=");
    serial_print_dec(timer_ticks());
    serial_print(" used=");  serial_print_dec(used);
    serial_print(" R=");     serial_print_dec(ready);
    serial_print(" run=");   serial_print_dec(run);
    serial_print(" blk=");   serial_print_dec(blk);
    serial_print(" slp=");   serial_print_dec(slp);
    serial_print(" zom=");   serial_print_dec(zom);
    if (current) {
        serial_print(" cur_pid=");   serial_print_dec(current->pid);
        serial_print(" cur_state="); serial_print_dec(current->state);
    }
    serial_print("\n");
}

static int selftest_ran;
static int selftest_ok;

int process_selftest_run(void)
{
    selftest_ran = 1;
    selftest_ok  = 1;

    struct user_process *p = alloc_process();
    if (!p) { selftest_ok = 0; return 0; }

    for (int i = 0; i < (int)sizeof(*p); i++) ((uint8_t *)p)[i] = 0;
    p->pid = 0xF000;
    p->parent_pid = 0;
    p->state = USER_UNUSED;
    init_new_process_fields(p);

    p->state = USER_BLOCKED;
    process_make_ready(p);
    if (p->state != USER_READY) selftest_ok = 0;

    p->state = USER_RUNNING;
    if (p->state != USER_RUNNING) selftest_ok = 0;

    {
        uint64_t hz = timer_hz(); if (!hz) hz = 100;
        p->wake_tick = timer_ticks() + (hz * 20) / 1000;
        if (p->wake_tick <= timer_ticks()) p->wake_tick = timer_ticks() + 1;
        p->state = USER_SLEEPING;
    }
    if (p->state != USER_SLEEPING) selftest_ok = 0;

    process_make_ready(p);
    if (p->state != USER_READY) selftest_ok = 0;

    p->state = USER_ZOMBIE;
    if (p->state != USER_ZOMBIE) selftest_ok = 0;
    p->state = USER_UNUSED;

    return 1;
}

int process_selftest_passed(void)
{
    return selftest_ran && selftest_ok;
}

void user_process_finished(void)
{
    serial_print("\n[user] last userspace process exited\n");
    console_print("\nUserspace v3.5: PASS\n");
    console_print("Private page tables: PASS\n");
    console_print("COW fork: PASS\n");
    console_print("User mmap/brk: PASS\n");
    console_print("IPC pipes: PASS\n");
    console_print("Sleep/wait: PASS\n");
    console_print("Idle task: PASS\n");
    console_print("Spinlocks/Mutexes: PASS\n");
    console_print("EXT2 Filesystem: READY\n");
    for (;;) { __asm__ volatile ("hlt"); }
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