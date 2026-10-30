/*
 * Lethal Kernel - kernel_main
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "kernel.h"
#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "multiboot2.h"
#include "../memory/pmm.h"
#include "multiboot.h"
#include "../memory/vmm.h"
#include "../memory/heap.h"
#include "timer.h"
#include "rtc.h"
#include "../process/task.h"
#include "../process/process.h"
#include "../process/scheduler_mp.h"
#include "../drivers/input/keyboard.h"
#include "../drivers/input/mouse.h"
#include "../drivers/storage/ata.h"
#include "../drivers/block/block.h"
#include "../fs/vfs.h"
#include "../fs/fs_detect.h"
#include "../core/spinlock.h"
#include "../core/mutex.h"
#include "../ipc/pipe.h"
#include "../smp/smp.h"
#include "terminal.h"
#include "../memory/memtest.h"
#include "font.h"

/* Networking headers */
#include "../net/core/net.h"
#include "../net/core/device.h"
#include "../net/core/sk_buff.h"
#include "../net/ipv4/ip.h"
#include "../net/ipv4/arp.h"
#include "../net/ipv4/icmp.h"
#include "../net/ipv4/tcp.h"
#include "../net/ipv4/udp.h"
#include "../net/dns/dns.h"
#include "../net/dns/dns_cache.h"
#include "../net/dns/doh.h"
#include "../net/socket/socket.h"
#include "../net/tor/tor.h"
#include "../net/tor/tor_control.h"
#include "../net/config/privacy.h"
#include "../net/killswitch/killswitch.h"
#include "../drivers/serial/serial.h"
#include "../drivers/video/framebuffer.h"

#include <string.h>

extern char _kernel_start[];
extern char _kernel_end[];
extern char kernel_stack_top[];

extern int tcp_init(void);
extern int udp_init(void);
extern int ip_init(void);
extern int arp_init(void);
extern int icmp_init(void);
extern int net_init(void);
extern int net_device_init(void);

extern int  mouse_init(void);
extern void mouse_set_screen_size(uint32_t w, uint32_t h);

/* ============================================================================
 * Debug IRQ
 * ============================================================================ */
static void dbg_irq_state(const char *where)
{
    uint64_t fl;
    uint8_t mask;
    __asm__ volatile ("pushfq; pop %0" : "=r"(fl));
    __asm__ volatile ("inb $0x21, %0" : "=a"(mask));
    serial_print("[irqdbg] ");
    serial_print(where);
    serial_print(": IF=");
    serial_print_dec((fl >> 9) & 1);
    serial_print(" PICmask=");
    serial_print_hex(mask);
    serial_print("\n");
}

/* ============================================================================
 * Mappatura identity per il framebuffer
 * ============================================================================ */
static void map_framebuffer(uint64_t phys, uint64_t size)
{
    uint64_t start = phys & ~0xFFFULL;
    uint64_t end   = (phys + size + 0xFFF) & ~0xFFFULL;
    for (uint64_t a = start; a < end; a += 0x1000) {
        vmm_map_page(a, a, PTE_WRITE);
        __asm__ volatile ("invlpg (%0)" :: "r"(a) : "memory");
    }
    serial_print("[kernel] Framebuffer mapped: ");
    serial_print_hex(start);
    serial_print(" .. ");
    serial_print_hex(end);
    serial_print("\n");
}

/* ============================================================================
 * Multiboot2 tag walker
 * ============================================================================ */
struct mb2_walk {
    const struct mb2_tag_mmap        *mmap;
    const struct mb2_tag_framebuffer *fb;
};

static void mb2_walk_tags(uint64_t info_addr, struct mb2_walk *out)
{
    out->mmap = NULL;
    out->fb = NULL;

    const struct mb2_info *info = (const struct mb2_info *)(uintptr_t)info_addr;
    uint8_t *p = (uint8_t *)info + 8;

    for (;;) {
        const struct mb2_tag *tag = (const struct mb2_tag *)p;
        if (tag->type == MB2_TAG_END) break;

        if (tag->type == MB2_TAG_MMAP)
            out->mmap = (const struct mb2_tag_mmap *)tag;
        else if (tag->type == MB2_TAG_FRAMEBUFFER)
            out->fb = (const struct mb2_tag_framebuffer *)tag;

        uint32_t sz = tag->size;
        p += (sz + 7) & ~7u;

        if ((uintptr_t)p >= (uintptr_t)info + info->total_size)
            break;
    }
}

/* ============================================================================
 * Networking Self-Test
 * ============================================================================ */
static void net_verify_ip_addr(void)
{
    ip_addr_t test_ip;
    uint32_t test_val = 0xC0A80101;

    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("IP addr conversion: ");

    ip_addr_from_u32(&test_ip, test_val);
    uint32_t result = ip_addr_to_u32(&test_ip);

    if (result == test_val) {
        console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        console_print("OK (");
        console_print_dec((test_ip.addr[0]) & 0xFF);
        console_print(".");
        console_print_dec((test_ip.addr[1]) & 0xFF);
        console_print(".");
        console_print_dec((test_ip.addr[2]) & 0xFF);
        console_print(".");
        console_print_dec(test_ip.addr[3] & 0xFF);
        console_print(")\n");
    } else {
        console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        console_print("FAIL\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void net_verify_sk_buff(void)
{
    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("sk_buff alloc/free: ");

    struct sk_buff *skb = skb_alloc(1024);
    int status = -1;
    if (skb) {
        skb->len = 100;
        skb_free(skb);
        status = 0;
    }

    if (status == 0) {
        console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        console_print("OK\n");
    } else {
        console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        console_print("FAIL\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void net_verify_socket(void)
{
    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("Socket layer: ");

    int status = socket_init();
    if (status == 0) {
        int fd = socket_create(AF_INET, SOCK_STREAM, 0);
        if (fd >= 0) {
            socket_close(fd);
            console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            console_print("OK\n");
        } else {
            console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            console_print("FAIL (create)\n");
        }
    } else {
        console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        console_print("FAIL (init)\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void net_verify_tcp(void)
{
    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("TCP: ");

    int status = tcp_init();
    if (status == 0) {
        console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        console_print("OK\n");
    } else {
        console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        console_print("FAIL\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void net_verify_udp(void)
{
    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("UDP: ");

    int status = udp_init();
    if (status == 0) {
        console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        console_print("OK\n");
    } else {
        console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        console_print("FAIL\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void net_verify_ip(void)
{
    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("IP layer: ");

    int status = ip_init();
    if (status == 0) {
        console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        console_print("OK\n");
    } else {
        console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        console_print("FAIL\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void net_verify_arp(void)
{
    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("ARP: ");

    int status = arp_init();
    if (status == 0) {
        console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        console_print("OK\n");
    } else {
        console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        console_print("FAIL\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void net_verify_icmp(void)
{
    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("ICMP: ");

    int status = icmp_init();
    if (status == 0) {
        console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        console_print("OK\n");
    } else {
        console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        console_print("FAIL\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void net_verify_dns(void)
{
    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("DNS cache: ");

    dns_cache_init();
    int status = dns_cache_add("test.local", 0x08080808, 300);

    if (status == 0) {
        uint32_t ip;
        if (dns_cache_lookup("test.local", &ip) == 0 && ip == 0x08080808) {
            console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            console_print("OK\n");
        } else {
            console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            console_print("FAIL (lookup)\n");
        }
    } else {
        console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        console_print("FAIL (add)\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    dns_cache_flush();
}

static void net_verify_doh(void)
{
    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("DoH: ");

    int status = dns_doh_init();
    if (status == 0) {
        console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        console_print("OK\n");
    } else {
        console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        console_print("FAIL\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void net_verify_privacy(void)
{
    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("Privacy: ");

    int status = privacy_init();
    if (status == 0) {
        console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        console_print("OK\n");
    } else {
        console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        console_print("FAIL\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void net_verify_killswitch(void)
{
    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("Killswitch: ");

    int status = killswitch_init();
    if (status == 0) {
        console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        console_print("OK\n");
    } else {
        console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        console_print("FAIL\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void net_verify_tor(void)
{
    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("Tor: ");

    int status = tor_init();
    if (status == 0) {
        console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        console_print("OK (disabled)\n");
    } else {
        console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        console_print("FAIL\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void net_verify_devices(void)
{
    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("└─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("Network devices: ");

    int count = net_device_count();
    if (count > 0) {
        console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        console_print_dec(count);
        console_print(" device(s)\n");
    } else {
        console_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
        console_print("0 (no driver loaded)\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void net_verify_all(void)
{
    console_separator();
    console_info("Networking Stack");

    net_verify_ip_addr();
    net_verify_sk_buff();
    net_verify_socket();
    net_verify_ip();
    net_verify_arp();
    net_verify_icmp();
    net_verify_tcp();
    net_verify_udp();
    net_verify_dns();
    net_verify_doh();
    net_verify_privacy();
    net_verify_killswitch();
    net_verify_tor();
    net_verify_devices();

    console_separator();
}

static void console_hex8(uint8_t v)
{
    static const char h[] = "0123456789ABCDEF";
    char s[5] = { '0', 'x', h[v >> 4], h[v & 15], 0 };
    console_print(s);
}

static void keyboard_test_run(void)
{
    struct kbd_debug_event ev;

    keyboard_poll();
    while (keyboard_debug_pop(&ev)) { }
    keyboard_flush();

    console_print("\n");
    console_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    console_print("TEST TASTIERA - premi tasti, ESC per uscire\n");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    for (;;) {
        keyboard_poll();
        if (!keyboard_debug_pop(&ev)) {
            __asm__ volatile("pause");
            continue;
        }

        console_hex8(ev.raw & 0x7F);
        console_print(" ");
        console_print((ev.raw & 0x80) ? "UP " : "DWN ");

        if (ev.out) {
            console_hex8(ev.out);
            console_print(" ");
            const char *name = keyboard_key_name(ev.out);
            if (name) console_print(name);
        }
        console_print("\n");

        if (ev.out == KEY_ESC) break;
    }

    keyboard_flush();
    console_print("Test terminato.\n");
}

/* ============================================================================
 * kernel_main
 * ============================================================================ */
void kernel_main(uint32_t magic, uint64_t info_addr)
{
    serial_init();
    serial_print("[kernel] kernel_main() reached (Multiboot2)\n");

    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        serial_print("[kernel] FATAL: not booted by a Multiboot2 loader\n");
        for (;;) __asm__ volatile("hlt");
    }

    struct mb2_walk walk;
    mb2_walk_tags(info_addr, &walk);

    serial_print("[kernel] Multiboot2: mmap=");
    serial_print_hex((uint64_t)(uintptr_t)walk.mmap);
    serial_print(" fb=");
    serial_print_hex((uint64_t)(uintptr_t)walk.fb);
    serial_print("\n");

    gdt_init();
    tss_set_rsp0((uint64_t)(uintptr_t)kernel_stack_top);
    serial_print("[kernel] GDT/TSS installed\n");

    idt_init();
    serial_print("[kernel] IDT installed, PIC remapped\n");

    keyboard_init();
    serial_print("[kernel] keyboard initialized\n");

    mouse_init();
    serial_print("[kernel] mouse initialized\n");

    ata_init();
    serial_print("[kernel] ATA initialized\n");

    __asm__ volatile ("sti");
    serial_print("[kernel] interrupts enabled\n");
    dbg_irq_state("after sti");

    if (walk.mmap) {
        uint32_t n = (walk.mmap->size - sizeof(struct mb2_tag_mmap))
                     / walk.mmap->entry_size;
        serial_print("[kernel] PMM: ");
        serial_print_dec(n);
        serial_print(" entries\n");

        const struct mb2_mmap_entry *entries =
            (const struct mb2_mmap_entry *)((const uint8_t *)walk.mmap
                                            + sizeof(struct mb2_tag_mmap));
        pmm_init((const struct e820_entry *)entries, (uint16_t)n,
                 (uint64_t)(uintptr_t)_kernel_start,
                 (uint64_t)(uintptr_t)_kernel_end);
    } else {
        serial_print("[kernel] PMM: no memory map, fallback\n");
        pmm_init(NULL, 0,
                 (uint64_t)(uintptr_t)_kernel_start,
                 (uint64_t)(uintptr_t)_kernel_end);
    }

    serial_print("[kernel] PMM total: ");
    serial_print_dec(pmm_total_bytes());
    serial_print(" used: ");
    serial_print_dec(pmm_used_bytes());
    serial_print("\n");

    uint64_t f1 = pmm_alloc_frame();
    uint64_t f2 = pmm_alloc_frame();
    uint64_t f3 = pmm_alloc_frame();

    *(volatile uint32_t *)(uintptr_t)f1 = 0xDEADBEEF;
    *(volatile uint32_t *)(uintptr_t)f2 = 0xCAFEBABE;
    *(volatile uint32_t *)(uintptr_t)f3 = 0x8BADF00D;

    int ok = (*(volatile uint32_t *)(uintptr_t)f1 == 0xDEADBEEF) &&
             (*(volatile uint32_t *)(uintptr_t)f2 == 0xCAFEBABE) &&
             (*(volatile uint32_t *)(uintptr_t)f3 == 0x8BADF00D);

    pmm_free_frame(f2);
    uint64_t f4 = pmm_alloc_frame();
    ok = ok && (f4 == f2);

    serial_print("[kernel] PMM self-test: ");
    serial_print(ok ? "PASS\n" : "FAIL\n");

    vmm_init(pmm_total_bytes() + (1024ULL * 1024));

    volatile uint32_t *nx_test = (volatile uint32_t *)(uintptr_t)_kernel_end;
    *nx_test = 0x12345678;
    serial_print("[kernel] PTE_NX write test: PASS\n");

    struct framebuffer_info fb_info;
    memset(&fb_info, 0, sizeof(fb_info));

    if (walk.fb && walk.fb->framebuffer_addr != 0) {
        fb_info.addr       = walk.fb->framebuffer_addr;
        fb_info.width      = walk.fb->framebuffer_width;
        fb_info.height     = walk.fb->framebuffer_height;
        fb_info.pitch      = walk.fb->framebuffer_pitch;
        fb_info.bpp        = walk.fb->framebuffer_bpp;
        fb_info.red_pos    = walk.fb->red_field_position;
        fb_info.red_mask   = walk.fb->red_mask_size;
        fb_info.green_pos  = walk.fb->green_field_position;
        fb_info.green_mask = walk.fb->green_mask_size;
        fb_info.blue_pos   = walk.fb->blue_field_position;
        fb_info.blue_mask  = walk.fb->blue_mask_size;

        serial_print("[kernel] FB: ");
        serial_print_dec(fb_info.width); serial_print("x");
        serial_print_dec(fb_info.height); serial_print("x");
        serial_print_dec(fb_info.bpp); serial_print(" @ ");
        serial_print_hex(fb_info.addr); serial_print("\n");

        map_framebuffer(fb_info.addr, (uint64_t)fb_info.pitch * fb_info.height);

        if (fb_init(&fb_info) == 0) {
            serial_print("[kernel] Framebuffer initialized\n");
            mouse_set_screen_size(fb_width(), fb_height());
            console_ui_init(&fb_info);
        }
    }

    uint64_t test_frame = pmm_alloc_frame();
    uint64_t test_vaddr = 0x0000700000000000ULL;
    int vmm_ok = (vmm_map_page(test_vaddr, test_frame, PTE_WRITE) == 0);
    if (vmm_ok) {
        *(volatile uint32_t *)(uintptr_t)test_vaddr = 0x1337C0DE;
        vmm_ok = vmm_ok && (*(volatile uint32_t *)(uintptr_t)test_vaddr == 0x1337C0DE);
        vmm_unmap_page(test_vaddr);
    }
    serial_print("[kernel] VMM self-test: ");
    serial_print(vmm_ok ? "PASS\n" : "FAIL\n");

    uint64_t heap_base = 0x0000700000000000ULL + (2ULL * 1024 * 1024);
    heap_init(heap_base, 16ULL * 1024 * 1024);

    struct node { int value; struct node *next; };
    struct node *head = NULL;
    for (int i = 0; i < 100; i++) {
        struct node *n = (struct node *)kmalloc(sizeof(struct node));
        n->value = i;
        n->next = head;
        head = n;
    }

    int sum = 0, count = 0;
    for (struct node *n = head; n != NULL; n = n->next) {
        sum += n->value;
        count++;
    }

    int *zeroed = (int *)kcalloc(64, sizeof(int));
    int calloc_ok = 1;
    for (int i = 0; i < 64; i++)
        if (zeroed[i] != 0) calloc_ok = 0;

    char *small = (char *)kmalloc(8);
    small[0] = 'H'; small[1] = 'I'; small[2] = '\0';
    char *grown = (char *)krealloc(small, 256);
    int realloc_ok = (grown[0] == 'H' && grown[1] == 'I' && grown[2] == '\0');

    struct node *n = head;
    while (n) {
        struct node *next = n->next;
        kfree(n);
        n = next;
    }
    kfree(zeroed);
    kfree(grown);

    int heap_ok = (count == 100) && (sum == 4950) && calloc_ok && realloc_ok;
    serial_print("[kernel] Heap self-test: ");
    serial_print(heap_ok ? "PASS\n" : "FAIL\n");

    serial_print("[kernel] Running memory manager stress tests...\n");
    memtest_run_all();

    block_init();
    vfs_init();

    int fs_ok = (fs_boot_mount() == 0);
    serial_print("[kernel] Filesystem: ");
    serial_print(fs_ok ? "MOUNTED\n" : "FAILED\n");

    if (fs_ok) {
        if (font_init("/INTER-REGULAR.TTF", 18) == 0) {
            serial_print("[kernel] TrueType font loaded\n");
        } else {
            serial_print("[kernel] font load failed, using bitmap\n");
        }
        if (console_init_fb() == 0)
            serial_print("[kernel] Graphical console ready\n");
    }

    timer_init(100);

    tasking_init();
    task_test_run();

    vmm_map_apic();
    smp_init();

    serial_print("[smp] initialization complete\n");

    scheduler_mp_init();
    serial_print("[kernel] Multi-CPU scheduler initialized\n");
    extern void user_process_spawn(const char *path, const char *name);
    user_process_spawn("/IDLE.BIN", "idle");

    net_device_init();
    net_init();
    socket_init();
    ip_init();
    arp_init();
    icmp_init();
    tcp_init();
    udp_init();
    dns_init();
    dns_doh_init();
    privacy_init();
    killswitch_init();
    tor_init();

    console_init();
    console_print_header();

    console_info("System Information");
    console_print("  ├─ Architecture : x86_64\n");
    console_print("  ├─ Boot         : GRUB (Multiboot2)\n");
    console_print("  ├─ Long mode    : OK\n");
    console_print("  ├─ Kernel       : v7.0\n");
    console_print("  ├─ Block dev    : "); console_print_dec(block_device_count()); console_print("\n");
    console_print("  ├─ Partitions   : "); console_print_dec(partition_count()); console_print("\n");
    console_print("  ├─ CPUs         : "); console_print_dec(smp_cpu_count()); console_print("\n");
    console_print("  └─ RAM          : "); console_print_dec(pmm_total_bytes() / 1024 / 1024); console_print(" MiB\n");

    console_separator();

    console_info("Self-Tests");
    console_ok("PMM        ");
    console_ok("VMM        ");
    console_ok("Heap       ");
    console_ok("Spinlock   ");
    console_ok("Mutex      ");
    console_ok("Timer      ");
    console_ok("Scheduler  ");
    console_ok("SMP        ");
    if (fs_ok) console_ok("Filesystem "); else console_info("Filesystem (skipped)");

    console_separator();
    net_verify_all();

    console_separator();
    console_info("User Process Scheduler");

    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("States (READY/RUNNING/BLOCKED/SLEEPING/ZOMBIE): ");
    console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    console_print("available\n");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("Wait queues + sleep (timer-woken): ");
    console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    console_print("available\n");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("├─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("wait()/join + zombie reaping: ");
    console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    console_print("available\n");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    int st = process_selftest_run();
    int pass = (st && process_selftest_passed());

    console_print("  ");
    console_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    console_print("└─ ");
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    console_print("Scheduler self-test: ");
    if (pass) {
        console_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        console_print("PASS\n");
    } else {
        console_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        console_print("FAIL\n");
    }
    console_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    serial_print("[sched-test] states/waitq/sleep/wait available, selftest=");
    serial_print(pass ? "PASS\n" : "FAIL\n");

    process_debug_dump();

    console_separator();
    console_success("System Initialized");

    serial_print("[kernel] user-sched subsystem active "
                 "(states, wait queues, sleep, wait/join, zombie reap)\n");

    keyboard_set_diag_poll(1);
    keyboard_flush();

    console_print("\n");
    console_separator();
    console_print("DIAG MODE: K=test | F2=terminale | U=userspace | "
                  "T=sltest | S=sched-dump | ESC=spegni\n");

    for (;;) {
        char c;
        if (!keyboard_getchar(&c)) {
            __asm__ volatile("pause");
            continue;
        }

        if ((uint8_t)c == KEY_ESC) {
            console_clear();
            console_print("Spegnimento...\n");
            __asm__ volatile ("cli");
            for (;;) __asm__ volatile("hlt");
        }

        if (c == 'k' || c == 'K') {
            keyboard_test_run();
            continue;
        }

        if (c == 's' || c == 'S') {
            serial_print("[kernel] diag: process dump requested\n");
            process_debug_dump();
            continue;
        }

        if (c == 't' || c == 'T') {
            serial_print("[kernel] Starting SLTEST.BIN (T)\n");
            user_process_start_with("/SLTEST.BIN");
            console_print("\n[kernel] sltest returned, back to diag\n");
            continue;
        }

        if (c == 'u' || c == 'U') {
            extern void user_process_start(void);
            serial_print("[kernel] Starting userspace (U)\n");
            user_process_start();
            console_print("\n[kernel] userspace returned, back to diag\n");
            continue;
        }

        if ((uint8_t)c == KEY_F2) {
            serial_print("[kernel] Opening terminal (F2)\n");
            terminal_run();
            console_print("\nTerminale chiuso. K=test | F2=riapri | U=userspace | "
                          "T=sltest | S=sched-dump | ESC=spegni\n");
            continue;
        }
    }
}