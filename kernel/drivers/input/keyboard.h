#ifndef LETHAL_KEYBOARD_H
#define LETHAL_KEYBOARD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Layout: lascia commentato per US, decommenta per tastiera italiana */
/* #define KEYBOARD_LAYOUT_IT 1 */

/* Debug: stampa ogni scancode anche sulla seriale */
/* #define KEYBOARD_DEBUG 1 */

#define KEYBOARD_BUFFER_SIZE 256

/* Confronta sempre con il cast: (uint8_t)c == KEY_F2 */
#define KEY_ESC     0x1B
#define KEY_F1      0x81
#define KEY_F2      0x82
#define KEY_F3      0x83
#define KEY_F4      0x84
#define KEY_F5      0x85
#define KEY_F6      0x86
#define KEY_F7      0x87
#define KEY_F8      0x88
#define KEY_F9      0x89
#define KEY_F10     0x8A
#define KEY_F11     0x8B
#define KEY_F12     0x8C

#define KEY_UP      0x90
#define KEY_DOWN    0x91
#define KEY_LEFT    0x92
#define KEY_RIGHT   0x93
#define KEY_HOME    0x94
#define KEY_END     0x95
#define KEY_PGUP    0x96
#define KEY_PGDN    0x97
#define KEY_INSERT  0x98
#define KEY_DELETE  0x99

#define KMOD_SHIFT  0x01
#define KMOD_CTRL   0x02
#define KMOD_ALT    0x04
#define KMOD_ALTGR  0x08
#define KMOD_CAPS   0x10
#define KMOD_NUM    0x20

struct kbd_debug_event {
    uint8_t raw;   
    uint8_t ext;   
    uint8_t out;   
};

void     keyboard_init(void);
void     keyboard_irq_handler(void);
void     keyboard_poll(void);               
void     keyboard_set_diag_poll(int on);    
int      keyboard_has_data(void);
int      keyboard_getchar(char *out);       
char     keyboard_wait_char(void);
void     keyboard_flush(void);
uint32_t keyboard_dropped_chars(void);
uint8_t  keyboard_get_modifiers(void);

int keyboard_is_shift_pressed(void);
int keyboard_is_alt_pressed(void);
int keyboard_is_ctrl_pressed(void);

int         keyboard_debug_pop(struct kbd_debug_event *ev);
const char *keyboard_key_name(uint8_t code);

#ifdef __cplusplus
}
#endif

#endif