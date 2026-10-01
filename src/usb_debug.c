#include "usb_debug.h"

#include <stdio.h>
#include <string.h>

#include "pico/time.h"
#include "tusb.h"

// Private VID/PID pair used only for the temporary development CDC device.
// Replace these with an assigned VID/PID before shipping the product.
#define USB_DEBUG_VID 0xCAFE
#define USB_DEBUG_PID 0x4026

static bool s_host_mode;
static bool s_debug_enabled = true;
static char s_command_buffer[16];
static size_t s_command_length;
static uint32_t s_device_mode_started_ms;

#define USB_DEBUG_DEVICE_WINDOW_MS 10000

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT   0x02
#define EPNUM_CDC_IN    0x82
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

static tusb_desc_device_t const s_device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType   = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    // CDC is described by the interfaces below. Keep the device class at 0
    // (per-interface) so Linux does not treat this as a class-specific device
    // before it has parsed the CDC interface association.
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_DEBUG_VID,
    .idProduct          = USB_DEBUG_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer     = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

static uint8_t const s_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8,
                       EPNUM_CDC_OUT, EPNUM_CDC_IN, 64)
};

static char const *const s_string_descriptors[] = {
    (const char[]){0x09, 0x04},
    "Ecorun",
    "Logger Front Panel Debug",
    "DEBUG",
    "USB CDC"
};

static uint16_t s_string_descriptor[32];
uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&s_device_descriptor;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return s_configuration_descriptor;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;

    if (index >= sizeof(s_string_descriptors) / sizeof(s_string_descriptors[0])) {
        return NULL;
    }

    if (index == 0) {
        memcpy(s_string_descriptor, s_string_descriptors[0], 2);
        s_string_descriptor[0] = (TUSB_DESC_STRING << 8) | 4;
        return s_string_descriptor;
    }

    size_t len = strlen(s_string_descriptors[index]);
    if (len > 31) {
        len = 31;
    }

    s_string_descriptor[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 + len * 2));
    for (size_t i = 0; i < len; ++i) {
        s_string_descriptor[1 + i] = s_string_descriptors[index][i];
    }

    return s_string_descriptor;
}

static void usb_debug_write(const char *message) {
    if (!tud_cdc_connected()) {
        return;
    }

    tud_cdc_write_str(message);
    tud_cdc_write_flush();
}

void usb_debug_init(void) {
    tusb_rhport_init_t init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL
    };

    if (!tusb_init(0, &init)) {
        printf("USB: failed to initialize device stack\n");
        return;
    }

    s_host_mode = false;
    s_command_length = 0;
    s_device_mode_started_ms = to_ms_since_boot(get_absolute_time());
    if (!s_debug_enabled) {
        (void)usb_debug_switch_to_host();
        return;
    }
    printf("USB: CDC debug device mode (send HOST to switch to USB host)\n");
}

void usb_debug_set_enabled(bool enabled) {
    s_debug_enabled = enabled;
}

void usb_debug_task(void) {
    if (s_host_mode) {
        tuh_task();
        return;
    }

    tud_task();

    // Preserve the original behavior: give a PC a short window to open the
    // debug CDC port, then automatically hand the controller to USB host mode.
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if (!tud_cdc_connected() &&
        now_ms - s_device_mode_started_ms >= USB_DEBUG_DEVICE_WINDOW_MS) {
        (void)usb_debug_switch_to_host();
        return;
    }

    while (tud_cdc_available()) {
        uint8_t buf[64];
        uint32_t count = tud_cdc_read(buf, sizeof(buf));

        for (uint32_t i = 0; i < count; ++i) {
            char c = (char)buf[i];
            if (c == '\n' || c == '\r') {
                s_command_buffer[s_command_length] = '\0';
                if (strcmp(s_command_buffer, "HOST") == 0) {
                    (void)usb_debug_switch_to_host();
                    return;
                }
                if (s_command_length != 0) {
                    tud_cdc_write_str("USB: unknown command (use HOST)\r\n");
                }
                s_command_length = 0;
            } else if (s_command_length < sizeof(s_command_buffer) - 1) {
                s_command_buffer[s_command_length++] = c;
            } else {
                s_command_length = 0;
            }
        }
        tud_cdc_write_flush();
    }
}

bool usb_debug_switch_to_host(void) {
    if (s_host_mode) {
        return false;
    }

    if (!tud_deinit(0)) {
        printf("USB: failed to deinitialize device stack\n");
        return false;
    }

    // Let the host side observe the disconnect before enabling VBUS/host mode.
    sleep_ms(20);

    tusb_rhport_init_t init = {
        .role = TUSB_ROLE_HOST,
        .speed = TUSB_SPEED_FULL
    };

    if (!tusb_init(0, &init)) {
        printf("USB: failed to initialize host stack\n");
        return false;
    }

    s_host_mode = true;
    printf("USB: switched to host mode\n");
    return true;
}

bool usb_debug_is_host(void) {
    return s_host_mode;
}

void usb_debug_log(const char *message) {
    usb_debug_write(message);
    printf("%s", message);
}

void tud_mount_cb(void) {
    usb_debug_write("USB: CDC debug device mounted\r\n");
}

void tud_umount_cb(void) {
    // Do not write here: the USB controller may already be disconnected.
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    (void)itf;
    if (dtr || rts) {
        usb_debug_write("USB: CDC terminal connected\r\n");
    }
}

void tud_cdc_rx_cb(uint8_t itf) {
    (void)itf;
}
