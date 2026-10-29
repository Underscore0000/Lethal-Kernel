/*
 * Lethal Kernel - Mutex
 * 
 * A sleeping mutex for cooperative multitasking.
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_MUTEX_H
#define LETHAL_MUTEX_H

#include <stdint.h>
#include "spinlock.h"
#include "../process/task.h"

typedef struct mutex {
    spinlock_t spinlock;
    volatile int locked;
    volatile int wait_count;
    struct task *wait_queue[64];
} mutex_t;

#define MUTEX_INIT { .spinlock = SPINLOCK_INIT, .locked = 0, .wait_count = 0 }

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);
int mutex_trylock(mutex_t *mutex);

#endif