#include "gdt.h"
#include <stddef.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct tss_ldescriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0, rsp1, rsp2;
    uint64_t reserved1;
    uint64_t ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

/*
 * GDT layout:
 *   0x00 null
 *   0x08 kernel code
 *   0x10 kernel data
 *   0x18 user code
 *   0x20 user data
 *   0x28 TSS (16-byte descriptor)
 */
#define GDT_NORMAL_ENTRIES 5
#define GDT_BLOB_SIZE (8 * GDT_NORMAL_ENTRIES + 16)

static struct gdt_entry gdt[GDT_NORMAL_ENTRIES] __attribute__((aligned(16)));
static struct tss_ldescriptor tss_desc __attribute__((aligned(16)));
static struct tss_entry tss __attribute__((aligned(16)));
static uint8_t gdt_blob[GDT_BLOB_SIZE] __attribute__((aligned(16)));
static struct gdt_ptr gp;

extern void gdt_flush(uint64_t gdt_ptr);
extern void tss_flush(void);

static void set_entry(int idx, uint32_t base, uint32_t limit,
                       uint8_t access, uint8_t gran)
{
    gdt[idx].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[idx].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt[idx].base_high   = (uint8_t)((base >> 24) & 0xFF);
    gdt[idx].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[idx].granularity = (uint8_t)((limit >> 16) & 0x0F);
    gdt[idx].granularity |= gran & 0xF0;
    gdt[idx].access      = access;
}

void gdt_init(void)
{
    for (size_t i = 0; i < sizeof(tss); i++) {
        ((uint8_t *)&tss)[i] = 0;
    }

    /*
     * No I/O bitmap is present. Setting iomap_base to sizeof(TSS) makes
     * every port access use the normal IOPL/privilege rules.
     */
    tss.iomap_base = sizeof(struct tss_entry);
    tss.rsp0 = 0;

    set_entry(0, 0, 0, 0, 0);
    set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0); /* ring0 64-bit code */
    set_entry(2, 0, 0xFFFFF, 0x92, 0xC0); /* ring0 data */
    set_entry(3, 0, 0xFFFFF, 0xFA, 0xA0); /* ring3 64-bit code */
    set_entry(4, 0, 0xFFFFF, 0xF2, 0xC0); /* ring3 data */

    uint64_t tss_base = (uint64_t)(uintptr_t)&tss;
    uint32_t tss_limit = sizeof(struct tss_entry) - 1;

    tss_desc.limit_low   = (uint16_t)(tss_limit & 0xFFFF);
    tss_desc.base_low    = (uint16_t)(tss_base & 0xFFFF);
    tss_desc.base_mid    = (uint8_t)((tss_base >> 16) & 0xFF);
    tss_desc.access      = 0x89; /* present, ring0, available 64-bit TSS */
    tss_desc.granularity = (uint8_t)((tss_limit >> 16) & 0x0F);
    tss_desc.base_high   = (uint8_t)((tss_base >> 24) & 0xFF);
    tss_desc.base_upper  = (uint32_t)(tss_base >> 32);
    tss_desc.reserved    = 0;

    for (int i = 0; i < GDT_NORMAL_ENTRIES; i++) {
        __builtin_memcpy(gdt_blob + i * 8, &gdt[i], 8);
    }
    __builtin_memcpy(gdt_blob + 8 * GDT_NORMAL_ENTRIES, &tss_desc, 16);

    gp.limit = sizeof(gdt_blob) - 1;
    gp.base  = (uint64_t)(uintptr_t)gdt_blob;

    gdt_flush((uint64_t)(uintptr_t)&gp);
    tss_flush();
}

void tss_set_rsp0(uint64_t rsp0)
{
    tss.rsp0 = rsp0;
}

uint64_t tss_get_rsp0(void)
{
    return tss.rsp0;
}
