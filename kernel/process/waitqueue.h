#ifndef LETHAL_WAITQUEUE_H
#define LETHAL_WAITQUEUE_H

#include <stdint.h>
#include "../core/spinlock.h"
#include "../core/idt.h"

struct user_process;

struct wait_queue {
    spinlock_t lock;
    struct user_process *head;
    struct user_process *tail;
};

#define WAIT_QUEUE_INIT { .lock = SPINLOCK_INIT, .head = NULL, .tail = NULL }

void wq_init(struct wait_queue *q);

void wq_block_locked(struct wait_queue *q, struct user_process *p,
                     uint64_t flags, struct interrupt_frame *frame);

void wq_wake_one(struct wait_queue *q);
void wq_wake_all(struct wait_queue *q);
void wq_remove(struct wait_queue *q, struct user_process *p);

#endif