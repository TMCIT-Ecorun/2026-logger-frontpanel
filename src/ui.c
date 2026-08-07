#include "lvgl.h"
#include <cJSON.h>
#include <stdio.h>
#include "command.h"
#include "structs.h"

/* --- LVGL UI Components --- */
static lv_obj_t *tabview;
static lv_obj_t *lbl_speed;
static lv_obj_t *arc_rpm;
static lv_obj_t *lbl_temp;
static lv_obj_t *lbl_volts;
static lv_obj_t *dd_sample_rate;

/* --- Event Callbacks --- */

/**
 * @brief Handle Log Start/Stop Toggle Button
 */
static void btn_log_event_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    bool is_active = lv_obj_has_state(btn, LV_STATE_CHECKED);

    if (is_active) {
        cmd_start_logging();
    } else {
        cmd_stop_logging();
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

    cmd_set_sample_rate("RPM", selected_rate);
}

/* --- UI Construction --- */

/**
 * @brief Build Dashboard Tab (Real-time telemetry display)
 */
static void build_dashboard_tab(lv_obj_t *parent) {
    /* Main Layout Container */
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Left Side: RPM Gauge */
    arc_rpm = lv_arc_create(cont);
    lv_obj_set_size(arc_rpm, 160, 160);
    lv_arc_set_range(arc_rpm, 0, 8000);
    lv_arc_set_bg_angles(arc_rpm, 135, 45);
    lv_arc_set_value(arc_rpm, 0);

    /* Center Overlay Text inside Arc: Speed */
    lbl_speed = lv_label_create(arc_rpm);
    lv_obj_align(lbl_speed, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(lbl_speed, "0.0\nkm/h");

    /* Right Side: Temperature and Voltage Cards */
    lv_obj_t *info_col = lv_obj_create(cont);
    lv_obj_set_size(info_col, 120, 160);
    lv_obj_set_flex_flow(info_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_col, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    lbl_temp = lv_label_create(info_col);
    lv_label_set_text(lbl_temp, "Temp: -- °C");

    lbl_volts = lv_label_create(info_col);
    lv_label_set_text(lbl_volts, "Volt: --.- V");
}

/**
 * @brief Build RaceCapture Settings Tab
 */
static void build_settings_tab(lv_obj_t *parent) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(cont, 15, 0);

    /* Section Title */
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "RaceCapture Settings");

    /* Toggle Logging Button */
    lv_obj_t *btn_log = lv_btn_create(cont);
    lv_obj_set_size(btn_log, 180, 40);
    lv_obj_add_flag(btn_log, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(btn_log, btn_log_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *lbl_btn = lv_label_create(btn_log);
    lv_label_set_text(lbl_btn, "Toggle Logging");
    lv_obj_center(lbl_btn);

    /* Sample Rate Setting Row */
    lv_obj_t *rate_row = lv_obj_create(cont);
    lv_obj_set_size(rate_row, 280, 50);
    lv_obj_set_flex_flow(rate_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rate_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *lbl_rate = lv_label_create(rate_row);
    lv_label_set_text(lbl_rate, "RPM Rate:");

    dd_sample_rate = lv_dropdown_create(rate_row);
    lv_dropdown_set_options(dd_sample_rate, "1 Hz\n10 Hz\n25 Hz\n50 Hz\n100 Hz");
    lv_dropdown_set_selected(dd_sample_rate, 1); /* Default 10Hz */
    lv_obj_add_event_cb(dd_sample_rate, dropdown_rate_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

/**
 * @brief Initialize Main Tabview Architecture
 */
void create_ui_tabview(lv_obj_t *screen_root) {
    /* Create Tabview */
    tabview = lv_tabview_create(screen_root);

    /* Add Tabs */
    lv_obj_t *tab_dash = lv_tabview_add_tab(tabview, "Dashboard");
    lv_obj_t *tab_cfg  = lv_tabview_add_tab(tabview, "RaceCapture Config");

    /* Populate Tabs */
    build_dashboard_tab(tab_dash);
    build_settings_tab(tab_cfg);
}

/**
 * @brief Update UI elements with current telemetry state (Call at ~10Hz-30Hz)
 */
void update_ui_telemetry(const RaceCaptureTelemetry *data) {
    if (arc_rpm != NULL) {
        lv_arc_set_value(arc_rpm, (int32_t)data->rpm);
    }
    if (lbl_speed != NULL) {
        lv_label_set_text_fmt(lbl_speed, "%.1f\nkm/h", data->speed);
    }
    if (lbl_temp != NULL) {
        lv_label_set_text_fmt(lbl_temp, "Temp: %.1f °C", data->temp);
    }
    if (lbl_volts != NULL) {
        lv_label_set_text_fmt(lbl_volts, "Volt: %.2f V", data->volts);
    }
}
