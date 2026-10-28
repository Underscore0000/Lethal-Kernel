/*
 * Lethal Kernel - PS/2 mouse driver
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
#ifndef LETHAL_DRIVERS_INPUT_MOUSE_H
#define LETHAL_DRIVERS_INPUT_MOUSE_H

#include <stdint.h>


int mouse_init(void);


int32_t mouse_get_x(void);
int32_t mouse_get_y(void);


int32_t mouse_get_wheel(void);


uint8_t mouse_get_buttons(void);


void mouse_set_screen_size(uint32_t w, uint32_t h);


void mouse_poll(void);

/* Debug */
uint32_t mouse_get_irq_count(void);
void mouse_debug_pic(void);
void mouse_debug_dump(void);

#endif /* LETHAL_DRIVERS_INPUT_MOUSE_H */