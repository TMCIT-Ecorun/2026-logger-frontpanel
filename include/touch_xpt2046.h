#ifndef TOUCH_XPT2046_H
#define TOUCH_XPT2046_H

#include "lvgl.h"

// Function prototypes
void touch_xpt2046_init(void);
void touch_xpt2046_read_cb(lv_indev_t *indev, lv_indev_data_t *data);

#endif // TOUCH_XPT2046_H
