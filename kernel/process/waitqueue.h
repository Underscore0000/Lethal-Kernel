/*
 * Lethal Kernel - Wait queues
 *
 * Generic blocking primitive for user processes. Each wait queue is a
 * singly-linked list of user_process pointers protected by a spinlock.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_WAITQUEUE_H
#define LETHAL_WAITQUEUE_H

#include <stdint.h>
#include "../core/spinlock.h"

struct user_process;

struct wait_queue {
    spinlock_t lock;
    struct user_process *head;
    struct user_process *tail;
};

#define WAIT_QUEUE_INIT { .lock = SPINLOCK_INIT, .head = NULL, .tail = NULL }

void wq_init(struct wait_queue *q);

/* Enqueue p and mark it USER_BLOCKED. The caller must already hold q->lock
 * and pass the irqsave flags; this function releases q->lock and restores
 * interrupts before returning. Returns only when p has been woken up and
 * has been made RUNNING again by the scheduler. */
void wq_block_locked(struct wait_queue *q, struct user_process *p,
                     uint64_t flags);

/* Wake up a single waiter. Safe to call from any context (IRQ or syscall). */
void wq_wake_one(struct wait_queue *q);

/* Wake everyone. */
void wq_wake_all(struct wait_queue *q);

/* Remove p from q if present (used on timeout / signal). */
void wq_remove(struct wait_queue *q, struct user_process *p);

#endif