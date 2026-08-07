#include "display_ili9341.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"

static void write_cmd(uint8_t cmd) {
    gpio_put(PIN_DISP_DC, 0); // Command mode
    gpio_put(PIN_DISP_CS, 0);
    spi_write_blocking(DISP_SPI_PORT, &cmd, 1);
    gpio_put(PIN_DISP_CS, 1);
}

static void write_data(const uint8_t *data, size_t len) {
    gpio_put(PIN_DISP_DC, 1); // Data mode
    gpio_put(PIN_DISP_CS, 0);
    spi_write_blocking(DISP_SPI_PORT, data, len);
    gpio_put(PIN_DISP_CS, 1);
}

void display_ili9341_init(void) {
    // Initialize SPI1 bus at 40MHz for Display
    spi_init(DISP_SPI_PORT, DISP_SPI_BAUDRATE);
    gpio_set_function(PIN_DISP_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_DISP_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_DISP_MISO, GPIO_FUNC_SPI);

    // Initialize control GPIO pins
    gpio_init(PIN_DISP_CS);  gpio_set_dir(PIN_DISP_CS,  GPIO_OUT); gpio_put(PIN_DISP_CS, 1);
    gpio_init(PIN_DISP_DC);  gpio_set_dir(PIN_DISP_DC,  GPIO_OUT); gpio_put(PIN_DISP_DC, 0);
    gpio_init(PIN_DISP_RST); gpio_set_dir(PIN_DISP_RST, GPIO_OUT); gpio_put(PIN_DISP_RST, 1);

    // Enable backlight
    gpio_init(PIN_DISP_BL);  gpio_set_dir(PIN_DISP_BL,  GPIO_OUT); gpio_put(PIN_DISP_BL, 1);

    // Hardware reset sequence
    gpio_put(PIN_DISP_RST, 0);
    sleep_ms(10);
    gpio_put(PIN_DISP_RST, 1);
    sleep_ms(120);

    // System Register Init Sequence
    write_cmd(0x01); // Software Reset
    sleep_ms(5);
    write_cmd(0x28); // Display OFF

    // Memory Access Control
    write_cmd(0x36);
    uint8_t mac = 0x48;
    write_data(&mac, 1);

    // Pixel Format Set (RGB565)
    write_cmd(0x3A);
    uint8_t fmt = 0x55;
    write_data(&fmt, 1);

    write_cmd(0x11); // Exit Sleep
    sleep_ms(120);

    write_cmd(0x29); // Display ON
}

void display_ili9341_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint8_t data[4];

    // Set Column Address (CASET)
    write_cmd(0x2A);
    data[0] = (area->x1 >> 8) & 0xFF;
    data[1] = area->x1 & 0xFF;
    data[2] = (area->x2 >> 8) & 0xFF;
    data[3] = area->x2 & 0xFF;
    write_data(data, 4);

    // Set Page Address (PASET)
    write_cmd(0x2B);
    data[0] = (area->y1 >> 8) & 0xFF;
    data[1] = area->y1 & 0xFF;
    data[2] = (area->y2 >> 8) & 0xFF;
    data[3] = area->y2 & 0xFF;
    write_data(data, 4);

    // Memory Write (RAMWR)
    write_cmd(0x2C);

    // Transfer frame pixel buffer
    uint32_t size = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * 2;
    write_data(px_map, size);

    // Notify LVGL that flushing is completed
    lv_display_flush_ready(disp);
}
