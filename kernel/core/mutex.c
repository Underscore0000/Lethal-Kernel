/*
 * Lethal Kernel - Mutex implementation
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "mutex.h"
#include "../core/console.h"
#include "../process/task.h"

void mutex_init(mutex_t *mutex)
{
    spinlock_init(&mutex->spinlock);
    mutex->locked = 0;
    mutex->wait_count = 0;
}

void mutex_lock(mutex_t *mutex)
{
    uint64_t flags;
    spinlock_lock_irqsave(&mutex->spinlock, &flags);
    
    while (mutex->locked) {

        if (mutex->wait_count < 64) {
            mutex->wait_queue[mutex->wait_count++] = task_current();
        }
        spinlock_unlock_irqrestore(&mutex->spinlock, flags);
        task_yield();
        spinlock_lock_irqsave(&mutex->spinlock, &flags);
    }
    
    mutex->locked = 1;
    spinlock_unlock_irqrestore(&mutex->spinlock, flags);
}

void mutex_unlock(mutex_t *mutex)
{
    uint64_t flags;
    spinlock_lock_irqsave(&mutex->spinlock, &flags);
    
    mutex->locked = 0;
    mutex->wait_count = 0; 
    
    spinlock_unlock_irqrestore(&mutex->spinlock, flags);
}

int mutex_trylock(mutex_t *mutex)
{
    uint64_t flags;
    int result = 0;
    
    spinlock_lock_irqsave(&mutex->spinlock, &flags);
    if (!mutex->locked) {
        mutex->locked = 1;
        result = 1;
    }
    spinlock_unlock_irqrestore(&mutex->spinlock, flags);
    
    return result;
}