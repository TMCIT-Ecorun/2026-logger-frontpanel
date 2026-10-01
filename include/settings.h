#ifndef FRONTPANEL_SETTINGS_H
#define FRONTPANEL_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>
#include "touch_xpt2046.h"

typedef struct {
    uint8_t brightness;
    bool usb_debug_enabled;
    TouchCalibration touch;
} FrontPanelSettings;

void settings_set_defaults(FrontPanelSettings *settings);
bool settings_load(FrontPanelSettings *settings);
bool settings_save(const FrontPanelSettings *settings);

#endif /* FRONTPANEL_SETTINGS_H */
