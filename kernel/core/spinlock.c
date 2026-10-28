/*
 * Lethal Kernel - Spinlock implementation
 *
 * v1.0: SMP-aware with PAUSE instruction for multi-core.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "spinlock.h"
#include "../../arch/x86_64/io.h"

void spinlock_init(spinlock_t *lock)
{
    lock->locked = 0;
    lock->flags = 0;
}

static inline void spinlock_acquire(spinlock_t *lock)
{
    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
        __asm__ volatile ("pause");
    }
}

static inline void spinlock_release(spinlock_t *lock)
{
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}

void spinlock_lock(spinlock_t *lock)
{
    spinlock_acquire(lock);
}

void spinlock_unlock(spinlock_t *lock)
{
    spinlock_release(lock);
}

int spinlock_trylock(spinlock_t *lock)
{
    return !__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE);
}

static inline uint64_t read_rflags(void)
{
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0" : "=r"(flags));
    return flags;
}

static inline void write_rflags(uint64_t flags)
{
    __asm__ volatile ("pushq %0; popfq" :: "r"(flags));
}

void spinlock_lock_irqsave(spinlock_t *lock, uint64_t *flags)
{
    *flags = read_rflags();
    __asm__ volatile ("cli");
    spinlock_acquire(lock);
}

void spinlock_unlock_irqrestore(spinlock_t *lock, uint64_t flags)
{
    spinlock_release(lock);
    write_rflags(flags);
}