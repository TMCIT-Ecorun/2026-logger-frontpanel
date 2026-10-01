#ifndef USB_DEBUG_H
#define USB_DEBUG_H

#include <stdbool.h>

void usb_debug_init(void);
void usb_debug_set_enabled(bool enabled);
void usb_debug_task(void);
bool usb_debug_switch_to_host(void);
void usb_debug_log(const char *message);
bool usb_debug_is_host(void);

#endif
