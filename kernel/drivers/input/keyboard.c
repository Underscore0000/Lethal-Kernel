#include "keyboard.h"
#include "../../../arch/x86_64/io.h"

#ifdef KEYBOARD_DEBUG
#include "../serial/serial.h"
#endif

#define KBD_DATA   0x60
#define KBD_STATUS 0x64
#define KBD_CMD    0x64

#define ST_OUT_FULL 0x01
#define ST_IN_FULL  0x02
#define ST_AUX_DATA 0x20

static volatile char     buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint16_t head, tail;
static volatile uint32_t dropped;

static uint8_t lshift, rshift, lctrl, rctrl, lalt, altgr;
static uint8_t caps, numlock;
static uint8_t extended;
static uint8_t pause_skip;
static uint8_t lock_latch;
static volatile uint8_t diag_poll;

#define LATCH_CAPS 0x01
#define LATCH_NUM  0x02

#define DBG_SIZE 64
static volatile struct kbd_debug_event dbg_buf[DBG_SIZE];
static volatile uint8_t dbg_head, dbg_tail;
static uint8_t cur_out;

#ifdef KEYBOARD_LAYOUT_IT
static const unsigned char normal[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0', '\'', 0xEC,
    '\b', '\t', 'q','w','e','r','t','y','u','i','o','p', 0xE8, '+',
    '\n', 0, 'a','s','d','f','g','h','j','k','l', 0xF2, 0xE0, '\\',
    0, 0xF9, 'z','x','c','v','b','n','m', ',', '.', '-', 0, '*', 0, ' ',
    [0x56] = '<'
};
static const unsigned char shifted[128] = {
    0, 27, '!','"',0xA3,'$','%','&','/','(',')','=', '?', '^',
    '\b', '\t', 'Q','W','E','R','T','Y','U','I','O','P', 0xE9, '*',
    '\n', 0, 'A','S','D','F','G','H','J','K','L', 0xE7, 0xB0, '|',
    0, 0xA7, 'Z','X','C','V','B','N','M', ';', ':', '_', 0, '*', 0, ' ',
    [0x56] = '>'
};
static unsigned char altgr_char(uint8_t sc, int shift)
{
    switch (sc) {
        case 0x27: return '@';
        case 0x28: return '#';
        case 0x1A: return shift ? '{' : '[';
        case 0x1B: return shift ? '}' : ']';
        case 0x0D: return '~';
        case 0x0C: return '`';
        default:   return 0;
    }
}
#else
static const unsigned char normal[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=',
    '\b', '\t', 'q','w','e','r','t','y','u','i','o','p','[',']',
    '\n', 0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\', 'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
    [0x56] = '\\'
};
static const unsigned char shifted[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+',
    '\b', '\t', 'Q','W','E','R','T','Y','U','I','O','P','{','}',
    '\n', 0, 'A','S','D','F','G','H','J','K','L',':','"','~',
    0, '|', 'Z','X','C','V','B','N','M','<','>','?', 0, '*', 0, ' ',
    [0x56] = '|'
};
static unsigned char altgr_char(uint8_t sc, int shift)
{
    (void)sc; (void)shift;
    return 0;
}
#endif

static const unsigned char keypad_num[13] = {
    '7','8','9','-','4','5','6','+','1','2','3','0','.'
};
static const unsigned char keypad_nav[13] = {
    KEY_HOME, KEY_UP, KEY_PGUP, '-', KEY_LEFT, 0, KEY_RIGHT, '+',
    KEY_END, KEY_DOWN, KEY_PGDN, KEY_INSERT, KEY_DELETE
};

static void push(unsigned char c)
{
    cur_out = c;
    uint16_t n = (uint16_t)((head + 1) % KEYBOARD_BUFFER_SIZE);
    if (n == tail) { dropped++; return; }
    buffer[head] = (char)c;
    head = n;
}

static int wait_in_clear(void)
{
    for (int i = 0; i < 100000; i++)
        if (!(inb(KBD_STATUS) & ST_IN_FULL)) return 1;
    return 0;
}

static int wait_out_full(void)
{
    for (int i = 0; i < 100000; i++)
        if (inb(KBD_STATUS) & ST_OUT_FULL) return 1;
    return 0;
}

void keyboard_init(void)
{
    head = tail = 0;
    dropped = 0;
    lshift = rshift = lctrl = rctrl = lalt = altgr = 0;
    caps = 0;
    numlock = 1;
    extended = 0;
    pause_skip = 0;
    lock_latch = 0;
    diag_poll = 0;
    dbg_head = dbg_tail = 0;
    cur_out = 0;

    for (int i = 0; i < 32 && (inb(KBD_STATUS) & ST_OUT_FULL); i++)
        (void)inb(KBD_DATA);

    if (wait_in_clear()) {
        outb(KBD_CMD, 0x20);
        if (wait_out_full()) {
            uint8_t cfg = inb(KBD_DATA);
            cfg |= 0x01;               
            cfg |= 0x40;               
            cfg &= (uint8_t)~0x10;     
            if (wait_in_clear()) {
                outb(KBD_CMD, 0x60);
                if (wait_in_clear())
                    outb(KBD_DATA, cfg);
            }
        }
    }

    if (wait_in_clear())
        outb(KBD_CMD, 0xAE);
}

static int is_letter_key(uint8_t sc)
{
    return normal[sc] >= 'a' && normal[sc] <= 'z';
}

static void decode_scancode(uint8_t raw)
{
    if (pause_skip) { pause_skip--; return; }
    if (raw == 0xE1) { pause_skip = 5; return; }
    if (raw == 0xE0) { extended = 1; return; }

    if (raw == 0xFA || raw == 0xAA || raw == 0xFE ||
        raw == 0xFF || raw == 0x00) {
        extended = 0;
        return;
    }

    uint8_t release = raw & 0x80;
    uint8_t sc = raw & 0x7F;
    uint8_t ext = extended;
    extended = 0;

    if (ext && (sc == 0x2A || sc == 0x36)) return;

    switch (sc) {
        case 0x2A: if (!ext) { lshift = !release; return; } break;
        case 0x36: if (!ext) { rshift = !release; return; } break;
        case 0x1D: if (ext) rctrl = !release; else lctrl = !release; return;
        case 0x38: if (ext) altgr = !release; else lalt = !release; return;
        case 0x3A:
            if (release) lock_latch &= (uint8_t)~LATCH_CAPS;
            else if (!(lock_latch & LATCH_CAPS)) {
                lock_latch |= LATCH_CAPS;
                caps ^= 1;
            }
            return;
        case 0x45:
            if (!ext) {
                if (release) lock_latch &= (uint8_t)~LATCH_NUM;
                else if (!(lock_latch & LATCH_NUM)) {
                    lock_latch |= LATCH_NUM;
                    numlock ^= 1;
                }
                return;
            }
            break;
        default: break;
    }

    if (release) return;

    int shift = lshift || rshift;
    int ctrl  = lctrl || rctrl;

    if (!ext) {
        switch (sc) {
            case 0x3B: push(KEY_F1);  return;
            case 0x3C: push(KEY_F2);  return;
            case 0x3D: push(KEY_F3);  return;
            case 0x3E: push(KEY_F4);  return;
            case 0x3F: push(KEY_F5);  return;
            case 0x40: push(KEY_F6);  return;
            case 0x41: push(KEY_F7);  return;
            case 0x42: push(KEY_F8);  return;
            case 0x43: push(KEY_F9);  return;
            case 0x44: push(KEY_F10); return;
            case 0x57: push(KEY_F11); return;
            case 0x58: push(KEY_F12); return;
            case 0x01: push(KEY_ESC); return;
            default: break;
        }
    }

    if (ext) {
        switch (sc) {
            case 0x48: push(KEY_UP);     return;
            case 0x50: push(KEY_DOWN);   return;
            case 0x4B: push(KEY_LEFT);   return;
            case 0x4D: push(KEY_RIGHT);  return;
            case 0x47: push(KEY_HOME);   return;
            case 0x4F: push(KEY_END);    return;
            case 0x49: push(KEY_PGUP);   return;
            case 0x51: push(KEY_PGDN);   return;
            case 0x52: push(KEY_INSERT); return;
            case 0x53: push(KEY_DELETE); return;
            case 0x1C: push('\n');       return;
            case 0x35: push('/');        return;
            default:   return;
        }
    }

    if (sc >= 0x47 && sc <= 0x53 && sc != 0x4A && sc != 0x4E) {
        unsigned char k = (numlock && !shift) ? keypad_num[sc - 0x47]
                                              : keypad_nav[sc - 0x47];
        if (k) push(k);
        return;
    }
    if (sc == 0x4A) { push('-'); return; }
    if (sc == 0x4E) { push('+'); return; }

    unsigned char c = 0;

    if (altgr) {
        c = altgr_char(sc, shift);
        if (c) { push(c); return; }
    }

    if (is_letter_key(sc)) {
        int upper = shift ^ caps;
        c = upper ? shifted[sc] : normal[sc];
    } else {
        c = shift ? shifted[sc] : normal[sc];
    }

    if (!c) return;

    if (ctrl) {
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 1);
        else if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 1);
    }

    push(c);
}

static void process_scancode(uint8_t raw)
{
    uint8_t was_ext = extended;
    cur_out = 0;

    decode_scancode(raw);

    if (raw == 0xE0 || raw == 0xE1) return;

#ifdef KEYBOARD_DEBUG
    serial_print("[kbd] raw=");
    serial_print_hex(raw);
    serial_print(" ext=");
    serial_print_dec(was_ext);
    serial_print(" out=");
    serial_print_hex(cur_out);
    serial_print("\n");
#endif

    uint8_t n = (uint8_t)((dbg_head + 1) % DBG_SIZE);
    if (n != dbg_tail) {
        dbg_buf[dbg_head].raw = raw;
        dbg_buf[dbg_head].ext = was_ext;
        dbg_buf[dbg_head].out = cur_out;
        dbg_head = n;
    }
}


static void drain(int discard_aux)
{
    for (int guard = 0; guard < 32; guard++) {
        uint8_t st = inb(KBD_STATUS);
        if (!(st & ST_OUT_FULL)) break;
        if (st & ST_AUX_DATA) {
            if (discard_aux) { (void)inb(KBD_DATA); continue; }
            break;
        }
        process_scancode(inb(KBD_DATA));
    }
}

void keyboard_irq_handler(void)
{
    drain(0);
}

void keyboard_poll(void)
{
    uint64_t fl;
    __asm__ volatile ("pushfq; pop %0; cli" : "=r"(fl) :: "memory");
    drain(diag_poll);
    if (fl & 0x200)
        __asm__ volatile ("sti" ::: "memory");
}

void keyboard_set_diag_poll(int on)
{
    diag_poll = on ? 1 : 0;
}

int keyboard_has_data(void)
{
    if (head == tail) keyboard_poll();
    return head != tail;
}

int keyboard_getchar(char *out)
{
    if (!out) return 0;
    if (head == tail) keyboard_poll();
    if (head == tail) return 0;
    *out = buffer[tail];
    tail = (uint16_t)((tail + 1) % KEYBOARD_BUFFER_SIZE);
    return 1;
}

char keyboard_wait_char(void)
{
    char c;
    while (!keyboard_getchar(&c))
        __asm__ volatile("pause");
    return c;
}

void keyboard_flush(void)
{
    tail = head;
}

uint32_t keyboard_dropped_chars(void) { return dropped; }

uint8_t keyboard_get_modifiers(void)
{
    uint8_t m = 0;
    if (lshift || rshift) m |= KMOD_SHIFT;
    if (lctrl || rctrl)   m |= KMOD_CTRL;
    if (lalt)             m |= KMOD_ALT;
    if (altgr)            m |= KMOD_ALTGR;
    if (caps)             m |= KMOD_CAPS;
    if (numlock)          m |= KMOD_NUM;
    return m;
}

int keyboard_is_shift_pressed(void) { return lshift || rshift; }
int keyboard_is_alt_pressed(void)   { return lalt || altgr; }
int keyboard_is_ctrl_pressed(void)  { return lctrl || rctrl; }

int keyboard_debug_pop(struct kbd_debug_event *ev)
{
    if (!ev || dbg_head == dbg_tail) return 0;
    ev->raw = dbg_buf[dbg_tail].raw;
    ev->ext = dbg_buf[dbg_tail].ext;
    ev->out = dbg_buf[dbg_tail].out;
    dbg_tail = (uint8_t)((dbg_tail + 1) % DBG_SIZE);
    return 1;
}

const char *keyboard_key_name(uint8_t c)
{
    switch (c) {
        case KEY_ESC:    return "ESC";
        case KEY_F1:     return "F1";
        case KEY_F2:     return "F2";
        case KEY_F3:     return "F3";
        case KEY_F4:     return "F4";
        case KEY_F5:     return "F5";
        case KEY_F6:     return "F6";
        case KEY_F7:     return "F7";
        case KEY_F8:     return "F8";
        case KEY_F9:     return "F9";
        case KEY_F10:    return "F10";
        case KEY_F11:    return "F11";
        case KEY_F12:    return "F12";
        case KEY_UP:     return "UP";
        case KEY_DOWN:   return "DOWN";
        case KEY_LEFT:   return "LEFT";
        case KEY_RIGHT:  return "RIGHT";
        case KEY_HOME:   return "HOME";
        case KEY_END:    return "END";
        case KEY_PGUP:   return "PGUP";
        case KEY_PGDN:   return "PGDN";
        case KEY_INSERT: return "INS";
        case KEY_DELETE: return "DEL";
        case '\n':       return "ENTER";
        case '\b':       return "BACKSPACE";
        case '\t':       return "TAB";
        case ' ':        return "SPACE";
        default:         return 0;
    }
}