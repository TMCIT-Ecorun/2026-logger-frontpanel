#ifndef DISPLAY_ILI9341_H
#define DISPLAY_ILI9341_H

#include "lvgl.h"
#include <stdint.h>

// Function prototypes
void display_ili9341_init(void);
void display_ili9341_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
bool display_ili9341_set_speed(uint32_t hz);
uint32_t display_ili9341_get_speed(void);
void display_ili9341_set_brightness(uint8_t percent);
uint8_t display_ili9341_get_brightness(void);

#endif // DISPLAY_ILI9341_H
