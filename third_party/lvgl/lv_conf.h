/*
 * Lethal Kernel - LVGL configuration
 */
#define LV_USE_LODEPNG 1
#ifndef LV_CONF_H
#define LV_CONF_H

/* Colori: 32 bpp (XRGB8888) */
#define LV_COLOR_DEPTH 32

/* Nessun OS */
#define LV_USE_OS 0

/* Memoria custom (kmalloc/kfree del kernel) */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CUSTOM
#define LV_USE_STDLIB_STRING LV_STDLIB_CUSTOM
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CUSTOM

/* Display: software rendering */
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_SUPPORT_XRGB8888 1
#define LV_DRAW_SW_SUPPORT_ARGB8888 1
#define LV_DRAW_SW_SUPPORT_RGB565 0

/* Font Montserrat */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Widget */
#define LV_USE_LABEL 1
#define LV_USE_BUTTON 1
#define LV_USE_BUTTONMATRIX 1
#define LV_USE_IMAGE 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_TEXTAREA 1
#define LV_USE_KEYBOARD 1
#define LV_USE_DROPDOWN 1
#define LV_USE_CHECKBOX 1
#define LV_USE_LIST 1
#define LV_USE_TABLE 1
#define LV_USE_CHART 1
#define LV_USE_BAR 1
#define LV_USE_ARC 1
#define LV_USE_SPINNER 1
#define LV_USE_ROLLER 1
#define LV_USE_CANVAS 1
#define LV_USE_MSGBOX 1
#define LV_USE_TABVIEW 1
#define LV_USE_TILEVIEW 1
#define LV_USE_WIN 1

/* Layout */
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/* Tema scuro */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

/* Logging: disabilitato (kernel freestanding) */
#define LV_USE_LOG 0

/* Assert: disabilitati */
#define LV_USE_ASSERT_NULL 0
#define LV_USE_ASSERT_MALLOC 0
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ 0

/* Refresh: 16 ms (~60 FPS) */
#define LV_DEF_REFR_PERIOD 16
#define LV_DPI_DEF 130

#endif /* LV_CONF_H */