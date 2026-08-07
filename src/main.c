#include <stdio.h>
#include <string.h>
#include <cJSON.h>
#include "ui.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "tusb.h"
#include "lvgl.h"
#include "structs.h"
#include "display_ili9341.h"
#include "touch_xpt2046.h"

/* Global / Static Buffers */
static RaceCaptureTelemetry g_telemetry = {0};
static char rx_line_buffer[512];
static size_t rx_line_pos = 0;
static bool g_telemetry_updated = false;

uint32_t tusb_time_millis_api(void) {
    return to_ms_since_boot(get_absolute_time());
}

/**
 * @brief Parse incoming JSON payload and update global telemetry state
 * @param json_str Null-terminated JSON string
 */
static void parse_telemetry_json(const char *json_str) {
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        /* Invalid or incomplete JSON; drop gracefully */
        return;
    }

    /* RaceCapture JSON structure: {"sample":{"RPM":3500, "Speed":45.2, ...}} */
    cJSON *sample = cJSON_GetObjectItemCaseSensitive(root, "sample");
    if (cJSON_IsObject(sample)) {
        cJSON *rpm   = cJSON_GetObjectItemCaseSensitive(sample, "RPM");
        cJSON *speed = cJSON_GetObjectItemCaseSensitive(sample, "Speed");
        cJSON *temp  = cJSON_GetObjectItemCaseSensitive(sample, "EngineTemp");
        cJSON *volts = cJSON_GetObjectItemCaseSensitive(sample, "Volts");

        if (cJSON_IsNumber(rpm))   g_telemetry.rpm   = (float)rpm->valuedouble;
        if (cJSON_IsNumber(speed)) g_telemetry.speed = (float)speed->valuedouble;
        if (cJSON_IsNumber(temp))  g_telemetry.temp  = (float)temp->valuedouble;
        if (cJSON_IsNumber(volts)) g_telemetry.volts = (float)volts->valuedouble;

        /* Set dirty flag for UI refresh */
        g_telemetry_updated = true;
    }

    /* Free memory allocated by cJSON */
    cJSON_Delete(root);
}

/**
 * @brief USB Host CDC Receive Handler (Assembles stream bytes into \r\n line packets)
 */
void process_usb_serial_rx(void) {
    uint8_t cdc_idx = 0; /* Assuming single USB CDC device connected */

    if (tuh_cdc_mounted(cdc_idx) && tuh_cdc_read_available(cdc_idx)) {
        uint8_t buf[64];
        uint32_t count = tuh_cdc_read(cdc_idx, buf, sizeof(buf));

        for (uint32_t i = 0; i < count; i++) {
            char c = (char)buf[i];

            if (c == '\n' || c == '\r') {
                if (rx_line_pos > 0) {
                    rx_line_buffer[rx_line_pos] = '\0';
                    parse_telemetry_json(rx_line_buffer);
                    rx_line_pos = 0; /* Reset line buffer */
                }
            } else {
                if (rx_line_pos < sizeof(rx_line_buffer) - 1) {
                    rx_line_buffer[rx_line_pos++] = c;
                } else {
                    /* Buffer overflow protection; discard line */
                    rx_line_pos = 0;
                }
            }
        }
    }
}

int main(void) {
    stdio_init_all();

    /* Initialize TinyUSB Host Stack */
    tusb_init();

    /* Initialize LVGL Library & Display Driver */
    display_ili9341_init();
    touch_xpt2046_init();
    lv_init();

    // 1. Register Display Driver
    lv_display_t *disp = lv_display_create(320, 240);

    // Allocate partial render buffer (20 lines buffer)
    static uint8_t buf[320 * 20 * 2];
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, display_ili9341_flush_cb);

    // 2. Register Touchpad Input Driver
    lv_indev_t *touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, touch_xpt2046_read_cb);

    /* Build the Tabview Dashboard UI */
    create_ui_tabview(lv_scr_act());

    uint32_t last_ui_update_ms = 0;

    while (1) {
        /* 1. Service TinyUSB Host Events */
        tuh_task();

        /* 2. Process Incoming USB CDC Serial Stream */
        process_usb_serial_rx();

        /* 3. Service LVGL Internal Timers (~5ms interval) */
        lv_timer_handler();

        /* 4. Refresh Dashboard UI at capped rate (~20Hz / 50ms) */
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (g_telemetry_updated && (now - last_ui_update_ms >= 50)) {
            update_ui_telemetry(&g_telemetry);
            g_telemetry_updated = false;
            last_ui_update_ms = now;
        }

        sleep_ms(1);
    }

    return 0;
}
