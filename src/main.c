#include <stdio.h>
#include <string.h>
#include <cJSON.h>
#include "gui/ui.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/cyw43_arch.h"
#include "tusb.h"
#include "bsp/board_api.h"
#include "lvgl.h"
#include "structs.h"
#include "display_ili9341.h"
#include "touch_xpt2046.h"
#include "usb_debug.h"
#include "command.h"
#include "settings.h"
#include "pins.h"
#include "sim7070.h"
#include "websocket_client.h"
#include "hfp_client.h"

static void gui_platform_set_brightness(uint8_t percent) {
    display_ili9341_set_brightness(percent);
}

static uint8_t gui_platform_get_brightness(void) {
    return display_ili9341_get_brightness();
}

static FrontPanelSettings g_settings;

static void gui_save_brightness(uint8_t percent) {
    if (percent == g_settings.brightness) {
        return;
    }
    FrontPanelSettings next = g_settings;
    next.brightness = percent;
    if (settings_save(&next)) {
        g_settings = next;
    } else {
        g_settings.brightness = percent;
    }
}

static bool gui_get_usb_debug_enabled(void) {
    return g_settings.usb_debug_enabled;
}

static void gui_set_usb_debug_enabled(bool enabled) {
    if (enabled == g_settings.usb_debug_enabled) return;
    FrontPanelSettings next = g_settings;
    next.usb_debug_enabled = enabled;
    if (settings_save(&next)) {
        g_settings = next;
        usb_debug_set_enabled(enabled);
    }
}

static bool gui_run_touch_calibration(void) {
    return touch_xpt2046_start_calibration();
}

static void gui_ptt_pressed(void) {
    /* The PTT UI is ready now; audio capture/encode is attached here once the
     * Bluetooth microphone/speaker path is selected. */
    usb_debug_log("[PTT] pressed\r\n");
}

static void gui_ptt_released(void) {
    usb_debug_log("[PTT] released\r\n");
}

static void gui_register_bluetooth_device(const char *name, const char *pin) {
    /* UI-only registration hook for now. HFP pairing/connection is added
     * separately; Opus/audio transport is intentionally not involved here. */
    usb_debug_log("[BT] registration requested\r\n");
    (void)name;
    (void)pin;
}

static int gui_bluetooth_scan(GuiBluetoothDevice *devices, int max_devices) {
    return hfp_client_scan(devices, max_devices);
}

static bool gui_bluetooth_connect(const GuiBluetoothDevice *device, const char *pin) {
    (void)pin;
    if (device == NULL) return false;
    bool ok = hfp_client_connect(device->address);
    if (ok && pin != NULL && pin[0] != '\0') {
        usb_debug_log("[BT] PIN supplied; Classic pairing policy is handled by BTstack\r\n");
    }
    return ok;
}

static const GuiPlatform g_gui_platform = {
    .get_brightness = gui_platform_get_brightness,
    .set_brightness = gui_platform_set_brightness,
    .save_brightness = gui_save_brightness,
    .start_logging = cmd_start_logging,
    .stop_logging = cmd_stop_logging,
    .set_sample_rate = cmd_set_sample_rate,
    .register_bluetooth_device = gui_register_bluetooth_device,
    .bluetooth_scan = gui_bluetooth_scan,
    .bluetooth_connect = gui_bluetooth_connect,
    .ptt_pressed = gui_ptt_pressed,
    .ptt_released = gui_ptt_released,
    .get_usb_debug_enabled = gui_get_usb_debug_enabled,
    .set_usb_debug_enabled = gui_set_usb_debug_enabled,
    .run_touch_calibration = gui_run_touch_calibration,
};

/* Global / Static Buffers */
static RaceCaptureTelemetry g_telemetry = {0};
static char rx_line_buffer[512];
static size_t rx_line_pos = 0;
static bool g_telemetry_updated = false;
static uint32_t g_mock_seed = 0x6D2B79F5u;
static void parse_telemetry_json(const char *json_str);

static void reset_settings_to_defaults(void) {
    FrontPanelSettings defaults;
    settings_set_defaults(&defaults);

    if (settings_save(&defaults)) {
        g_settings = defaults;
        touch_xpt2046_set_calibration(&g_settings.touch);
        display_ili9341_set_brightness(g_settings.brightness);
        usb_debug_log("[BOOTSEL] settings initialized; release BOOTSEL to enter USB bootloader\r\n");
    } else {
        usb_debug_log("[BOOTSEL] failed to save initialized settings\r\n");
    }
}

static void handle_bootsel_reset(void) {
    if (!board_button_read()) {
        return;
    }

    reset_settings_to_defaults();
    usb_debug_log("[BOOTSEL] waiting for button release...\r\n");
    sleep_ms(100);
    while (board_button_read()) {
        sleep_ms(20);
    }
    sleep_ms(50);
    usb_debug_log("[BOOTSEL] entering USB bootloader\r\n");
    sleep_ms(100);
    reset_usb_boot(0, 0);
}

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

void parse_telemetry_json_from_websocket(const char *json) {
    parse_telemetry_json(json);
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

static uint32_t mock_rand(void) {
    // Small deterministic PRNG; no extra SDK dependency is needed.
    g_mock_seed ^= g_mock_seed << 13;
    g_mock_seed ^= g_mock_seed >> 17;
    g_mock_seed ^= g_mock_seed << 5;
    return g_mock_seed;
}

static void update_mock_telemetry(void) {
    g_telemetry.rpm = 1200.0f + (float)(mock_rand() % 6001u);
    g_telemetry.speed = 10.0f + (float)(mock_rand() % 121u) * 0.5f;
    g_telemetry.temp = 70.0f + (float)(mock_rand() % 451u) * 0.1f;
    g_telemetry.volts = 12.0f + (float)(mock_rand() % 181u) * 0.01f;
    g_telemetry_updated = true;
}

static void sim7070_power_key_startup(void) {
    gpio_init(SIM7070_PWRKEY_PIN);
    gpio_set_dir(SIM7070_PWRKEY_PIN, GPIO_OUT);
    /* PWRKEY is active-low: pull GPIO1 to GND briefly, then release to Hi-Z. */
    gpio_put(SIM7070_PWRKEY_PIN, 0);
    sleep_ms(1000);
    gpio_set_pulls(SIM7070_PWRKEY_PIN, false, false);
    gpio_set_dir(SIM7070_PWRKEY_PIN, GPIO_IN);
}

int main(void) {
    stdio_init_all();

    /* PWRKEY is wired to GPIO1: pulse LOW at boot, then release to Hi-Z. */
    sim7070_power_key_startup();

    /* Pico W Bluetooth Classic/HFP is provided by BTstack through the
     * CYW43 background architecture. */
    if (cyw43_arch_init() != 0) {
        printf("[HFP] CYW43 initialization failed\n");
    } else {
        hfp_client_init();
    }

    /* Initialize LVGL Library & Display Driver */
    touch_xpt2046_init();
    bool settings_valid = settings_load(&g_settings);
    touch_xpt2046_set_calibration(&g_settings.touch);
    usb_debug_set_enabled(g_settings.usb_debug_enabled);
    usb_debug_init();
    lv_init();
    // LVGL does not advance its internal clock automatically on bare metal.
    // Without this, input/timer callbacks can stop running even though the
    // main loop continues to call lv_timer_handler().
    lv_tick_set_cb(tusb_time_millis_api);

    // 1. Register Display Driver
    lv_display_t *disp = lv_display_create(320, 240);

    // Allocate partial render buffer (20 lines buffer)
    static uint8_t buf[320 * 20 * 2];
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    display_ili9341_init();
    display_ili9341_set_brightness(g_settings.brightness);
    lv_display_set_flush_cb(disp, display_ili9341_flush_cb);

    // 2. Register Touchpad Input Driver
    lv_indev_t *touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, touch_xpt2046_read_cb);

    /* Calibrate only on first boot (or after settings were invalidated). A
     * successful calibration is persisted and reused on subsequent boots. */
    if (!settings_valid) {
        while (!touch_xpt2046_run_calibration()) {
            sleep_ms(100);
        }
        touch_xpt2046_get_calibration(&g_settings.touch);
        settings_save(&g_settings);
    }

    /* Build the swipe-only dashboard UI */
    gui_init(lv_scr_act(), &g_gui_platform);

    /* Match the working SIM7070G Python client configuration. */
    static const Sim7070Config modem_config = {
        .apn = "soracom.io",
        .username = "sora",
        .password = "sora",
        .baudrate = SIM7070_UART_BAUDRATE,
        .uart_tx_pin = SIM7070_UART_TX_PIN,
        .uart_rx_pin = SIM7070_UART_RX_PIN,
    };
    static const WebSocketConfig websocket_config = {
        .host = "maruo-desktop.tailecc88.ts.net",
        .port = 443,
        .path = "/ws/audio",
        .tls = true,
        .on_binary = NULL,
    };
    sim7070_init(&modem_config);
    websocket_client_init(&websocket_config);

    uint32_t last_ui_update_ms = 0;
    uint32_t last_mock_update_ms = 0;
    uint32_t last_network_task_ms = 0;
    while (1) {
        /* USB starts as CDC device and switches to host after the original
         * debug window if no PC application opens the CDC interface. */
        usb_debug_task();

        if (usb_debug_is_host()) {
            process_usb_serial_rx();
        }

        /* Keep the dashboard visibly alive until a real USB telemetry source
         * is selected. Real incoming telemetry takes over in host mode. */
        uint32_t now = to_ms_since_boot(get_absolute_time());

        /* BOOTSEL restores defaults, waits for release, then enters the USB
         * bootloader. This keeps the physical BOOTSEL reset workflow intact. */
        handle_bootsel_reset();

        if (now - last_network_task_ms >= 10) {
            websocket_client_task();
            last_network_task_ms = now;
        }
        if (!websocket_client_is_connected() && now - last_mock_update_ms >= 250) {
            update_mock_telemetry();
            last_mock_update_ms = now;
        }

        if (touch_xpt2046_calibration_active()) {
            bool finished = touch_xpt2046_calibration_task();
            if (finished) {
                touch_xpt2046_get_calibration(&g_settings.touch);
                if (!settings_save(&g_settings)) {
                    usb_debug_log("[TOUCH] calibration applied but save failed\r\n");
                } else {
                    usb_debug_log("[TOUCH] calibration saved\r\n");
                }
            }
        }

        /* 2. Service LVGL Internal Timers (~5ms interval) */
        lv_timer_handler();

        /* 3. Refresh Dashboard UI at capped rate (~20Hz / 50ms) */
        now = to_ms_since_boot(get_absolute_time());
        if (g_telemetry_updated && (now - last_ui_update_ms >= 50)) {
            gui_update_telemetry(&g_telemetry);
            g_telemetry_updated = false;
            last_ui_update_ms = now;
        }

        sleep_ms(1);
    }

    return 0;
}
