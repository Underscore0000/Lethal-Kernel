#include "panic.h"
#include "console.h"

static const char *exception_names[32] = {
    "Divide Error", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
    "x87 FP Exception", "Alignment Check", "Machine Check", "SIMD FP Exception",
    "Virtualization Exception", "Control Protection Exception",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection", "VMM Communication", "Security Exception", "Reserved"
};

static void print_hex64(uint64_t v)
{
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    buf[18] = '\0';
    for (int i = 0; i < 16; i++) {
        uint8_t nibble = (uint8_t)((v >> ((15 - i) * 4)) & 0xF);
        buf[2 + i] = (char)(nibble < 10 ? ('0' + nibble) : ('A' + nibble - 10));
    }
    console_print(buf);
    serial_print(buf);
}

static void line(const char *label, uint64_t value)
{
    console_print(label);
    serial_print(label);
    print_hex64(value);
    console_print("\n");
    serial_print("\n");
}

void panic_exception(struct interrupt_frame *frame)
{
    uint64_t cr2 = 0;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    console_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    console_print("\n*** KERNEL PANIC: CPU EXCEPTION ***\n");
    serial_print("\n*** KERNEL PANIC: CPU EXCEPTION ***\n");

    const char *name = (frame->int_no < 32) ? exception_names[frame->int_no] : "Unknown";
    console_print(name);
    console_print("\n");
    serial_print(name);
    serial_print("\n");

    line("vector:   ", frame->int_no);
    line("error:    ", frame->err_code);
    line("rip:      ", frame->rip);
    line("cs:       ", frame->cs);
    line("rflags:   ", frame->rflags);
    line("rsp:      ", frame->rsp);
    line("ss:       ", frame->ss);
    if (frame->int_no == 14) {
        line("cr2 (fault addr): ", cr2);
    }

    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void panic(const char *msg)
{
    console_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    console_print("\n*** KERNEL PANIC ***\n");
    console_print(msg);
    console_print("\n");
    serial_print("\n*** KERNEL PANIC ***\n");
    serial_print(msg);
    serial_print("\n");

    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
