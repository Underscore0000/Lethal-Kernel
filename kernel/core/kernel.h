/*
 * Lethal Kernel - kernel-wide declarations
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_KERNEL_H
#define LETHAL_KERNEL_H

#include <stdint.h>
#include <stddef.h>

/* Entry point called from arch/x86_64/entry.asm once long mode, the stack
 * and .bss are all set up. rdi/rsi carry the BIOS E820 memory map pointer
 * and entry count forwarded from boot/stage2.asm (System V AMD64: first
 * two integer args). Never expected to return. */
void kernel_main(uint32_t magic, uint64_t info_addr);

#endif /* LETHAL_KERNEL_H */
