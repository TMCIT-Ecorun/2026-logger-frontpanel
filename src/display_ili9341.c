#include "display_ili9341.h"
#include "display_pio_ili9341.pio.h"
#include "pins.h"

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"

#include <stddef.h>

static PIO g_pio = pio0;
static uint g_sm = 0;
static uint g_pio_offset;
static uint32_t g_speed_hz = DISP_SPI_BAUDRATE;
static bool g_initialized;
static uint g_bl_slice;
static uint g_bl_channel;
static uint8_t g_bl_percent;

#define DISPLAY_BL_PWM_WRAP 255u

static void display_select_pins(void) {
    // The touch controller shares SCK/MOSI. Switch the shared pins back to
    // PIO0 whenever the ILI9341 state machine is about to drive them.
    pio_gpio_init(g_pio, PIN_DISP_MOSI);
    pio_gpio_init(g_pio, PIN_DISP_SCK);
    pio_sm_set_consecutive_pindirs(g_pio, g_sm, PIN_DISP_MOSI, 1, true);
    pio_sm_set_consecutive_pindirs(g_pio, g_sm, PIN_DISP_SCK, 1, true);
}

static void pio_set_frame_bits(uint bits) {
    pio_sm_config c = ili9341_tx_program_get_default_config(g_pio_offset);
    sm_config_set_out_pins(&c, PIN_DISP_MOSI, 1);
    sm_config_set_sideset_pins(&c, PIN_DISP_SCK);
    sm_config_set_out_shift(&c, false, true, bits);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / (2.0f * (float)g_speed_hz));
    pio_sm_init(g_pio, g_sm, g_pio_offset, &c);
    pio_sm_set_enabled(g_pio, g_sm, true);
}

static void write_bytes(const uint8_t *data, size_t len) {
    pio_sm_set_enabled(g_pio, g_sm, false);
    pio_sm_clear_fifos(g_pio, g_sm);
    pio_sm_restart(g_pio, g_sm);
    display_select_pins();
    pio_set_frame_bits(8);

    gpio_put(PIN_DISP_CS, 0);
    for (size_t i = 0; i < len; ++i) {
        pio_sm_put_blocking(g_pio, g_sm, (uint32_t)data[i] << 24);
    }
    while (!pio_sm_is_tx_fifo_empty(g_pio, g_sm)) {
        tight_loop_contents();
    }
    sleep_us(1);
    gpio_put(PIN_DISP_CS, 1);

    pio_sm_set_enabled(g_pio, g_sm, false);
    pio_sm_clear_fifos(g_pio, g_sm);
    pio_sm_restart(g_pio, g_sm);
}

static void write_cmd(uint8_t cmd) {
    gpio_put(PIN_DISP_DC, 0);
    write_bytes(&cmd, 1);
}

static void write_data(const uint8_t *data, size_t len) {
    gpio_put(PIN_DISP_DC, 1);
    write_bytes(data, len);
}

bool display_ili9341_set_speed(uint32_t hz) {
    if (!g_initialized || hz == 0 || hz > 62500000u) {
        return false;
    }
    g_speed_hz = hz;
    pio_sm_set_clkdiv(g_pio, g_sm,
                      (float)clock_get_hz(clk_sys) / (2.0f * (float)g_speed_hz));
    return true;
}

uint32_t display_ili9341_get_speed(void) {
    return g_speed_hz;
}

void display_ili9341_init(void) {
    if (g_initialized) {
        return;
    }

    gpio_init(PIN_DISP_CS);
    gpio_set_dir(PIN_DISP_CS, GPIO_OUT);
    gpio_put(PIN_DISP_CS, 1);
    gpio_init(PIN_DISP_DC);
    gpio_set_dir(PIN_DISP_DC, GPIO_OUT);
    gpio_put(PIN_DISP_DC, 0);
    gpio_init(PIN_DISP_RST);
    gpio_set_dir(PIN_DISP_RST, GPIO_OUT);
    gpio_put(PIN_DISP_RST, 1);
    gpio_init(PIN_DISP_BL);
    g_bl_slice = pwm_gpio_to_slice_num(PIN_DISP_BL);
    g_bl_channel = pwm_gpio_to_channel(PIN_DISP_BL);
    gpio_set_function(PIN_DISP_BL, GPIO_FUNC_PWM);
    pwm_config bl_cfg = pwm_get_default_config();
    pwm_config_set_wrap(&bl_cfg, DISPLAY_BL_PWM_WRAP);
    pwm_config_set_clkdiv(&bl_cfg, 4.0f);
    pwm_init(g_bl_slice, &bl_cfg, true);
    pwm_set_chan_level(g_bl_slice, g_bl_channel, 0);

    g_pio_offset = pio_add_program(g_pio, &ili9341_tx_program);
    pio_sm_config c = ili9341_tx_program_get_default_config(g_pio_offset);
    sm_config_set_out_pins(&c, PIN_DISP_MOSI, 1);
    sm_config_set_sideset_pins(&c, PIN_DISP_SCK);
    sm_config_set_out_shift(&c, false, true, 16);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / (2.0f * (float)g_speed_hz));

    pio_gpio_init(g_pio, PIN_DISP_MOSI);
    pio_gpio_init(g_pio, PIN_DISP_SCK);
    pio_sm_set_consecutive_pindirs(g_pio, g_sm, PIN_DISP_MOSI, 1, true);
    pio_sm_set_consecutive_pindirs(g_pio, g_sm, PIN_DISP_SCK, 1, true);
    pio_sm_init(g_pio, g_sm, g_pio_offset, &c);
    pio_sm_set_pins(g_pio, g_sm, 0);
    pio_sm_set_enabled(g_pio, g_sm, false);

    g_initialized = true;

    gpio_put(PIN_DISP_RST, 0);
    sleep_ms(10);
    gpio_put(PIN_DISP_RST, 1);
    sleep_ms(120);
    write_cmd(0x01);
    sleep_ms(5);
    write_cmd(0x28);

    // 320x240 landscape, BGR. MV exchanges row/column addressing so the
    // controller's native 240x320 panel matches LVGL's 320x240 display.
    uint8_t mac = 0x28;
    write_cmd(0x36);
    write_data(&mac, 1);
    uint8_t fmt = 0x55;
    write_cmd(0x3A);
    write_data(&fmt, 1);
    write_cmd(0x11);
    sleep_ms(120);
    write_cmd(0x29);
    display_ili9341_set_brightness(100);
}

void display_ili9341_set_brightness(uint8_t percent) {
    if (percent > 100u) percent = 100u;
    uint16_t level = (uint16_t)(((uint32_t)percent * DISPLAY_BL_PWM_WRAP + 50u) / 100u);
    pwm_set_chan_level(g_bl_slice, g_bl_channel, level);
    g_bl_percent = percent;
}

uint8_t display_ili9341_get_brightness(void) {
    return g_bl_percent;
}

void display_ili9341_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    const int32_t width = area->x2 - area->x1 + 1;
    const int32_t height = area->y2 - area->y1 + 1;
    const uint32_t pixels = (uint32_t)width * (uint32_t)height;
    uint8_t addr[4];

    gpio_put(PIN_DISP_DC, 0);
    uint8_t caset = 0x2A;
    write_bytes(&caset, 1);
    gpio_put(PIN_DISP_DC, 1);
    addr[0] = (uint8_t)(area->x1 >> 8);
    addr[1] = (uint8_t)area->x1;
    addr[2] = (uint8_t)(area->x2 >> 8);
    addr[3] = (uint8_t)area->x2;
    write_bytes(addr, sizeof(addr));

    gpio_put(PIN_DISP_DC, 0);
    uint8_t paset = 0x2B;
    write_bytes(&paset, 1);
    gpio_put(PIN_DISP_DC, 1);
    addr[0] = (uint8_t)(area->y1 >> 8);
    addr[1] = (uint8_t)area->y1;
    addr[2] = (uint8_t)(area->y2 >> 8);
    addr[3] = (uint8_t)area->y2;
    write_bytes(addr, sizeof(addr));

    gpio_put(PIN_DISP_DC, 0);
    uint8_t ramwr = 0x2C;
    write_bytes(&ramwr, 1);

    gpio_put(PIN_DISP_DC, 1);
    gpio_put(PIN_DISP_CS, 0);
    // ILI9341 RGB565 is transmitted MSB first. LVGL's RGB565 pixels are
    // stored as native-endian uint16_t values on RP2040, so form each wire
    // word explicitly instead of relying on the buffer's byte order.
    pio_sm_set_enabled(g_pio, g_sm, false);
    pio_sm_clear_fifos(g_pio, g_sm);
    pio_sm_restart(g_pio, g_sm);
    display_select_pins();
    pio_set_frame_bits(16);
    pio_sm_set_enabled(g_pio, g_sm, true);
    for (uint32_t i = 0; i < pixels; ++i) {
        uint16_t pixel = (uint16_t)px_map[i * 2u] |
                         ((uint16_t)px_map[i * 2u + 1u] << 8);
        pio_sm_put_blocking(g_pio, g_sm, (uint32_t)pixel << 16);
    }
    while (!pio_sm_is_tx_fifo_empty(g_pio, g_sm)) {
        tight_loop_contents();
    }
    sleep_us(1);
    gpio_put(PIN_DISP_CS, 1);
    pio_sm_set_enabled(g_pio, g_sm, false);
    pio_sm_clear_fifos(g_pio, g_sm);
    pio_sm_restart(g_pio, g_sm);

    lv_display_flush_ready(disp);
}
