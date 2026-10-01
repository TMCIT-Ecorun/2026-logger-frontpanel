#include "touch_xpt2046.h"
#include "pins.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "touch_xpt2046.pio.h"
#include "usb_debug.h"

static bool g_initialized;
static PIO g_pio = pio1;
static uint g_sm;
static uint g_pio_offset;
static uint32_t g_touch_transaction_failures;
static bool g_touch_pressed;
static int32_t g_touch_last_x;
static int32_t g_touch_last_y;
static TouchCalibration g_calibration;

typedef enum {
    CAL_IDLE,
    CAL_WAIT_RELEASE,
    CAL_WAIT_PRESS,
} CalibrationState;

static CalibrationState g_cal_state = CAL_IDLE;
static lv_obj_t *g_cal_screen;
static lv_obj_t *g_cal_target;
static lv_obj_t *g_cal_status;
static lv_obj_t *g_cal_previous_screen;
static unsigned g_cal_target_index;
static uint16_t g_cal_min_x;
static uint16_t g_cal_max_x;
static uint16_t g_cal_min_y;
static uint16_t g_cal_max_y;
static unsigned g_cal_release_samples;
static unsigned g_cal_press_samples;
static uint32_t g_cal_state_since_us;

static const struct {
    int16_t x;
    int16_t y;
} g_cal_targets[] = {
    {20, 20}, {299, 20}, {299, 219}, {20, 219}
};

static const TouchCalibration g_default_calibration = {
    TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX,
    TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX,
};

static void touch_select_pins(void) {
    pio_gpio_init(g_pio, PIN_TOUCH_MOSI);
    pio_gpio_init(g_pio, PIN_TOUCH_SCK);
    pio_gpio_init(g_pio, PIN_TOUCH_MISO);
    pio_sm_set_consecutive_pindirs(g_pio, g_sm, PIN_TOUCH_MOSI, 1, true);
    pio_sm_set_consecutive_pindirs(g_pio, g_sm, PIN_TOUCH_SCK, 1, true);
    pio_sm_set_consecutive_pindirs(g_pio, g_sm, PIN_TOUCH_MISO, 1, false);
}

#define TOUCH_TRANSACTION_TIMEOUT_US 250u

void touch_xpt2046_init(void) {
    if (g_initialized) return;
    gpio_init(PIN_TOUCH_CS);
    gpio_set_dir(PIN_TOUCH_CS, GPIO_OUT);
    gpio_put(PIN_TOUCH_CS, 1);

    g_pio_offset = pio_add_program(g_pio, &xpt2046_spi_program);
    g_sm = pio_claim_unused_sm(g_pio, true);
    pio_sm_config c = xpt2046_spi_program_get_default_config(g_pio_offset);
    sm_config_set_out_pins(&c, PIN_TOUCH_MOSI, 1);
    sm_config_set_in_pins(&c, PIN_TOUCH_MISO);
    sm_config_set_sideset_pins(&c, PIN_TOUCH_SCK);
    sm_config_set_in_shift(&c, false, true, 24);
    sm_config_set_out_shift(&c, false, false, 32);
    sm_config_set_clkdiv(&c, 20.833333f);

    touch_select_pins();
    pio_sm_init(g_pio, g_sm, g_pio_offset, &c);
    pio_sm_set_enabled(g_pio, g_sm, false);

    g_calibration = g_default_calibration;
    g_initialized = true;
}

void touch_xpt2046_set_default_calibration(TouchCalibration *calibration) {
    *calibration = g_default_calibration;
}

void touch_xpt2046_set_calibration(const TouchCalibration *calibration) {
    g_calibration = *calibration;
    g_touch_pressed = false;
}

void touch_xpt2046_get_calibration(TouchCalibration *calibration) {
    *calibration = g_calibration;
}

static bool xpt2046_cmd(uint8_t cmd, uint16_t *result) {
    uint32_t tx = (uint32_t)cmd << 24;

    pio_sm_set_enabled(g_pio, g_sm, false);
    pio_sm_clear_fifos(g_pio, g_sm);
    pio_sm_restart(g_pio, g_sm);
    touch_select_pins();

    gpio_put(PIN_TOUCH_CS, 0);
    pio_sm_put(g_pio, g_sm, tx);
    pio_sm_set_enabled(g_pio, g_sm, true);

    uint32_t start = time_us_32();
    while (pio_sm_is_rx_fifo_empty(g_pio, g_sm)) {
        if ((uint32_t)(time_us_32() - start) >= TOUCH_TRANSACTION_TIMEOUT_US) {
            pio_sm_set_enabled(g_pio, g_sm, false);
            pio_sm_clear_fifos(g_pio, g_sm);
            pio_sm_restart(g_pio, g_sm);
            gpio_put(PIN_TOUCH_CS, 1);
            g_touch_transaction_failures++;
            return false;
        }
        tight_loop_contents();
    }

    uint32_t rx = pio_sm_get(g_pio, g_sm);
    pio_sm_set_enabled(g_pio, g_sm, false);
    gpio_put(PIN_TOUCH_CS, 1);
    *result = (uint16_t)((rx & 0xffffu) >> 3);
    return true;
}

bool touch_xpt2046_read_raw(uint16_t *raw_x, uint16_t *raw_y, uint32_t *pressure) {
    uint16_t z1, z2;
    if (!xpt2046_cmd(0xB0, &z1) ||
        !xpt2046_cmd(0xC0, &z2) ||
        !xpt2046_cmd(0xD0, raw_x) ||
        !xpt2046_cmd(0x90, raw_y)) {
        *pressure = 0;
        return false;
    }
    if (z1 == 0 || z2 > (uint16_t)(z1 + 4095u)) {
        *pressure = 0;
    } else {
        uint32_t z = (uint32_t)z1 + 4095u - z2;
        *pressure = ((uint32_t)*raw_x * z) / z1;
    }
    return true;
}

static int32_t map_axis(uint16_t raw, uint16_t raw_min, uint16_t raw_max, int32_t screen_max) {
    int32_t value = raw;
    if (value < raw_min) value = raw_min;
    if (value > raw_max) value = raw_max;
    return (value - (int32_t)raw_min) * screen_max /
           ((int32_t)raw_max - (int32_t)raw_min);
}

bool touch_xpt2046_run_calibration(void) {
    /* Keep calibration independent from the LVGL input callback: while the
     * calibration screen is active we need the uncalibrated ADC values. */
    static const struct {
        int16_t x;
        int16_t y;
    } targets[] = {
        {20, 20}, {299, 20}, {299, 219}, {20, 219}
    };

    lv_obj_t *previous_screen = lv_scr_act();
    lv_obj_t *screen = lv_obj_create(NULL);
    if (screen == NULL) return false;
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Touch Calibration\nTouch each target");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *target = lv_obj_create(screen);
    lv_obj_set_size(target, 20, 20);
    lv_obj_set_style_bg_opa(target, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(target, 3, 0);
    lv_obj_set_style_border_opa(target, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(target, lv_color_white(), 0);
    lv_obj_set_style_radius(target, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_clickable(target, false);

    lv_obj_t *status = lv_label_create(screen);
    lv_obj_set_style_text_color(status, lv_color_white(), 0);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -8);

    lv_scr_load(screen);
    lv_timer_handler();

    uint16_t min_x = 4095, max_x = 0, min_y = 4095, max_y = 0;
    bool ok = true;

    for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); ++i) {
        lv_obj_set_pos(target, targets[i].x - 10, targets[i].y - 10);
        lv_label_set_text_fmt(status, "Target %u / %u", (unsigned)(i + 1),
                               (unsigned)(sizeof(targets) / sizeof(targets[0])));
        lv_timer_handler();
        sleep_ms(150);

        /* Do not accept the press that launched the calibration button. */
        uint32_t release_start = time_us_32();
        while ((uint32_t)(time_us_32() - release_start) < 3000000u) {
            uint16_t raw_x, raw_y;
            uint32_t pressure;
            if (!touch_xpt2046_read_raw(&raw_x, &raw_y, &pressure) ||
                pressure <= TOUCH_PRESSURE_THRESHOLD) {
                break;
            }
            lv_timer_handler();
            sleep_ms(10);
        }

        bool pressed = false;
        uint32_t wait_start = time_us_32();
        while ((uint32_t)(time_us_32() - wait_start) < 15000000u) {
            uint16_t raw_x, raw_y;
            uint32_t pressure;
            if (touch_xpt2046_read_raw(&raw_x, &raw_y, &pressure) &&
                pressure > TOUCH_PRESSURE_THRESHOLD) {
                uint32_t sum_x = raw_x;
                uint32_t sum_y = raw_y;
                unsigned samples = 1;
                for (unsigned n = 0; n < 7; ++n) {
                    sleep_ms(5);
                    uint16_t sx, sy;
                    uint32_t sp;
                    if (touch_xpt2046_read_raw(&sx, &sy, &sp) &&
                        sp > TOUCH_PRESSURE_THRESHOLD) {
                        sum_x += sx;
                        sum_y += sy;
                        ++samples;
                    }
                }
                raw_x = (uint16_t)(sum_x / samples);
                raw_y = (uint16_t)(sum_y / samples);
                if (raw_x < min_x) min_x = raw_x;
                if (raw_x > max_x) max_x = raw_x;
                if (raw_y < min_y) min_y = raw_y;
                if (raw_y > max_y) max_y = raw_y;
                pressed = true;
                break;
            }
            lv_timer_handler();
            sleep_ms(10);
        }

        if (!pressed) {
            ok = false;
            break;
        }

        /* Wait for release before advancing to the next target. */
        uint32_t release_wait = time_us_32();
        while ((uint32_t)(time_us_32() - release_wait) < 3000000u) {
            uint16_t raw_x, raw_y;
            uint32_t pressure;
            if (!touch_xpt2046_read_raw(&raw_x, &raw_y, &pressure) ||
                pressure <= TOUCH_PRESSURE_THRESHOLD) {
                break;
            }
            lv_timer_handler();
            sleep_ms(10);
        }
    }

    if (ok && (uint16_t)(max_x - min_x) >= 300u &&
        (uint16_t)(max_y - min_y) >= 300u) {
        TouchCalibration calibration = {min_x, max_x, min_y, max_y};
        g_calibration = calibration;
        g_touch_pressed = false;
        char message[96];
        snprintf(message, sizeof(message),
                 "[TOUCH] calibration raw X=%u..%u Y=%u..%u\r\n",
                 min_x, max_x, min_y, max_y);
        usb_debug_log(message);
    } else {
        ok = false;
        char message[96];
        snprintf(message, sizeof(message),
                 "[TOUCH] calibration failed: X=%u..%u Y=%u..%u\r\n",
                 min_x, max_x, min_y, max_y);
        usb_debug_log(message);
    }

    lv_scr_load(previous_screen);
    lv_obj_del(screen);
    lv_timer_handler();
    return ok;
}

bool touch_xpt2046_start_calibration(void) {
    if (g_cal_state != CAL_IDLE) return false;

    g_cal_previous_screen = lv_scr_act();
    g_cal_screen = lv_obj_create(NULL);
    if (g_cal_screen == NULL) return false;
    lv_obj_set_style_bg_opa(g_cal_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_cal_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(g_cal_screen, 0, 0);
    lv_obj_set_style_pad_all(g_cal_screen, 0, 0);

    lv_obj_t *title = lv_label_create(g_cal_screen);
    lv_label_set_text(title, "Touch Calibration\nTouch each target");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    g_cal_target = lv_obj_create(g_cal_screen);
    lv_obj_set_size(g_cal_target, 20, 20);
    lv_obj_set_style_bg_opa(g_cal_target, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_cal_target, 3, 0);
    lv_obj_set_style_border_opa(g_cal_target, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_cal_target, lv_color_white(), 0);
    lv_obj_set_style_radius(g_cal_target, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_clickable(g_cal_target, false);

    g_cal_status = lv_label_create(g_cal_screen);
    lv_obj_set_style_text_color(g_cal_status, lv_color_white(), 0);
    lv_obj_align(g_cal_status, LV_ALIGN_BOTTOM_MID, 0, -8);

    g_cal_target_index = 0;
    g_cal_min_x = 4095;
    g_cal_max_x = 0;
    g_cal_min_y = 4095;
    g_cal_max_y = 0;
    g_cal_release_samples = 0;
    g_cal_press_samples = 0;
    g_cal_state_since_us = time_us_32();
    g_cal_state = CAL_WAIT_RELEASE;
    lv_scr_load(g_cal_screen);
    lv_obj_set_pos(g_cal_target,
                   g_cal_targets[0].x - 10, g_cal_targets[0].y - 10);
    lv_label_set_text_fmt(g_cal_status, "Target 1 / 4");
    return true;
}

bool touch_xpt2046_calibration_active(void) {
    return g_cal_state != CAL_IDLE;
}

bool touch_xpt2046_calibration_task(void) {
    if (g_cal_state == CAL_IDLE) return false;

    uint16_t raw_x, raw_y;
    uint32_t pressure;
    bool touched = touch_xpt2046_read_raw(&raw_x, &raw_y, &pressure) &&
                   pressure > TOUCH_PRESSURE_THRESHOLD;

    if (g_cal_state == CAL_WAIT_RELEASE) {
        /* The calibration button's press must be fully released before the
         * first target can be accepted. Require several consecutive clean
         * samples and a small settling time to reject ADC noise. */
        if (!touched) {
            ++g_cal_release_samples;
        } else {
            g_cal_release_samples = 0;
        }
        if (g_cal_release_samples >= 5 &&
            (uint32_t)(time_us_32() - g_cal_state_since_us) >= 150000u) {
            g_cal_press_samples = 0;
            g_cal_state_since_us = time_us_32();
            g_cal_state = CAL_WAIT_PRESS;
        }
        return false;
    }

    if (!touched) {
        g_cal_press_samples = 0;
        g_cal_state_since_us = time_us_32();
        return false;
    }

    /* Require a stable press instead of accepting one noisy sample. */
    ++g_cal_press_samples;
    if (g_cal_press_samples < 5 ||
        (uint32_t)(time_us_32() - g_cal_state_since_us) < 120u * 1000u) {
        return false;
    }

    if (raw_x < g_cal_min_x) g_cal_min_x = raw_x;
    if (raw_x > g_cal_max_x) g_cal_max_x = raw_x;
    if (raw_y < g_cal_min_y) g_cal_min_y = raw_y;
    if (raw_y > g_cal_max_y) g_cal_max_y = raw_y;

    ++g_cal_target_index;
    if (g_cal_target_index < sizeof(g_cal_targets) / sizeof(g_cal_targets[0])) {
        g_cal_release_samples = 0;
        g_cal_press_samples = 0;
        g_cal_state_since_us = time_us_32();
        g_cal_state = CAL_WAIT_RELEASE;
        lv_obj_set_pos(g_cal_target,
                       g_cal_targets[g_cal_target_index].x - 10,
                       g_cal_targets[g_cal_target_index].y - 10);
        lv_label_set_text_fmt(g_cal_status, "Target %u / 4", g_cal_target_index + 1);
        return false;
    }

    bool ok = (uint16_t)(g_cal_max_x - g_cal_min_x) >= 300u &&
              (uint16_t)(g_cal_max_y - g_cal_min_y) >= 300u;
    if (ok) {
        g_calibration = (TouchCalibration){
            g_cal_min_x, g_cal_max_x, g_cal_min_y, g_cal_max_y
        };
        g_touch_pressed = false;
        char message[96];
        snprintf(message, sizeof(message),
                 "[TOUCH] calibration raw X=%u..%u Y=%u..%u\r\n",
                 g_cal_min_x, g_cal_max_x, g_cal_min_y, g_cal_max_y);
        usb_debug_log(message);
    } else {
        char message[96];
        snprintf(message, sizeof(message),
                 "[TOUCH] calibration failed: X=%u..%u Y=%u..%u\r\n",
                 g_cal_min_x, g_cal_max_x, g_cal_min_y, g_cal_max_y);
        usb_debug_log(message);
    }

    lv_scr_load(g_cal_previous_screen);
    lv_obj_del(g_cal_screen);
    g_cal_screen = NULL;
    g_cal_target = NULL;
    g_cal_status = NULL;
    g_cal_state = CAL_IDLE;
    return ok;
}

void touch_xpt2046_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    /* Calibration owns the raw touch stream. Do not let LVGL dispatch the
     * finger-up/finger-down event to the settings screen while the calibration
     * state machine is changing screens. */
    if (g_cal_state != CAL_IDLE) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    uint16_t raw_x, raw_y;
    uint32_t pressure;
    if (!touch_xpt2046_read_raw(&raw_x, &raw_y, &pressure) ||
        pressure <= TOUCH_PRESSURE_THRESHOLD) {
        g_touch_pressed = false;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    const uint16_t margin = 128u;
    if (raw_x + margin < g_calibration.raw_x_min ||
        raw_x > g_calibration.raw_x_max + margin ||
        raw_y + margin < g_calibration.raw_y_min ||
        raw_y > g_calibration.raw_y_max + margin) {
        g_touch_pressed = false;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    int32_t x = map_axis(raw_x, g_calibration.raw_x_min, g_calibration.raw_x_max, 239);
    int32_t y = map_axis(raw_y, g_calibration.raw_y_min, g_calibration.raw_y_max, 319);
#if TOUCH_SWAP_XY
    int32_t tmp = x; x = y; y = tmp;
#endif
#if TOUCH_INVERT_X
    x = 319 - x;
#endif
#if TOUCH_INVERT_Y
    y = 239 - y;
#endif

    if (g_touch_pressed) {
        int32_t dx = x - g_touch_last_x;
        int32_t dy = y - g_touch_last_y;
        if (dx > 80 || dx < -80 || dy > 80 || dy < -80) {
            x = g_touch_last_x;
            y = g_touch_last_y;
        }
    }

    g_touch_pressed = true;
    g_touch_last_x = x;
    g_touch_last_y = y;
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
}
