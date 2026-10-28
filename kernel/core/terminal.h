#ifndef LETHAL_TERMINAL_H
#define LETHAL_TERMINAL_H
#include "../drivers/video/framebuffer.h"
void terminal_set_fb(const struct framebuffer_info *info);
void terminal_run(void);
#endif