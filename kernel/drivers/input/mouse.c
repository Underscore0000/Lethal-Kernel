/*
 * Lethal Kernel - PS/2 mouse driver
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#include "mouse.h"
#include "../../arch/x86_64/io.h"
#include "../../core/irq.h"
#include "../../drivers/serial/serial.h"

#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_COMMAND 0x64

#define PS2_CMD_ENABLE_AUX   0xA8
#define PS2_CMD_READ_CONFIG  0x20
#define PS2_CMD_WRITE_CONFIG 0x60
#define PS2_CMD_SEND_TO_AUX  0xD4

#define MOUSE_CMD_GET_ID     0xF2
#define MOUSE_CMD_DEFAULTS   0xF6
#define MOUSE_CMD_ENABLE     0xF4
#define MOUSE_CMD_SET_RATE   0xF3
#define MOUSE_CMD_ACK        0xFA


#define PS2_AUX_READY_MASK   0x21


#define MOUSE_USE_IRQ 1


#define PACKET_SIZE 4

static int32_t  mouse_x = 512;
static int32_t  mouse_y = 384;
static int32_t  mouse_wheel = 0;
static uint8_t  mouse_buttons = 0;
static uint32_t screen_w = 1024;
static uint32_t screen_h = 768;

static uint8_t packet[PACKET_SIZE];
static int     packet_idx = 0;
static int     packet_expected = 3;  


static volatile uint32_t dbg_irq_count   = 0;
static volatile uint32_t dbg_irq_bytes   = 0;
static volatile uint32_t dbg_poll_calls  = 0;
static volatile uint32_t dbg_poll_bytes  = 0;
static volatile uint32_t dbg_resync      = 0;
static volatile uint32_t dbg_overflow    = 0;
static volatile uint32_t dbg_packets     = 0;
static volatile uint32_t dbg_kbd_pending = 0;
static volatile uint8_t  dbg_last_status = 0;
static volatile uint8_t  dbg_last_byte   = 0;
static volatile int32_t  dbg_last_dx     = 0;
static volatile int32_t  dbg_last_dy     = 0;
static volatile int32_t  dbg_last_dz     = 0;

static void ps2_wait_write(void)
{
    for (int i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & 0x02) == 0) return;
    }
    serial_print("[ps2] WARN: wait_write timeout\n");
}

static void ps2_wait_read(void)
{
    for (int i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS) & 0x01) return;
    }
    serial_print("[ps2] WARN: wait_read timeout\n");
}

static void ps2_send_command(uint8_t cmd)
{
    ps2_wait_write();
    outb(PS2_COMMAND, cmd);
}

static void mouse_send(uint8_t byte)
{
    ps2_send_command(PS2_CMD_SEND_TO_AUX);
    ps2_wait_write();
    outb(PS2_DATA, byte);
}

static uint8_t ps2_read_data(void)
{
    ps2_wait_read();
    return inb(PS2_DATA);
}

static void ps2_flush(void)
{
    int n = 0;
    for (int i = 0; i < 64 && (inb(PS2_STATUS) & 0x01); i++) {
        (void)inb(PS2_DATA);
        n++;
    }
    serial_print("[ps2] flushed bytes: ");
    serial_print_dec((uint64_t)n);
    serial_print("\n");
}


static void mouse_process_byte(uint8_t d)
{
    dbg_last_byte = d;

    if (packet_idx == 0 && !(d & 0x08)) {
        dbg_resync++;
        return;
    }

    packet[packet_idx++] = d;
    if (packet_idx < packet_expected) return;
    packet_idx = 0;

    if (packet[0] & 0xC0) {
        dbg_overflow++;
        return;
    }

    int32_t dx = (int8_t)packet[1];
    int32_t dy = (int8_t)packet[2];
    dbg_last_dx = dx;
    dbg_last_dy = dy;

    mouse_x += dx;
    mouse_y -= dy;

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if ((uint32_t)mouse_x >= screen_w) mouse_x = (int32_t)screen_w - 1;
    if ((uint32_t)mouse_y >= screen_h) mouse_y = (int32_t)screen_h - 1;

    mouse_buttons = packet[0] & 0x07;

    
    if (packet_expected == 4) {
        int8_t dz = (int8_t)packet[3];
        dbg_last_dz = dz;
        mouse_wheel += dz;
    }

    dbg_packets++;
}

static uint32_t mouse_drain(void)
{
    uint32_t n = 0;
    uint8_t status;

    for (int i = 0; i < 64; i++) {
        status = inb(PS2_STATUS);
        dbg_last_status = status;

        if (!(status & 0x01)) break;

        if ((status & PS2_AUX_READY_MASK) != PS2_AUX_READY_MASK) {
            dbg_kbd_pending++;
            break;
        }

        mouse_process_byte(inb(PS2_DATA));
        n++;
    }
    return n;
}

static void mouse_irq_handler(void *data)
{
    (void)data;
    dbg_irq_count++;
    dbg_irq_bytes += mouse_drain();
}

void mouse_poll(void)
{
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");

    dbg_poll_calls++;
    dbg_poll_bytes += mouse_drain();

    if (flags & 0x200) __asm__ volatile("sti" ::: "memory");
}


static int mouse_cmd_with_ack(uint8_t cmd)
{
    mouse_send(cmd);
    uint8_t ack = ps2_read_data();
    return (ack == MOUSE_CMD_ACK);
}


static int mouse_enable_wheel(void)
{
    serial_print("[mouse] enabling Intellimouse (wheel)...\n");

    if (!mouse_cmd_with_ack(MOUSE_CMD_SET_RATE)) return 0;
    mouse_send(200);
    if (ps2_read_data() != MOUSE_CMD_ACK) return 0;

    if (!mouse_cmd_with_ack(MOUSE_CMD_SET_RATE)) return 0;
    mouse_send(100);
    if (ps2_read_data() != MOUSE_CMD_ACK) return 0;

    if (!mouse_cmd_with_ack(MOUSE_CMD_SET_RATE)) return 0;
    mouse_send(80);
    if (ps2_read_data() != MOUSE_CMD_ACK) return 0;

    /* Chiedi l'ID: dovrebbe essere 0x03 per Intellimouse, 0x04 per Explorer */
    mouse_send(MOUSE_CMD_GET_ID);
    uint8_t ack_id = ps2_read_data();
    uint8_t dev_id = ps2_read_data();

    serial_print("[mouse] wheel get-id reply = ");
    serial_print_hex(ack_id);
    serial_print(" id=");
    serial_print_hex(dev_id);
    serial_print("\n");

    if (dev_id == 0x03) {
        packet_expected = 4;   /* Intellimouse: 4 byte, rotella */
        serial_print("[mouse] Intellimouse (wheel) enabled, 4-byte packets\n");
        return 1;
    }

    if (dev_id == 0x04) {
        packet_expected = 4;   /* Explorer: 4 byte, rotella + 2 tasti */
        serial_print("[mouse] Explorer mouse enabled, 4-byte packets\n");
        return 1;
    }


    packet_expected = 3;
    serial_print("[mouse] wheel not supported, 3-byte packets\n");
    return 0;
}

int mouse_init(void)
{
    serial_print("[mouse] initializing PS/2 mouse...\n");

    ps2_flush();

    ps2_send_command(PS2_CMD_ENABLE_AUX);

    ps2_send_command(PS2_CMD_READ_CONFIG);
    uint8_t config = ps2_read_data();
    serial_print("[mouse] config before = ");
    serial_print_hex(config);
    serial_print("\n");

    config |= 0x02;
    config &= ~0x20;
    ps2_send_command(PS2_CMD_WRITE_CONFIG);
    ps2_wait_write();
    outb(PS2_DATA, config);

    ps2_send_command(PS2_CMD_READ_CONFIG);
    uint8_t config2 = ps2_read_data();
    serial_print("[mouse] config after  = ");
    serial_print_hex(config2);
    serial_print("\n");

    mouse_send(MOUSE_CMD_DEFAULTS);
    uint8_t ack_def = ps2_read_data();
    serial_print("[mouse] set-defaults reply = ");
    serial_print_hex(ack_def);
    serial_print("\n");

    /* Prova ad abilitare la rotella. Se fallisce, resta in 3 byte. */
    mouse_enable_wheel();

    /* Enable streaming */
    mouse_send(MOUSE_CMD_ENABLE);
    uint8_t ack = ps2_read_data();
    if (ack != MOUSE_CMD_ACK) {
        serial_print("[mouse] WARNING: enable no ACK, got ");
        serial_print_hex(ack);
        serial_print("\n");
    } else {
        serial_print("[mouse] enable ACK received\n");
    }

    ps2_flush();
    packet_idx = 0;

#if MOUSE_USE_IRQ
    irq_register(12, mouse_irq_handler, NULL);
    serial_print("[mouse] IRQ12 handler registered\n");
#else
    serial_print("[mouse] IRQ disabled, polling only\n");
#endif

    serial_print("[pic] master mask=");
    serial_print_hex(inb(0x21));
    serial_print(" slave mask=");
    serial_print_hex(inb(0xA1));
    serial_print("\n");

    mouse_debug_pic();

    serial_print("[mouse] PS/2 mouse initialized (packet_size=");
    serial_print_dec((uint64_t)packet_expected);
    serial_print(")\n");
    return 0;
}

void mouse_debug_pic(void)
{
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");

    outb(0x20, 0x0B); uint8_t isr_m = inb(0x20);
    outb(0xA0, 0x0B); uint8_t isr_s = inb(0xA0);
    outb(0x20, 0x0A); uint8_t irr_m = inb(0x20);
    outb(0xA0, 0x0A); uint8_t irr_s = inb(0xA0);

    if (flags & 0x200) __asm__ volatile("sti" ::: "memory");

    serial_print("[pic] ISR m=");
    serial_print_hex(isr_m);
    serial_print(" s=");
    serial_print_hex(isr_s);
    serial_print(" | IRR m=");
    serial_print_hex(irr_m);
    serial_print(" s=");
    serial_print_hex(irr_s);
    serial_print("\n");
}

void mouse_debug_dump(void)
{
    serial_print("[mouse-dbg] irq=");        serial_print_dec(dbg_irq_count);
    serial_print(" irq_bytes=");             serial_print_dec(dbg_irq_bytes);
    serial_print(" poll_calls=");            serial_print_dec(dbg_poll_calls);
    serial_print(" poll_bytes=");            serial_print_dec(dbg_poll_bytes);
    serial_print(" packets=");               serial_print_dec(dbg_packets);
    serial_print(" resync=");                serial_print_dec(dbg_resync);
    serial_print(" ovf=");                   serial_print_dec(dbg_overflow);
    serial_print(" kbd_pending=");           serial_print_dec(dbg_kbd_pending);
    serial_print("\n[mouse-dbg] last_status="); serial_print_hex(dbg_last_status);
    serial_print(" last_byte=");             serial_print_hex(dbg_last_byte);
    serial_print(" last_dx=");               serial_print_dec((uint64_t)(int64_t)dbg_last_dx);
    serial_print(" last_dy=");               serial_print_dec((uint64_t)(int64_t)dbg_last_dy);
    serial_print(" last_dz=");               serial_print_dec((uint64_t)(int64_t)dbg_last_dz);
    serial_print(" x=");                     serial_print_dec((uint64_t)mouse_x);
    serial_print(" y=");                     serial_print_dec((uint64_t)mouse_y);
    serial_print(" wheel=");                 serial_print_dec((uint64_t)mouse_wheel);
    serial_print("\n");
}

int32_t mouse_get_x(void) { return mouse_x; }
int32_t mouse_get_y(void) { return mouse_y; }
int32_t mouse_get_wheel(void) { return mouse_wheel; }
uint8_t mouse_get_buttons(void) { return mouse_buttons; }
uint32_t mouse_get_irq_count(void) { return dbg_irq_count; }

void mouse_set_screen_size(uint32_t w, uint32_t h)
{
    screen_w = w;
    screen_h = h;
}