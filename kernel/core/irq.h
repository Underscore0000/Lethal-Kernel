/*
 * Lethal Kernel - IRQ (Interrupt Request) Management
 *
 * v5.0: IRQ handling and registration.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_CORE_IRQ_H
#define LETHAL_CORE_IRQ_H

#include <stdint.h>
#include <stddef.h>

/* IRQ Handler function type */
typedef void (*irq_handler_t)(void *data);

/* IRQ Handler structure */
struct irq_handler {
    irq_handler_t handler;
    void *data;
    uint8_t registered;
};

/* Function prototypes */
int irq_init(void);
int irq_register(uint8_t irq, irq_handler_t handler, void *data);
int irq_unregister(uint8_t irq);
void irq_enable(uint8_t irq);
void irq_disable(uint8_t irq);
void irq_acknowledge(uint8_t irq);
void irq_handle(uint8_t irq);

#endif /* LETHAL_CORE_IRQ_H */