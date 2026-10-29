#ifndef LETHAL_PANIC_H
#define LETHAL_PANIC_H

#include "idt.h"

void panic_exception(struct interrupt_frame *frame);
void panic(const char *msg);

#endif
