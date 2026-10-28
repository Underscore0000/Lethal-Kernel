/*
 * Lethal Kernel - Multi-CPU Scheduler Implementation
 *
 * v1.0: Round-robin scheduler with per-CPU run queues.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "scheduler_mp.h"
#include "../core/console.h"
#include "../core/panic.h"
#include "../memory/heap.h"
#include "../smp/ipi.h"
#include "../smp/apic.h"
#include "../smp/smp.h"
#include <stddef.h>

#define TIMESLICE 10

struct mp_task mp_tasks[MAX_MP_TASKS];
static uint32_t next_pid = 1;
static struct scheduler_cpu per_cpu[MAX_CPUS];
static spinlock_t task_lock = SPINLOCK_INIT;

extern void mp_task_switch(struct mp_task *old, struct mp_task *new);
extern void mp_task_bootstrap(void);

static struct mp_task *alloc_mp_task(void)
{
    spinlock_lock(&task_lock);
    for (int i = 0; i < MAX_MP_TASKS; i++) {
        if (mp_tasks[i].state == 0 || mp_tasks[i].state == 3) { /* UNUSED or ZOMBIE */
            struct mp_task *t = &mp_tasks[i];
            spinlock_unlock(&task_lock);
            return t;
        }
    }
    spinlock_unlock(&task_lock);
    return NULL;
}

static void init_idle_task(int cpu_id)
{
    struct scheduler_cpu *sc = &per_cpu[cpu_id];
    struct mp_task *idle = alloc_mp_task();
    if (!idle) panic("scheduler_mp: no task for idle");
    
    idle->pid = 0;
    idle->state = 1; /* READY */
    idle->cpu_affinity = cpu_id;
    idle->name[0] = 'i';
    idle->name[1] = 'd';
    idle->name[2] = 'l';
    idle->name[3] = 'e';
    idle->name[4] = 0;
    idle->next = NULL;
    idle->stack = NULL;
    idle->entry = NULL;
    
    sc->idle_task = idle;
    sc->current = idle;
    sc->ready_queue = idle;
    sc->task_count = 1;
}

void scheduler_mp_init(void)
{
    serial_print("[sched_mp] initializing multi-CPU scheduler\n");
    
    /* Initialize per-CPU data */
    for (int i = 0; i < MAX_CPUS; i++) {
        per_cpu[i].current = NULL;
        per_cpu[i].ready_queue = NULL;
        per_cpu[i].idle_task = NULL;
        per_cpu[i].task_count = 0;
        per_cpu[i].ticks = 0;
        spinlock_init(&per_cpu[i].lock);
    }
    
    /* Clear tasks */
    for (int i = 0; i < MAX_MP_TASKS; i++) {
        mp_tasks[i].state = 0;
        mp_tasks[i].stack = NULL;
    }
    
    /* Create idle task for current CPU */
    int cpu_id = smp_current_cpu();
    if (cpu_id < 0) cpu_id = 0;
    init_idle_task(cpu_id);
    
    serial_print("[sched_mp] initialized\n");
}

int scheduler_mp_create_task(const char *name, void (*entry)(void))
{
    struct mp_task *t = alloc_mp_task();
    if (!t) return -1;
    
    /* Allocate stack */
    uint8_t *stack = (uint8_t *)kmalloc(MP_TASK_STACK_SIZE);
    if (!stack) return -1;
    
    /* Initialize task */
    t->pid = next_pid++;
    t->state = 1; /* READY */
    t->stack = stack;
    t->entry = entry;
    t->cpu_affinity = -1;  /* any CPU */
    t->timeslice = TIMESLICE;
    t->ticks = 0;
    t->next = NULL;
    
    /* Set up stack for first switch */
    uint64_t *sp = (uint64_t *)(stack + MP_TASK_STACK_SIZE);
    sp -= 1;
    *sp = (uint64_t)entry;  /* return address */
    sp -= 1;
    *sp = (uint64_t)mp_task_bootstrap;
    t->rsp = (uint64_t)sp;
    t->rip = (uint64_t)entry;
    
    /* Copy name */
    int i = 0;
    while (name[i] && i < 31) {
        t->name[i] = name[i];
        i++;
    }
    t->name[i] = 0;
    
    /* Add to ready queue of current CPU */
    int cpu_id = smp_current_cpu();
    if (cpu_id < 0) cpu_id = 0;
    struct scheduler_cpu *sc = &per_cpu[cpu_id];
    
    spinlock_lock(&sc->lock);
    t->next = sc->ready_queue;
    sc->ready_queue = t;
    sc->task_count++;
    spinlock_unlock(&sc->lock);
    
    serial_print("[sched_mp] created task: ");
    serial_print(name);
    serial_print(" (PID ");
    serial_print_dec(t->pid);
    serial_print(")\n");
    
    return t->pid;
}

void scheduler_mp_yield(void)
{
    int cpu_id = smp_current_cpu();
    if (cpu_id < 0) cpu_id = 0;
    struct scheduler_cpu *sc = &per_cpu[cpu_id];
    
    spinlock_lock(&sc->lock);
    
    struct mp_task *current = sc->current;
    if (!current || current == sc->idle_task) {
        spinlock_unlock(&sc->lock);
        return;
    }
    
    /* Move current task to READY */
    current->state = 1; /* READY */
    
    /* Find next ready task */
    struct mp_task *next = sc->ready_queue;
    
    while (next) {
        if (next->state == 1 && next != current) { /* READY */
            break;
        }
        next = next->next;
    }
    
    if (!next) {
        next = sc->idle_task;
    }
    
    /* Remove current from queue */
    if (sc->ready_queue == current) {
        sc->ready_queue = current->next;
    } else {
        struct mp_task *t = sc->ready_queue;
        while (t && t->next != current) t = t->next;
        if (t) t->next = current->next;
    }
    current->next = NULL;
    
    /* Move next to front */
    if (next != sc->idle_task) {
        if (sc->ready_queue != next) {
            struct mp_task *t = sc->ready_queue;
            while (t && t->next != next) t = t->next;
            if (t) t->next = next->next;
        }
        next->next = sc->ready_queue;
        sc->ready_queue = next;
    }
    
    spinlock_unlock(&sc->lock);
    
    if (next && next != current) {
        next->state = 2; /* RUNNING */
        sc->current = next;
        mp_task_switch(current, next);
    }
}

void scheduler_mp_exit(void)
{
    int cpu_id = smp_current_cpu();
    if (cpu_id < 0) cpu_id = 0;
    struct scheduler_cpu *sc = &per_cpu[cpu_id];
    
    spinlock_lock(&sc->lock);
    
    struct mp_task *current = sc->current;
    if (!current || current == sc->idle_task) {
        spinlock_unlock(&sc->lock);
        return;
    }
    
    current->state = 3; /* ZOMBIE */
    sc->task_count--;
    
    /* Find next ready task */
    struct mp_task *next = sc->ready_queue;
    while (next && (next->state != 1 || next == current)) { /* READY */
        next = next->next;
    }
    
    if (!next) next = sc->idle_task;
    
    /* Remove current from queue */
    if (sc->ready_queue == current) {
        sc->ready_queue = current->next;
    } else {
        struct mp_task *t = sc->ready_queue;
        while (t && t->next != current) t = t->next;
        if (t) t->next = current->next;
    }
    current->next = NULL;
    
    spinlock_unlock(&sc->lock);
    
    next->state = 2; /* RUNNING */
    sc->current = next;
    
    serial_print("[sched_mp] task exiting: ");
    serial_print(current->name);
    serial_print("\n");
    
    if (current->stack) {
        kfree(current->stack);
        current->stack = NULL;
    }
    
    if (next != current) {
        mp_task_switch(current, next);
    }
}

struct mp_task *scheduler_mp_current(void)
{
    int cpu_id = smp_current_cpu();
    if (cpu_id < 0) cpu_id = 0;
    return per_cpu[cpu_id].current;
}

void scheduler_mp_tick(void)
{
    int cpu_id = smp_current_cpu();
    if (cpu_id < 0) cpu_id = 0;
    struct scheduler_cpu *sc = &per_cpu[cpu_id];
    
    sc->ticks++;
    struct mp_task *current = sc->current;
    if (!current || current == sc->idle_task) return;
    
    current->ticks++;
    if (current->ticks % TIMESLICE == 0) {
        scheduler_mp_yield();
    }
}

void scheduler_mp_handle_ipi(void)
{
    scheduler_mp_yield();
}

int scheduler_mp_balance(void)
{
    int max_cpu = 0, max_tasks = 0;
    int min_cpu = 0, min_tasks = 999;
    
    for (int i = 0; i < MAX_CPUS; i++) {
        if (per_cpu[i].task_count > max_tasks) {
            max_tasks = per_cpu[i].task_count;
            max_cpu = i;
        }
        if (per_cpu[i].task_count < min_tasks) {
            min_tasks = per_cpu[i].task_count;
            min_cpu = i;
        }
    }
    
    if (max_tasks - min_tasks > 2 && max_tasks > 1) {
        spinlock_lock(&per_cpu[max_cpu].lock);
        struct mp_task *t = per_cpu[max_cpu].ready_queue;
        if (t && t != per_cpu[max_cpu].idle_task) {
            per_cpu[max_cpu].ready_queue = t->next;
            per_cpu[max_cpu].task_count--;
            t->next = NULL;
            spinlock_unlock(&per_cpu[max_cpu].lock);
            
            spinlock_lock(&per_cpu[min_cpu].lock);
            t->next = per_cpu[min_cpu].ready_queue;
            per_cpu[min_cpu].ready_queue = t;
            per_cpu[min_cpu].task_count++;
            spinlock_unlock(&per_cpu[min_cpu].lock);
            
            if (min_cpu != smp_current_cpu()) {
                ipi_send_cpu(cpus[min_cpu].apic_id, IPI_VECTOR_SCHEDULE);
            }
            return 1;
        }
        spinlock_unlock(&per_cpu[max_cpu].lock);
    }
    return 0;
}