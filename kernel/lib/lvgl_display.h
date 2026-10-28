#ifndef LETHAL_LVGL_DISPLAY_H
#define LETHAL_LVGL_DISPLAY_H

#include "../../third_party/lvgl/lvgl.h"

int lvgl_display_init(void);
void lvgl_tick_inc(uint32_t ms);
void lvgl_handler(void);
lv_display_t *lvgl_get_display(void);

#endif