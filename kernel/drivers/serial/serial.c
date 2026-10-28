#include <stddef.h>
#include "serial.h"
#include "../../../arch/x86_64/io.h"

#define UART_RBR 0
#define UART_THR 0
#define UART_IER 1
#define UART_IIR 2
#define UART_FCR 2
#define UART_LCR 3
#define UART_MCR 4
#define UART_LSR 5
#define COM1 0x3F8

serial_port_t serial_com1 = { COM1, 0 };

static int tx_empty(void)
{ 
    return (inb(COM1 + UART_LSR) & 0x20) != 0; 
}

void serial_init(void)
{ 
    outb(COM1 + UART_IER, 0); 
    outb(COM1 + UART_LCR, 0x80); 
    outb(COM1 + 0, 3); 
    outb(COM1 + 1, 0); 
    outb(COM1 + UART_LCR, 3); 
    outb(COM1 + UART_FCR, 0xC7); 
    outb(COM1 + UART_MCR, 0x0B); 
    serial_com1.initialized = 1; 
}

int serial_available(void)
{ 
    return (inb(COM1 + UART_LSR) & 1) != 0; 
}

int serial_read(void)
{ 
    return serial_available() ? inb(COM1 + UART_RBR) : -1; 
}

void serial_putc(char c)
{ 
    while (!tx_empty()) {} 
    outb(COM1 + UART_THR, (uint8_t)c); 
}

void serial_print(const char *s)
{ 
    while (*s) { 
        if (*s == '\n') serial_putc('\r'); 
        serial_putc(*s++); 
    } 
}

static void dec(uint64_t v, char *b)
{
    char t[21];
    size_t i = 0, j = 0;
    if (!v) t[i++] = '0';
    while (v) {
        t[i++] = (char)('0' + v % 10);
        v /= 10;
    }
    while (i) b[j++] = t[--i];
    b[j] = 0;
}

static void hex(uint64_t v, char *b)
{
    static const char *h = "0123456789ABCDEF";
    b[0] = '0';
    b[1] = 'x';
    for (int i = 0; i < 16; i++)
        b[2 + i] = h[(v >> ((15 - i) * 4)) & 15];
    b[18] = 0;
}

void serial_print_dec(uint64_t v)
{
    char b[21];
    dec(v, b);
    serial_print(b);
}

void serial_print_hex(uint64_t v)
{
    char b[19];
    hex(v, b);
    serial_print(b);
}


void serial_print_hex8(uint8_t v)
{
    static const char *h = "0123456789ABCDEF";
    char b[3];
    b[0] = h[(v >> 4) & 0xF];
    b[1] = h[v & 0xF];
    b[2] = '\0';
    serial_print(b);
}

void serial_write(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++)
        serial_putc((char)buf[i]);
}