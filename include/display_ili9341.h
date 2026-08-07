#ifndef DISPLAY_ILI9341_H
#define DISPLAY_ILI9341_H

#include "lvgl.h"

// Function prototypes
void display_ili9341_init(void);
void display_ili9341_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

#endif // DISPLAY_ILI9341_H
