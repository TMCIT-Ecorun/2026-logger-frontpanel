#include <stdio.h>
#include <stdint.h>
#include "lvgl.h"
#include "lvgl/drivers/sdl/lv_sdl_window.h"
#include "lvgl/drivers/sdl/lv_sdl_mouse.h"
#include "gui/ui.h"

GuiPlatform pc_gui_platform(void);

static uint32_t g_mock_seed = 0x6D2B79F5u;

static uint32_t mock_rand(void) {
    g_mock_seed ^= g_mock_seed << 13;
    g_mock_seed ^= g_mock_seed >> 17;
    g_mock_seed ^= g_mock_seed << 5;
    return g_mock_seed;
}

static void update_mock_telemetry(RaceCaptureTelemetry *data) {
    data->rpm = 1200.0f + (float)(mock_rand() % 6001u);
    data->speed = 10.0f + (float)(mock_rand() % 121u) * 0.5f;
    data->temp = 70.0f + (float)(mock_rand() % 451u) * 0.1f;
    data->volts = 12.0f + (float)(mock_rand() % 181u) * 0.01f;
}

int main(void) {
    lv_init();

    lv_display_t *display = lv_sdl_window_create(320, 240);
    if (display == NULL) {
        fprintf(stderr, "Failed to create the LVGL SDL window.\n");
        return 1;
    }
    lv_sdl_window_set_zoom(display, 2.0f);
    lv_sdl_mouse_create();

    GuiPlatform platform = pc_gui_platform();
    gui_init(lv_screen_active(), &platform);

    RaceCaptureTelemetry telemetry = {0};
    uint32_t last_update = 0;
    while (1) {
        uint32_t now = lv_tick_get();
        if ((uint32_t)(now - last_update) >= 250) {
            update_mock_telemetry(&telemetry);
            gui_update_telemetry(&telemetry);
            last_update = now;
        }
        lv_timer_handler();
        lv_delay_ms(1);
    }
}
