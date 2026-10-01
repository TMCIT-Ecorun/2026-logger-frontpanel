#include <stdio.h>
#include "gui/platform.h"

static uint8_t g_brightness = 100;

static const GuiBluetoothDevice g_pc_devices[] = {
    {"Demo Phone", "10:20:30:40:50:60"},
    {"Demo Headset", "11:22:33:44:55:66"},
    {"Demo Car Audio", "22:33:44:55:66:77"},
};

static uint8_t pc_get_brightness(void) {
    return g_brightness;
}

static void pc_set_brightness(uint8_t percent) {
    g_brightness = percent;
    printf("[GUI] brightness=%u%%\n", percent);
}

static void pc_save_brightness(uint8_t percent) {
    printf("[GUI] save brightness=%u%%\n", percent);
}

static void pc_start_logging(void) {
    puts("[GUI] startLogging");
}

static void pc_stop_logging(void) {
    puts("[GUI] stopLogging");
}

static void pc_set_sample_rate(const char *channel_name, int rate_hz) {
    printf("[GUI] setConfig %s=%d Hz\n", channel_name, rate_hz);
}

static void pc_ptt_pressed(void) {
    puts("[GUI] PTT pressed");
}

static void pc_ptt_released(void) {
    puts("[GUI] PTT released");
}

static void pc_register_bluetooth_device(const char *name, const char *pin) {
    printf("[GUI] Bluetooth registration requested: name=%s pin=%s\n", name, pin);
}

static int pc_bluetooth_scan(GuiBluetoothDevice *devices, int max_devices) {
    int count = (int)(sizeof(g_pc_devices) / sizeof(g_pc_devices[0]));
    if (count > max_devices) count = max_devices;
    for (int i = 0; i < count; ++i) devices[i] = g_pc_devices[i];
    printf("[GUI] Bluetooth scan: %d device(s)\n", count);
    return count;
}

static bool pc_bluetooth_connect(const GuiBluetoothDevice *device, const char *pin) {
    printf("[GUI] Bluetooth connect: %s (%s) pin=%s\n", device->name, device->address, pin);
    return pin != NULL && pin[0] != '\0';
}

GuiPlatform pc_gui_platform(void) {
    return (GuiPlatform){
        .get_brightness = pc_get_brightness,
        .set_brightness = pc_set_brightness,
        .save_brightness = pc_save_brightness,
        .start_logging = pc_start_logging,
        .stop_logging = pc_stop_logging,
        .set_sample_rate = pc_set_sample_rate,
        .register_bluetooth_device = pc_register_bluetooth_device,
        .bluetooth_scan = pc_bluetooth_scan,
        .bluetooth_connect = pc_bluetooth_connect,
        .ptt_pressed = pc_ptt_pressed,
        .ptt_released = pc_ptt_released,
    };
}
