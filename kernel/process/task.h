#ifndef LETHAL_TASK_H
#define LETHAL_TASK_H

#include <stdint.h>

#define TASK_STACK_SIZE 16384
#define MAX_TASKS 16

enum task_state {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_TERMINATED
};

/* Saved callee-saved register context for a cooperative context switch.
 * Only what task_switch.asm actually saves/restores needs to live here -
 * caller-saved registers are already spilled by the C calling convention
 * before task_switch() is ever called. */
struct task_context {
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;
};

struct task {
    int id;
    enum task_state state;
    struct task_context context;
    uint8_t *stack;              /* base of this task's allocated stack (for kfree) */
    uint64_t rsp;                /* saved stack pointer, valid while not RUNNING */
    const char *name;
};

typedef void (*task_entry_fn)(void);

void tasking_init(void);
int  task_create(const char *name, task_entry_fn entry);
void task_yield(void);
struct task *task_current(void);
void task_exit(void);

/* Self-test used by kernel.c to prove the context switch is correct. */
void task_test_run(void);
int  task_test_passed(void);

#endif
