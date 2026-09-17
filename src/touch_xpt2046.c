#include "display_ili9341.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"

void display_ili9341_init(void) {
  // Initialize SPI1 bus at 40MHz for Display
  spi_init(DISP_SPI_PORT, DISP_SPI_BAUDRATE);
  gpio_set_function(PIN_DISP_SCK,GPIO_FUNC_SPI);
  gpio_set_function(PIN_DISP_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(PIN_DISP_MISO, GPIO_FUNC_SPI);

  // Initialize control GPIO pins
  gpio_init(PIN_DISP_CS);
  gpio_set_dir(PIN_DISP_CS,GPIO_OUT);
  gpio_put(PIN_DISP_CS, 1);

  gpio_init(PIN_DISP_DC);
  gpio_set_dir(PIN_DISP_DC,GPIO_OUT);
  gpio_put(PIN_DISP_DC, 0);

  gpio_init(PIN_DISP_RST);
  gpio_set_dir(PIN_DISP_RST, GPIO_OUT);
  gpio_put(PIN_DISP_RST, 0);

  sleep_ms(100);
  gpio_put(PIN_DISP_RST, 1);

  // Initialize display
  write_cmd(0x11); // Sleep out
  sleep_ms(120);
  write_cmd(0xB1); // Frame rate control
  write_data((uint8_t[]){0x00, 0x1B}, 2);
  write_cmd(0xB2); // Frame rate control (in idle mode)
  write_data((uint8_t[]){0x00, 0x1B}, 2);
  write_cmd(0xB3); // Frame rate control (in partial mode)
  write_data((uint8_t[]){0x00, 0x1B, 0x00, 0x1B}, 4);
  write_cmd(0xC0); // Power control 1
  write_data((uint8_t[]){0x1F, 0x23, 0x04, 0x2B, 0x3F, 0x00}, 6);
  write_cmd(0xC1); // Power control 2
  write_data((uint8_t[]){0xC0, 0x00, 0x00}, 3);
  write_cmd(0xC5); // VCOM control 1
  write_data((uint8_t[]){0x30, 0x30}, 2);
  write_cmd(0xC7); // VCOM control 2
  write_data((uint8_t[]){0x10}, 1);
  write_cmd(0x3A); // Interface pixel format
  write_data((uint8_t[]){0x55}, 1);
  write_cmd(0x29); // Display on
}

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
