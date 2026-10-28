#ifndef LETHAL_SERIAL_H
#define LETHAL_SERIAL_H

#include <stddef.h>
#include <stdint.h>

typedef struct { 
    uint16_t base; 
    uint8_t initialized; 
} serial_port_t;

extern serial_port_t serial_com1;

void serial_init(void);
void serial_putc(char c);
void serial_print(const char *s);
void serial_print_dec(uint64_t v);
void serial_print_hex(uint64_t v);
void serial_print_hex8(uint8_t v);  
int serial_available(void);
int serial_read(void);
void serial_write(const uint8_t *buf, size_t len);

#endif