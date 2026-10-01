#ifndef TOUCH_XPT2046_H
#define TOUCH_XPT2046_H

#include "lvgl.h"

typedef struct {
    uint16_t raw_x_min;
    uint16_t raw_x_max;
    uint16_t raw_y_min;
    uint16_t raw_y_max;
} TouchCalibration;

// Function prototypes
void touch_xpt2046_init(void);
void touch_xpt2046_read_cb(lv_indev_t *indev, lv_indev_data_t *data);
void touch_xpt2046_set_default_calibration(TouchCalibration *calibration);
void touch_xpt2046_set_calibration(const TouchCalibration *calibration);
void touch_xpt2046_get_calibration(TouchCalibration *calibration);
bool touch_xpt2046_read_raw(uint16_t *raw_x, uint16_t *raw_y, uint32_t *pressure);
bool touch_xpt2046_run_calibration(void);
bool touch_xpt2046_start_calibration(void);
bool touch_xpt2046_calibration_active(void);
bool touch_xpt2046_calibration_task(void);

#endif // TOUCH_XPT2046_H
