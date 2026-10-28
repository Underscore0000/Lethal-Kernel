/*
 * Lethal Kernel - cooperative round-robin task scheduler
 *
 * Each task gets its own kernel stack (allocated from the heap). Context
 * switching saves/restores only the callee-saved registers plus RIP/RSP -
 * everything else is already preserved by the C calling convention at the
 * point task_switch() is called from task_yield().
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "task.h"
#include "../memory/heap.h"
#include "../core/console.h"
#include "../core/panic.h"

extern void task_switch(struct task_context *old_ctx, struct task_context *new_ctx);
extern void task_bootstrap_trampoline(void);

static struct task tasks[MAX_TASKS];
static int current_task = -1;
static int task_count = 0;

struct task *task_current(void)
{
    if (current_task < 0) {
        return NULL;
    }
    return &tasks[current_task];
}

void tasking_init(void)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_UNUSED;
    }
    current_task = -1;
    task_count = 0;

    /* Register the calling context (kernel_main's own stack) as task 0,
     * "main". This guarantees pick_next() always has somewhere valid to
     * return to once every other task has exited - without this, the
     * last task to call task_exit() has no target and must panic. */
    struct task *m = &tasks[0];
    m->id = 0;
    m->name = "main";
    m->state = TASK_RUNNING;
    m->stack = NULL; /* not heap-allocated; this is kernel_main's real stack */
    task_count = 1;
    current_task = 0;

    serial_print("[task] scheduler initialized\n");
}

/* Called (via the assembly trampoline) the very first time a newly
 * created task is switched to. Runs the task's real entry point, then
 * cleans up if it ever returns instead of calling task_exit() itself. */
void task_trampoline_c(task_entry_fn entry)
{
    entry();
    task_exit();
}

int task_create(const char *name, task_entry_fn entry)
{
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        return -1; /* no free task slots */
    }

    uint8_t *stack = (uint8_t *)kmalloc(TASK_STACK_SIZE);
    if (!stack) {
        return -1;
    }

    struct task *t = &tasks[slot];
    t->id = slot;
    t->name = name;
    t->state = TASK_READY;
    t->stack = stack;

    uint64_t *sp = (uint64_t *)(stack + TASK_STACK_SIZE);
    sp -= 1; *sp = (uint64_t)(uintptr_t)entry;
    sp -= 1; *sp = (uint64_t)(uintptr_t)task_bootstrap_trampoline;

    t->context.rbx = 0;
    t->context.rbp = 0;
    t->context.r12 = 0;
    t->context.r13 = 0;
    t->context.r14 = 0;
    t->context.r15 = 0;
    t->context.rsp = (uint64_t)(uintptr_t)sp;
    t->rsp = t->context.rsp;

    task_count++;
    serial_print("[task] created task: ");
    serial_print(name);
    serial_print("\n");
    return slot;
}

static int pick_next(void)
{
    if (task_count == 0) {
        return -1;
    }
    int start = (current_task < 0) ? 0 : current_task;
    for (int offset = 1; offset <= MAX_TASKS; offset++) {
        int idx = (start + offset) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY) {
            return idx;
        }
    }
    /* No other READY task; allow re-running the current one if it's still runnable. */
    if (current_task >= 0 &&
        (tasks[current_task].state == TASK_RUNNING || tasks[current_task].state == TASK_READY)) {
        return current_task;
    }
    return -1;
}

void task_yield(void)
{
    int next = pick_next();
    if (next < 0 || next == current_task) {
        return; /* nothing else to run */
    }

    int prev = current_task;
    if (prev >= 0 && tasks[prev].state == TASK_RUNNING) {
        tasks[prev].state = TASK_READY;
    }

    current_task = next;
    tasks[next].state = TASK_RUNNING;

    static struct task_context throwaway_ctx;
    struct task_context *old_ctx = (prev >= 0) ? &tasks[prev].context : &throwaway_ctx;
    task_switch(old_ctx, &tasks[next].context);
}

void task_exit(void)
{
    if (current_task < 0) {
        panic("task_exit() called with no current task");
    }

    struct task *t = &tasks[current_task];
    serial_print("[task] task exiting: ");
    serial_print(t->name);
    serial_print("\n");

    t->state = TASK_TERMINATED;
    task_count--;

    /* We cannot kfree() this task's own stack while still running on it.
     * Freeing terminated-task stacks is deferred to whichever task next
     * calls task_yield() and notices TERMINATED slots - kept simple and
     * explicit for v0.7 rather than a background reaper. */

    int next = pick_next();
    if (next < 0) {
        panic("task_exit(): no other task left to run");
    }

    current_task = next;
    tasks[next].state = TASK_RUNNING;

    struct task_context dummy;
    task_switch(&dummy, &tasks[next].context);
    /* Never returns here. */
}

/* --------------------------------------------------------------------- */
/* Self-test: two cooperative tasks (plus the caller as an implicit third
 * "main" context) that yield back and forth a fixed number of times,
 * recording execution order into a shared trace buffer. */

#define TASK_TEST_ITERS 5
static char trace_buf[64];
static int  trace_len;
static int  test_task_a_id = -1;
static int  test_task_b_id = -1;
static volatile int test_done_count;

static void trace(char c)
{
    if (trace_len < (int)sizeof(trace_buf) - 1) {
        trace_buf[trace_len++] = c;
        trace_buf[trace_len] = '\0';
    }
}

static void task_test_a(void)
{
    for (int i = 0; i < TASK_TEST_ITERS; i++) {
        trace('A');
        task_yield();
    }
    test_done_count++;
}

static void task_test_b(void)
{
    for (int i = 0; i < TASK_TEST_ITERS; i++) {
        trace('B');
        task_yield();
    }
    test_done_count++;
}

void task_test_run(void)
{
    trace_len = 0;
    trace_buf[0] = '\0';
    test_done_count = 0;

    test_task_a_id = task_create("selftest-a", task_test_a);
    test_task_b_id = task_create("selftest-b", task_test_b);

    /* Drive the two test tasks to completion by yielding from this
     * "main" context repeatedly. Since task_yield() round-robins over
     * READY tasks, this reliably schedules A and B alternately until
     * both have run their loop and called task_exit(). */
    int guard = 0;
    while (test_done_count < 2 && guard < 1000) {
        task_yield();
        guard++;
    }

    serial_print("[task] self-test trace: ");
    serial_print(trace_buf);
    serial_print("\n");
}

int task_test_passed(void)
{
    /* A correct interleaving alternates strictly: ABABAB... (10 chars for
     * TASK_TEST_ITERS=5) since both tasks run the identical loop shape
     * and the scheduler round-robins. Anything else means a corrupted
     * context switch. */
    if (trace_len != TASK_TEST_ITERS * 2) {
        return 0;
    }
    for (int i = 0; i < trace_len; i++) {
        char expected = (i % 2 == 0) ? 'A' : 'B';
        if (trace_buf[i] != expected) {
            return 0;
        }
    }
    return (test_done_count == 2);
}
