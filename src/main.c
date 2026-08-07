#include "pico/stdlib.h"
#include "lvgl.h"
#include "display_ili9341.h"
#include "touch_xpt2046.h"

int main() {
    stdio_init_all();

    // Initialize hardware peripherals
    display_ili9341_init();
    touch_xpt2046_init();

    // Initialize LVGL core library
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

    // Create a demo UI button
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_center(btn);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Pico LVGL Test");

    // Main Loop
    while (1) {
        // Increment LVGL tick counter (5ms)
        lv_tick_inc(5);

        // Handle LVGL tasks
        lv_timer_handler();

        sleep_ms(5);
    }

    return 0;
}
