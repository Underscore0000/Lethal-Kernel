#include "waitqueue.h"
#include "process.h"
#include "../core/spinlock.h"
#include <stddef.h>

void wq_init(struct wait_queue *q)
{
    spinlock_init(&q->lock);
    q->head = NULL;
    q->tail = NULL;
}

static void enqueue_locked(struct wait_queue *q, struct user_process *p)
{
    p->wq_next = NULL;
    if (q->tail) q->tail->wq_next = p;
    else         q->head = p;
    q->tail = p;
}

static void remove_locked(struct wait_queue *q, struct user_process *p)
{
    struct user_process *prev = NULL;
    struct user_process *cur  = q->head;
    while (cur) {
        if (cur == p) {
            if (prev) prev->wq_next = cur->wq_next;
            else      q->head = cur->wq_next;
            if (q->tail == cur) q->tail = prev;
            cur->wq_next = NULL;
            return;
        }
        prev = cur;
        cur = cur->wq_next;
    }
}

void wq_block_locked(struct wait_queue *q, struct user_process *p,
                     uint64_t flags, struct interrupt_frame *frame)
{
    enqueue_locked(q, p);
    process_block_current(p);
    spinlock_unlock_irqrestore(&q->lock, flags);
    process_schedule_from_syscall(frame);
}

void wq_wake_one(struct wait_queue *q)
{
    uint64_t flags;
    spinlock_lock_irqsave(&q->lock, &flags);
    struct user_process *p = q->head;
    if (p) {
        remove_locked(q, p);
        process_make_ready(p);
    }
    spinlock_unlock_irqrestore(&q->lock, flags);
}

void wq_wake_all(struct wait_queue *q)
{
    uint64_t flags;
    spinlock_lock_irqsave(&q->lock, &flags);
    struct user_process *p = q->head;
    while (p) {
        struct user_process *next = p->wq_next;
        p->wq_next = NULL;
        process_make_ready(p);
        p = next;
    }
    q->head = q->tail = NULL;
    spinlock_unlock_irqrestore(&q->lock, flags);
}

void wq_remove(struct wait_queue *q, struct user_process *p)
{
    uint64_t flags;
    spinlock_lock_irqsave(&q->lock, &flags);
    remove_locked(q, p);
    spinlock_unlock_irqrestore(&q->lock, flags);
}