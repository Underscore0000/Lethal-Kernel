/*
 * Lethal Kernel - Spinlock
 *
 * v1.0: SMP-aware spinlock with interrupt masking and PAUSE instruction.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_SPINLOCK_H
#define LETHAL_SPINLOCK_H

#include <stdint.h>
#include <stdatomic.h>

typedef struct spinlock {
    volatile int locked;
    volatile uint64_t flags;
} spinlock_t;

#define SPINLOCK_INIT { .locked = 0, .flags = 0 }

void spinlock_init(spinlock_t *lock);
void spinlock_lock(spinlock_t *lock);
void spinlock_unlock(spinlock_t *lock);
int spinlock_trylock(spinlock_t *lock);

/* Safe versions that disable/enable interrupts */
void spinlock_lock_irqsave(spinlock_t *lock, uint64_t *flags);
void spinlock_unlock_irqrestore(spinlock_t *lock, uint64_t flags);

#endif