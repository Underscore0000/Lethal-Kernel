/*
 * Lethal Kernel - x86_64 Model-Specific Registers
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_ARCH_X86_64_MSR_H
#define LETHAL_ARCH_X86_64_MSR_H

#include <stdint.h>

/* IA32_APIC_BASE MSR */
#define MSR_APIC_BASE       0x1B
#define MSR_APIC_BASE_EN    (1ULL << 11)
#define MSR_APIC_BASE_BSP   (1ULL << 8)

/* IA32_EFER MSR */
#define MSR_EFER            0xC0000080
#define MSR_EFER_SCE        (1ULL << 0)
#define MSR_EFER_LME        (1ULL << 8)
#define MSR_EFER_LMA        (1ULL << 10)
#define MSR_EFER_NXE        (1ULL << 11)

/* GS Base MSRs */
#define MSR_GS_BASE         0xC0000101
#define MSR_KERNEL_GS_BASE  0xC0000102

static inline uint64_t read_msr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void write_msr(uint32_t msr, uint64_t value)
{
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr" :: "c"(msr), "a"(lo), "d"(hi));
}

#endif /* LETHAL_ARCH_X86_64_MSR_H */