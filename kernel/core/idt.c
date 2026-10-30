/*
 * Lethal Kernel - IDT (Interrupt Descriptor Table)
 *
 * v1.0: Added IPI (Inter-Processor Interrupt) handlers for SMP support.
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "idt.h"
#include "panic.h"
#include "timer.h"
#include "../process/syscall.h"
#include "../../arch/x86_64/io.h"
#include "../drivers/input/keyboard.h"
#include "../process/process.h"
#include "../smp/ipi.h"
#include <stddef.h>
#include "../drivers/serial/serial.h"

int current_pid_for_debug(void)
{
    extern struct user_process *process_current(void);
    struct user_process *p = process_current();
    return p ? p->pid : -1;
}

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES] __attribute__((aligned(16)));
static struct idt_ptr ip;

extern void idt_load(uint64_t idt_ptr);

extern void isr0(void),  isr1(void),  isr2(void),  isr3(void);
extern void isr4(void),  isr5(void),  isr6(void),  isr7(void);
extern void isr8(void),  isr9(void),  isr10(void), isr11(void);
extern void isr12(void), isr13(void), isr14(void), isr15(void);
extern void isr16(void), isr17(void), isr18(void), isr19(void);
extern void isr20(void), isr21(void), isr22(void), isr23(void);
extern void isr24(void), isr25(void), isr26(void), isr27(void);
extern void isr28(void), isr29(void), isr30(void), isr31(void);

extern void irq0(void),  irq1(void),  irq2(void),  irq3(void);
extern void irq4(void),  irq5(void),  irq6(void),  irq7(void);
extern void irq8(void),  irq9(void),  irq10(void), irq11(void);
extern void irq12(void), irq13(void), irq14(void), irq15(void);

/* IPI (Inter-Processor Interrupt) handlers - defined in isr_stubs.asm */
extern void ipi0xF0_handler(void);  /* schedule */
extern void ipi0xF1_handler(void);  /* sync */
extern void ipi0xF2_handler(void);  /* stop */
extern void ipi0xF3_handler(void);  /* tlb_flush */
extern void ipi0xF4_handler(void);  /* debug */

extern void syscall128(void);

static void set_gate(uint8_t num, uint64_t handler,
                     uint16_t selector, uint8_t flags)
{
    idt[num].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].selector    = selector;
    idt[num].ist         = 0;
    idt[num].type_attr   = flags;
    idt[num].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[num].zero        = 0;
}

static void pic_remap(void)
{
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0x00); outb(0xA1, 0x00);
}

void idt_init(void)
{
    const uint16_t kernel_cs = 0x08;
    const uint8_t  kernel_gate = 0x8E;
    const uint8_t  user_gate = 0xEE;  /* DPL=3 for syscall */

    /* Clear all IDT entries */
    for (size_t i = 0; i < IDT_ENTRIES; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].type_attr = 0;
        idt[i].offset_mid = 0;
        idt[i].offset_high = 0;
        idt[i].zero = 0;
    }

    ip.limit = sizeof(idt) - 1;
    ip.base  = (uint64_t)(uintptr_t)&idt;

    /* --------------------------------------------------------------------
     * CPU Exceptions (0-31)
     * -------------------------------------------------------------------- */
    set_gate(0,  (uint64_t)isr0,  kernel_cs, kernel_gate);
    set_gate(1,  (uint64_t)isr1,  kernel_cs, kernel_gate);
    set_gate(2,  (uint64_t)isr2,  kernel_cs, kernel_gate);
    set_gate(3,  (uint64_t)isr3,  kernel_cs, kernel_gate);
    set_gate(4,  (uint64_t)isr4,  kernel_cs, kernel_gate);
    set_gate(5,  (uint64_t)isr5,  kernel_cs, kernel_gate);
    set_gate(6,  (uint64_t)isr6,  kernel_cs, kernel_gate);
    set_gate(7,  (uint64_t)isr7,  kernel_cs, kernel_gate);
    set_gate(8,  (uint64_t)isr8,  kernel_cs, kernel_gate);
    set_gate(9,  (uint64_t)isr9,  kernel_cs, kernel_gate);
    set_gate(10, (uint64_t)isr10, kernel_cs, kernel_gate);
    set_gate(11, (uint64_t)isr11, kernel_cs, kernel_gate);
    set_gate(12, (uint64_t)isr12, kernel_cs, kernel_gate);
    set_gate(13, (uint64_t)isr13, kernel_cs, kernel_gate);
    set_gate(14, (uint64_t)isr14, kernel_cs, kernel_gate);
    set_gate(15, (uint64_t)isr15, kernel_cs, kernel_gate);
    set_gate(16, (uint64_t)isr16, kernel_cs, kernel_gate);
    set_gate(17, (uint64_t)isr17, kernel_cs, kernel_gate);
    set_gate(18, (uint64_t)isr18, kernel_cs, kernel_gate);
    set_gate(19, (uint64_t)isr19, kernel_cs, kernel_gate);
    set_gate(20, (uint64_t)isr20, kernel_cs, kernel_gate);
    set_gate(21, (uint64_t)isr21, kernel_cs, kernel_gate);
    set_gate(22, (uint64_t)isr22, kernel_cs, kernel_gate);
    set_gate(23, (uint64_t)isr23, kernel_cs, kernel_gate);
    set_gate(24, (uint64_t)isr24, kernel_cs, kernel_gate);
    set_gate(25, (uint64_t)isr25, kernel_cs, kernel_gate);
    set_gate(26, (uint64_t)isr26, kernel_cs, kernel_gate);
    set_gate(27, (uint64_t)isr27, kernel_cs, kernel_gate);
    set_gate(28, (uint64_t)isr28, kernel_cs, kernel_gate);
    set_gate(29, (uint64_t)isr29, kernel_cs, kernel_gate);
    set_gate(30, (uint64_t)isr30, kernel_cs, kernel_gate);
    set_gate(31, (uint64_t)isr31, kernel_cs, kernel_gate);

    /* --------------------------------------------------------------------
     * PIC IRQ remapping (32-47)
     * -------------------------------------------------------------------- */
    pic_remap();

    set_gate(32, (uint64_t)irq0,  kernel_cs, kernel_gate);   /* PIT Timer */
    set_gate(33, (uint64_t)irq1,  kernel_cs, kernel_gate);   /* Keyboard */
    set_gate(34, (uint64_t)irq2,  kernel_cs, kernel_gate);
    set_gate(35, (uint64_t)irq3,  kernel_cs, kernel_gate);
    set_gate(36, (uint64_t)irq4,  kernel_cs, kernel_gate);
    set_gate(37, (uint64_t)irq5,  kernel_cs, kernel_gate);
    set_gate(38, (uint64_t)irq6,  kernel_cs, kernel_gate);
    set_gate(39, (uint64_t)irq7,  kernel_cs, kernel_gate);
    set_gate(40, (uint64_t)irq8,  kernel_cs, kernel_gate);
    set_gate(41, (uint64_t)irq9,  kernel_cs, kernel_gate);
    set_gate(42, (uint64_t)irq10, kernel_cs, kernel_gate);
    set_gate(43, (uint64_t)irq11, kernel_cs, kernel_gate);
    set_gate(44, (uint64_t)irq12, kernel_cs, kernel_gate);
    set_gate(45, (uint64_t)irq13, kernel_cs, kernel_gate);
    set_gate(46, (uint64_t)irq14, kernel_cs, kernel_gate);
    set_gate(47, (uint64_t)irq15, kernel_cs, kernel_gate);

    /* --------------------------------------------------------------------
     * IPI (Inter-Processor Interrupt) vectors (0xF0-0xF4)
     * These are used for SMP inter-core communication.
     * -------------------------------------------------------------------- */
    set_gate(0xF0, (uint64_t)ipi0xF0_handler, kernel_cs, kernel_gate);
    set_gate(0xF1, (uint64_t)ipi0xF1_handler, kernel_cs, kernel_gate);
    set_gate(0xF2, (uint64_t)ipi0xF2_handler, kernel_cs, kernel_gate);
    set_gate(0xF3, (uint64_t)ipi0xF3_handler, kernel_cs, kernel_gate);
    set_gate(0xF4, (uint64_t)ipi0xF4_handler, kernel_cs, kernel_gate);

    /* --------------------------------------------------------------------
     * System Call (0x80) - DPL=3 so ring3 code can call it
     * -------------------------------------------------------------------- */
    set_gate(0x80, (uint64_t)syscall128, kernel_cs, user_gate);

    /* Load the IDT */
    idt_load((uint64_t)(uintptr_t)&ip);
}

void isr_handler(struct interrupt_frame *frame)
{
    /* Page fault - check if it's a COW fault */
    if (frame->int_no == 14 && process_handle_page_fault(frame)) {
        return;
    }
    panic_exception(frame);
}

void irq_handler(struct interrupt_frame *frame)
{
    uint8_t irq = (uint8_t)(frame->int_no - 32);

    if (irq == 0) {
        timer_tick_handler();
        process_schedule_from_interrupt(frame);
    }
    else if (irq == 1) {
        keyboard_irq_handler();
    }

    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}