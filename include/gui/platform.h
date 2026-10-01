#ifndef FRONTPANEL_GUI_PLATFORM_H
#define FRONTPANEL_GUI_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char name[64];
    char address[18];
} GuiBluetoothDevice;

typedef struct {
    uint8_t (*get_brightness)(void);
    void (*set_brightness)(uint8_t percent);
    void (*save_brightness)(uint8_t percent);
    void (*start_logging)(void);
    void (*stop_logging)(void);
    void (*set_sample_rate)(const char *channel_name, int rate_hz);
    void (*register_bluetooth_device)(const char *name, const char *pin);
    int (*bluetooth_scan)(GuiBluetoothDevice *devices, int max_devices);
    bool (*bluetooth_connect)(const GuiBluetoothDevice *device, const char *pin);
    void (*ptt_pressed)(void);
    void (*ptt_released)(void);
    bool (*get_usb_debug_enabled)(void);
    void (*set_usb_debug_enabled)(bool enabled);
    bool (*run_touch_calibration)(void);
} GuiPlatform;

#endif /* FRONTPANEL_GUI_PLATFORM_H */
