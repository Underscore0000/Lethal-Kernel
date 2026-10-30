#ifndef LETHAL_PROCESS_H
#define LETHAL_PROCESS_H

#include <stdint.h>
#include "../memory/vmm.h"
#include "../core/idt.h"
#include "waitqueue.h"

/* Forward declaration for pipe_handle - defined in pipe.h */
struct pipe_handle;

#define USER_CODE_BASE  0x0000000040000000ULL
#define USER_STACK_BASE 0x0000000080000000ULL
#define USER_STACK_PAGES 8
#define USER_STACK_TOP (USER_STACK_BASE + USER_STACK_PAGES * 4096ULL)
#define USER_PROGRAM_MAX_SIZE (256u*1024u)
#define MAX_USER_PROCESSES 16

enum user_process_state {
    USER_UNUSED = 0,
    USER_READY,
    USER_RUNNING,
    USER_BLOCKED,
    USER_SLEEPING,
    USER_ZOMBIE,
};

struct user_process {
    int pid;
    int parent_pid;
    enum user_process_state state;
    struct vm_space *space;
    struct interrupt_frame frame;
    int frame_valid;
    int exit_status;
    char name[16];

    struct user_process *wq_next;
    uint64_t wake_tick;
    int      pending_kill;

    struct user_process *children_head;
    struct user_process *child_next;
    struct wait_queue    exit_wq;
};

void user_process_start(void);
void user_process_start_with(const char *path);
void user_process_spawn(const char *path, const char *name);
void user_process_finished(void);
void process_cleanup(struct user_process *p);

struct user_process *process_current(void);
int  process_fork(struct interrupt_frame *parent_frame);
long process_mmap(uint64_t length, uint64_t flags);
long process_brk(uint64_t new_brk);
void process_schedule_from_interrupt(struct interrupt_frame *frame);
int  process_handle_page_fault(struct interrupt_frame *frame);
void process_exit_from_syscall(struct interrupt_frame *frame, int status);

void process_block_current(struct user_process *p);
void process_make_ready(struct user_process *p);
void process_schedule_from_syscall(struct interrupt_frame *frame);
int  process_sleep_ms(uint64_t ms, struct interrupt_frame *frame);
int  process_wait(int pid, int *status, struct interrupt_frame *frame);
void process_reap_zombies(void);

void process_debug_dump(void);
int  process_selftest_run(void);
int  process_selftest_passed(void);

extern void user_process_return_trampoline(void);

#endif