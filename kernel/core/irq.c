/*
 * Lethal Kernel - IRQ management
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "irq.h"
#include "../arch/x86_64/io.h"
#include "../drivers/serial/serial.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

#define ICW1_INIT    0x10
#define ICW1_ICW4    0x01
#define ICW4_8086    0x01

static irq_handler_t irq_handlers[16] = { NULL };
static void         *irq_data[16]     = { NULL };

void pic_remap(void)
{
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    outb(PIC1_DATA, 0x20);   /* master offset 32 */
    outb(PIC2_DATA, 0x28);   /* slave offset 40  */

    outb(PIC1_DATA, 0x04);   
    outb(PIC2_DATA, 0x02);

    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    
    outb(PIC1_DATA, 0xF8);   /* 1111 1000 */
    outb(PIC2_DATA, 0xEF);   /* 1110 1111 */
}

void irq_mask(uint8_t irq)
{
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) | (1 << irq);
    outb(port, value);
}

void irq_unmask(uint8_t irq)
{
    uint16_t port;
    uint8_t value;

    if (irq >= 8) {
        
        uint8_t master = inb(PIC1_DATA);
        outb(PIC1_DATA, master & ~(1 << 2));
        port = PIC2_DATA;
        irq -= 8;
    } else {
        port = PIC1_DATA;
    }
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

int irq_register(uint8_t irq, irq_handler_t handler, void *data)
{
    if (irq > 15 || !handler) return -1;

    irq_handlers[irq] = handler;
    irq_data[irq]     = data;

    irq_unmask(irq);

    serial_print("[irq] Registered IRQ ");
    serial_print_dec(irq);
    serial_print("\n");
    return 0;
}

int irq_unregister(uint8_t irq)
{
    if (irq > 15) return -1;
    irq_mask(irq);
    irq_handlers[irq] = NULL;
    irq_data[irq]     = NULL;
    return 0;
}

void irq_dispatch(uint8_t irq)
{
    if (irq > 15) return;

    if (irq_handlers[irq]) {
        irq_handlers[irq](irq_data[irq]);
    }

    if (irq >= 8) outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}