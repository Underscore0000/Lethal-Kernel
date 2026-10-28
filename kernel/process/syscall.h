#ifndef LETHAL_SYSCALL_H
#define LETHAL_SYSCALL_H

#include <stdint.h>
#include "../core/idt.h"

#define SYS_WRITE   1
#define SYS_EXIT    2
#define SYS_MMAP    3
#define SYS_BRK     4
#define SYS_FORK    5
#define SYS_YIELD   6
#define SYS_PIPE    7
#define SYS_READ    8
#define SYS_CLOSE   9

void syscall_handler(struct interrupt_frame *frame);

#endif