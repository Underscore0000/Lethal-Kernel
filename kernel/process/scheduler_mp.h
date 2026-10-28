/*
 * Lethal Kernel - Multi-CPU Scheduler
 *
 * v1.0: Per-CPU run queues with round-robin scheduling.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_SCHEDULER_MP_H
#define LETHAL_SCHEDULER_MP_H

#include <stdint.h>
#include "../smp/cpu.h"
#include "../core/spinlock.h"
#include "task.h"

#define MP_TASK_STACK_SIZE 16384
#define MAX_MP_TASKS 64

struct mp_task {
    uint32_t pid;
    uint32_t state;
    uint8_t *stack;
    uint64_t rsp;
    uint64_t rip;
    uint64_t rbx, rbp, r12, r13, r14, r15;
    char name[32];
    struct mp_task *next;
    int cpu_affinity;
    uint32_t timeslice;
    uint64_t ticks;
    void (*entry)(void);
};

/* Per-CPU scheduler data */
struct scheduler_cpu {
    struct mp_task *current;
    struct mp_task *ready_queue;
    struct mp_task *idle_task;
    int task_count;
    uint64_t ticks;
    spinlock_t lock;
};

/* Scheduler functions */
void scheduler_mp_init(void);
int scheduler_mp_create_task(const char *name, void (*entry)(void));
void scheduler_mp_yield(void);
void scheduler_mp_exit(void);
struct mp_task *scheduler_mp_current(void);
void scheduler_mp_tick(void);

/* SMP functions */
void scheduler_mp_handle_ipi(void);
int scheduler_mp_balance(void);

/* Global task list */
extern struct mp_task mp_tasks[MAX_MP_TASKS];

#endif