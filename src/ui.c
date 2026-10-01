#include "lvgl.h"
#include "structs.h"
#include "gui/ui.h"

/* --- LVGL UI Components --- */
static lv_obj_t *lbl_speed;
static lv_obj_t *arc_rpm;
static lv_obj_t *lbl_temp;
static lv_obj_t *lbl_volts;
static lv_obj_t *dd_sample_rate;
static lv_obj_t *lbl_brightness;
static lv_obj_t *usb_debug_switch;
static lv_obj_t *btn_ptt;
static lv_obj_t *lbl_ptt;
static lv_obj_t *registration_page;
static lv_obj_t *registration_pin;
static lv_obj_t *registration_status;
static lv_obj_t *bluetooth_list;
static lv_obj_t *bluetooth_keyboard;
static lv_obj_t *bluetooth_connect;
static GuiBluetoothDevice bluetooth_devices[12];
static int bluetooth_device_count;
static int bluetooth_selected = -1;
static bool g_ptt_active;
static GuiPlatform g_platform;
static lv_obj_t *touch_indicator;
static lv_timer_t *touch_indicator_timer;

static void touch_indicator_timer_cb(lv_timer_t *timer) {
    (void)timer;

    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev != NULL && lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER) {
        indev = lv_indev_get_next(indev);
    }

    if (indev == NULL || touch_indicator == NULL) {
        return;
    }

    if (lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        lv_obj_set_pos(touch_indicator, point.x - 8, point.y - 8);
        lv_obj_set_hidden(touch_indicator, false);
    } else {
        lv_obj_set_hidden(touch_indicator, true);
    }
}

static void create_touch_indicator(void) {
    lv_obj_t *layer = lv_layer_top();
    touch_indicator = lv_obj_create(layer);
    lv_obj_set_size(touch_indicator, 16, 16);
    lv_obj_set_style_radius(touch_indicator, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(touch_indicator, LV_OPA_70, 0);
    lv_obj_set_style_border_width(touch_indicator, 2, 0);
    lv_obj_set_style_border_opa(touch_indicator, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(touch_indicator, 0, 0);
    lv_obj_set_clickable(touch_indicator, false);
    lv_obj_set_hidden(touch_indicator, true);

    touch_indicator_timer = lv_timer_create(touch_indicator_timer_cb, 16, NULL);
}

/*
 * LVGL's built-in formatter has floating-point support tied to LV_USE_FLOAT.
 * This project deliberately keeps LV_USE_FLOAT disabled, so "%f" would be
 * rendered as a literal/unsupported conversion (the PC preview showed "f").
 * Format the telemetry as fixed-point integers instead; this also keeps the
 * formatting identical on the Pico and PC builds.
 */
static void set_label_fixed_1(lv_obj_t *label, const char *prefix, float value, const char *suffix) {
    int32_t scaled = (int32_t)(value * 10.0f + (value >= 0.0f ? 0.5f : -0.5f));
    const char *sign = scaled < 0 ? "-" : "";
    int32_t magnitude = scaled < 0 ? -scaled : scaled;
    int32_t whole = magnitude / 10;
    int32_t frac = magnitude % 10;
    lv_label_set_text_fmt(label, "%s%s%d.%d%s", prefix, sign, (int)whole, (int)frac, suffix);
}

static void set_label_fixed_2(lv_obj_t *label, const char *prefix, float value, const char *suffix) {
    int32_t scaled = (int32_t)(value * 100.0f + (value >= 0.0f ? 0.5f : -0.5f));
    const char *sign = scaled < 0 ? "-" : "";
    int32_t magnitude = scaled < 0 ? -scaled : scaled;
    int32_t whole = magnitude / 100;
    int32_t frac = magnitude % 100;
    lv_label_set_text_fmt(label, "%s%s%d.%02d%s", prefix, sign, (int)whole, (int)frac, suffix);
}

static void brightness_slider_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    uint8_t percent = (uint8_t)lv_slider_get_value(slider);
    if (g_platform.set_brightness != NULL) {
        g_platform.set_brightness(percent);
    }
    if (lbl_brightness != NULL) {
        lv_label_set_text_fmt(lbl_brightness, "Display Brightness: %u%%", percent);
    }
}

static void brightness_slider_release_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    if (g_platform.save_brightness != NULL) {
        g_platform.save_brightness((uint8_t)lv_slider_get_value(slider));
    }
}

static void usb_debug_switch_event_cb(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (g_platform.set_usb_debug_enabled != NULL) {
        g_platform.set_usb_debug_enabled(enabled);
    }
}

static void calibration_button_event_cb(lv_event_t *e) {
    (void)e;
    if (g_platform.run_touch_calibration != NULL) {
        (void)g_platform.run_touch_calibration();
    }
}

/* --- Event Callbacks --- */

/**
 * @brief Handle Log Start/Stop Toggle Button
 */
static void btn_log_event_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    bool is_active = lv_obj_has_state(btn, LV_STATE_CHECKED);

    if (is_active) {
        if (g_platform.start_logging != NULL) {
            g_platform.start_logging();
        }
    } else {
        if (g_platform.stop_logging != NULL) {
            g_platform.stop_logging();
        }
    }
}

static void btn_ptt_pressed_cb(lv_event_t *e) {
    (void)e;
    if (g_ptt_active) return;
    g_ptt_active = true;
    if (lbl_ptt != NULL) {
        lv_label_set_text(lbl_ptt, "PTT ACTIVE");
    }
    if (g_platform.ptt_pressed != NULL) {
        g_platform.ptt_pressed();
    }
}

static void btn_ptt_released_cb(lv_event_t *e) {
    (void)e;
    if (!g_ptt_active) return;
    g_ptt_active = false;
    if (lbl_ptt != NULL) {
        lv_label_set_text(lbl_ptt, "PUSH TO TALK");
    }
    if (g_platform.ptt_released != NULL) {
        g_platform.ptt_released();
    }
}

/**
 * @brief Handle Sample Rate Dropdown selection change
 */
static void dropdown_rate_event_cb(lv_event_t *e) {
    lv_obj_t *dd = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dd);

    int rate_map[] = {1, 10, 25, 50, 100};
    int selected_rate = rate_map[selected];

    if (g_platform.set_sample_rate != NULL) {
        g_platform.set_sample_rate("RPM", selected_rate);
    }
}

static void open_registration_cb(lv_event_t *e) {
    (void)e;
    if (registration_page != NULL) {
        lv_obj_scroll_to_view(registration_page, LV_ANIM_ON);
    }
}

static void bluetooth_keyboard_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        if (bluetooth_keyboard == NULL) return;
        lv_keyboard_set_mode(bluetooth_keyboard, LV_KEYBOARD_MODE_NUMBER);
        lv_keyboard_set_textarea(bluetooth_keyboard, ta);
        lv_obj_set_hidden(bluetooth_keyboard, false);
        lv_obj_scroll_to_view(ta, LV_ANIM_ON);
    } else if (code == LV_EVENT_DEFOCUSED) {
        if (bluetooth_keyboard == NULL) return;
        lv_keyboard_set_textarea(bluetooth_keyboard, NULL);
        lv_obj_set_hidden(bluetooth_keyboard, true);
    }
}

static void bluetooth_device_selected_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    bluetooth_selected = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_clear_state(bluetooth_connect, LV_STATE_DISABLED);
    lv_label_set_text_fmt(registration_status, "Selected: %s", bluetooth_devices[bluetooth_selected].name);
    (void)btn;
}

static void bluetooth_scan_cb(lv_event_t *e) {
    (void)e;
    bluetooth_selected = -1;
    lv_obj_clean(bluetooth_list);
    lv_obj_add_state(bluetooth_connect, LV_STATE_DISABLED);
    if (g_platform.bluetooth_scan == NULL) {
        lv_label_set_text(registration_status, "Bluetooth scan unavailable");
        return;
    }
    bluetooth_device_count = g_platform.bluetooth_scan(bluetooth_devices, 12);
    if (bluetooth_device_count < 0) bluetooth_device_count = 0;
    for (int i = 0; i < bluetooth_device_count; ++i) {
        lv_obj_t *item = lv_button_create(bluetooth_list);
        lv_obj_set_width(item, LV_PCT(100));
        lv_obj_set_height(item, 34);
        lv_obj_t *item_label = lv_label_create(item);
        lv_label_set_text(item_label, bluetooth_devices[i].name);
        lv_obj_align(item_label, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_add_event_cb(item, bluetooth_device_selected_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    lv_label_set_text_fmt(registration_status, "%d device(s) found", bluetooth_device_count);
}

static void bluetooth_connect_cb(lv_event_t *e) {
    (void)e;
    if (bluetooth_selected < 0 || bluetooth_selected >= bluetooth_device_count || g_platform.bluetooth_connect == NULL) {
        lv_label_set_text(registration_status, "Select a device first");
        return;
    }
    const char *pin = lv_textarea_get_text(registration_pin);
    lv_label_set_text(registration_status, "Connecting...");
    bool ok = g_platform.bluetooth_connect(&bluetooth_devices[bluetooth_selected], pin);
    lv_label_set_text(registration_status, ok ? "Connected" : "Connection failed");
}

/* --- UI Construction --- */

/**
 * @brief Build Dashboard Tab (Real-time telemetry display)
 */
static void build_dashboard_tab(lv_obj_t *parent) {
    /* Main Layout Container */
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 8, 0);
    lv_obj_set_style_pad_row(cont, 6, 0);
    lv_obj_set_scrollable(parent, false);
    lv_obj_set_scrollable(cont, false);

    /* Upper telemetry area: keep the gauge and info card side-by-side so the
     * 320x240 display has room for a full-width PTT control underneath. */
    lv_obj_t *telemetry_row = lv_obj_create(cont);
    lv_obj_set_width(telemetry_row, LV_PCT(100));
    lv_obj_set_height(telemetry_row, 174);
    lv_obj_set_flex_flow(telemetry_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(telemetry_row, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(telemetry_row, false);

    /* Left Side: RPM Gauge */
    arc_rpm = lv_arc_create(telemetry_row);
    lv_obj_set_size(arc_rpm, 150, 150);
    lv_arc_set_range(arc_rpm, 0, 8000);
    lv_arc_set_bg_angles(arc_rpm, 135, 45);
    lv_arc_set_value(arc_rpm, 0);
    // Dashboard telemetry is display-only. LVGL arcs are clickable/draggable
    // by default, which makes touching the gauge (or its child speed label)
    // change the RPM value as if it were a slider.
    lv_obj_set_clickable(arc_rpm, false);

    /* Center Overlay Text inside Arc: Speed */
    lbl_speed = lv_label_create(arc_rpm);
    lv_obj_align(lbl_speed, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(lbl_speed, "0.0\nkm/h");

    /* Right Side: Temperature and Voltage Cards */
    lv_obj_t *info_col = lv_obj_create(telemetry_row);
    lv_obj_set_size(info_col, 126, 150);
    lv_obj_set_flex_flow(info_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_col, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(info_col, false);

    lbl_temp = lv_label_create(info_col);
    lv_label_set_text(lbl_temp, "Temp: -- °C");

    lbl_volts = lv_label_create(info_col);
    lv_label_set_text(lbl_volts, "Volt: --.- V");

    /* Full-width, low-profile PTT control. Audio transport is deliberately
     * kept behind the platform callbacks until the microphone/speaker path is
     * selected. */
    btn_ptt = lv_btn_create(cont);
    lv_obj_set_width(btn_ptt, LV_PCT(100));
    lv_obj_set_height(btn_ptt, 42);
    lv_obj_add_event_cb(btn_ptt, btn_ptt_pressed_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(btn_ptt, btn_ptt_released_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(btn_ptt, btn_ptt_released_cb, LV_EVENT_PRESS_LOST, NULL);
    lbl_ptt = lv_label_create(btn_ptt);
    lv_label_set_text(lbl_ptt, "PUSH TO TALK");
    lv_obj_center(lbl_ptt);
}

/**
 * @brief Build RaceCapture Settings Tab
 */
static void build_settings_tab(lv_obj_t *parent) {
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(parent, 15, 0);
    lv_obj_set_style_pad_ver(parent, 12, 0);

    /* Section Title */
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "RaceCapture Settings");

    /* Toggle Logging Button */
    lv_obj_t *btn_log = lv_btn_create(parent);
    lv_obj_set_size(btn_log, 180, 40);
    lv_obj_set_checkable(btn_log, true);
    lv_obj_add_event_cb(btn_log, btn_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *lbl_btn = lv_label_create(btn_log);
    lv_label_set_text(lbl_btn, "Toggle Logging");
    lv_obj_center(lbl_btn);

    /* Sample Rate Setting Row */
    lv_obj_t *rate_row = lv_obj_create(parent);
    lv_obj_set_size(rate_row, 280, 50);
    lv_obj_set_flex_flow(rate_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rate_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(rate_row, false);

    lv_obj_t *lbl_rate = lv_label_create(rate_row);
    lv_label_set_text(lbl_rate, "RPM Rate:");

    dd_sample_rate = lv_dropdown_create(rate_row);
    lv_dropdown_set_options(dd_sample_rate, "1 Hz\n10 Hz\n25 Hz\n50 Hz\n100 Hz");
    lv_dropdown_set_selected(dd_sample_rate, 1); /* Default 10Hz */
    lv_obj_add_event_cb(dd_sample_rate, dropdown_rate_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Display backlight brightness */
    lv_obj_t *brightness_row = lv_obj_create(parent);
    lv_obj_set_size(brightness_row, 280, 65);
    lv_obj_set_flex_flow(brightness_row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(brightness_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(brightness_row, false);

    lbl_brightness = lv_label_create(brightness_row);
    uint8_t brightness = g_platform.get_brightness != NULL ? g_platform.get_brightness() : 100;
    lv_label_set_text_fmt(lbl_brightness, "Display Brightness: %u%%", brightness);

    lv_obj_t *brightness_slider = lv_slider_create(brightness_row);
    lv_obj_set_width(brightness_slider, 230);
    lv_slider_set_range(brightness_slider, 0, 100);
    lv_slider_set_value(brightness_slider, brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_release_cb, LV_EVENT_RELEASED, NULL);

    /* Bluetooth device registration entry point. */
    lv_obj_t *btn_registration = lv_btn_create(parent);
    lv_obj_set_size(btn_registration, 220, 42);
    lv_obj_add_event_cb(btn_registration, open_registration_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_registration = lv_label_create(btn_registration);
    lv_label_set_text(lbl_registration, "Bluetooth Registration");
    lv_obj_center(lbl_registration);

    lv_obj_t *usb_row = lv_obj_create(parent);
    lv_obj_set_size(usb_row, 280, 50);
    lv_obj_set_flex_flow(usb_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(usb_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(usb_row, false);
    lv_obj_t *usb_label = lv_label_create(usb_row);
    lv_label_set_text(usb_label, "USB Debug (reboot)");
    usb_debug_switch = lv_switch_create(usb_row);
    bool usb_enabled = g_platform.get_usb_debug_enabled == NULL || g_platform.get_usb_debug_enabled();
    if (usb_enabled) {
        lv_obj_add_state(usb_debug_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(usb_debug_switch, usb_debug_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *btn_calibration = lv_btn_create(parent);
    lv_obj_set_size(btn_calibration, 220, 42);
    lv_obj_add_event_cb(btn_calibration, calibration_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_calibration = lv_label_create(btn_calibration);
    lv_label_set_text(lbl_calibration, "Touch Calibration");
    lv_obj_center(lbl_calibration);
}

static void build_registration_page(lv_obj_t *parent) {
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(parent, 12, 0);
    lv_obj_set_style_pad_all(parent, 12, 0);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Bluetooth Devices");

    lv_obj_t *btn_scan = lv_btn_create(parent);
    lv_obj_set_size(btn_scan, 150, 38);
    lv_obj_add_event_cb(btn_scan, bluetooth_scan_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_scan = lv_label_create(btn_scan);
    lv_label_set_text(lbl_scan, "Scan");
    lv_obj_center(lbl_scan);

    bluetooth_list = lv_obj_create(parent);
    lv_obj_set_size(bluetooth_list, 280, 72);
    lv_obj_set_flex_flow(bluetooth_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(bluetooth_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(bluetooth_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(bluetooth_list, LV_SCROLLBAR_MODE_AUTO);

    registration_pin = lv_textarea_create(parent);
    lv_obj_set_size(registration_pin, 280, 42);
    lv_textarea_set_one_line(registration_pin, true);
    lv_textarea_set_text(registration_pin, "0000");
    lv_textarea_set_accepted_chars(registration_pin, "0123456789");
    lv_textarea_set_password_mode(registration_pin, true);

    lv_obj_add_event_cb(registration_pin, bluetooth_keyboard_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(registration_pin, bluetooth_keyboard_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(registration_pin, bluetooth_keyboard_event_cb, LV_EVENT_DEFOCUSED, NULL);

    bluetooth_connect = lv_btn_create(parent);
    lv_obj_set_size(bluetooth_connect, 160, 42);
    lv_obj_add_state(bluetooth_connect, LV_STATE_DISABLED);
    lv_obj_add_event_cb(bluetooth_connect, bluetooth_connect_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_connect = lv_label_create(bluetooth_connect);
    lv_label_set_text(lbl_connect, "Connect");
    lv_obj_center(lbl_connect);

    registration_status = lv_label_create(parent);
    lv_label_set_text(registration_status, "Ready");

    /* Keep the keyboard out of the scrollable registration layout so it can
     * always overlay the lower part of the 320x240 display. */
    bluetooth_keyboard = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(bluetooth_keyboard, 320, 112);
    lv_obj_align(bluetooth_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_hidden(bluetooth_keyboard, true);
}

/**
 * @brief Initialize Main Tabview Architecture
 */
void gui_init(lv_obj_t *screen_root, const GuiPlatform *platform) {
    if (platform != NULL) {
        g_platform = *platform;
    } else {
        g_platform = (GuiPlatform){0};
    }

    /* Full-screen horizontal pages. There is deliberately no tab bar: page
     * changes are made only by swiping left/right. */
    lv_obj_t *page_view = lv_obj_create(screen_root);
    lv_obj_set_size(page_view, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(page_view, 0, 0);
    lv_obj_set_flex_flow(page_view, LV_FLEX_FLOW_ROW);
    lv_obj_set_scroll_dir(page_view, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(page_view, LV_SCROLL_SNAP_START);
    lv_obj_set_scrollbar_mode(page_view, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *dashboard_page = lv_obj_create(page_view);
    lv_obj_set_size(dashboard_page, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollable(dashboard_page, false);
    lv_obj_t *config_page = lv_obj_create(page_view);
    lv_obj_set_size(config_page, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_height(config_page, LV_PCT(100));
    registration_page = lv_obj_create(page_view);
    lv_obj_set_size(registration_page, LV_PCT(100), LV_PCT(100));
    build_dashboard_tab(dashboard_page);
    build_settings_tab(config_page);
    build_registration_page(registration_page);
    create_touch_indicator();
}

/**
 * @brief Update UI elements with current telemetry state (Call at ~10Hz-30Hz)
 */
void gui_update_telemetry(const RaceCaptureTelemetry *data) {
    if (arc_rpm != NULL) {
        lv_arc_set_value(arc_rpm, (int32_t)data->rpm);
    }
    if (lbl_speed != NULL) {
        set_label_fixed_1(lbl_speed, "", data->speed, "\nkm/h");
    }
    if (lbl_temp != NULL) {
        set_label_fixed_1(lbl_temp, "Temp: ", data->temp, " °C");
    }
    if (lbl_volts != NULL) {
        set_label_fixed_2(lbl_volts, "Volt: ", data->volts, " V");
    }
}
