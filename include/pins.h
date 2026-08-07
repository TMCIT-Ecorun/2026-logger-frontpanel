#ifndef PINS_H
#define PINS_H

// Hardware Pin Definitions
#define DISP_SPI_PORT     spi1
#define DISP_SPI_BAUDRATE 40 * 1000 * 1000
#define PIN_DISP_CS  15
#define PIN_DISP_DC  13
#define PIN_DISP_RST 14
#define PIN_DISP_BL  6
#define PIN_DISP_SCK 10
#define PIN_DISP_MOSI 11
#define PIN_DISP_MISO 8

#define TOUCH_SPI_PORT    spi0
#define TOUCH_SPI_BAUDRATE 2 * 1000 * 1000
#define PIN_TOUCH_CS 5
#define PIN_TOUCH_IRQ 1
#define PIN_TOUCH_SCK 2
#define PIN_TOUCH_MOSI 3
#define PIN_TOUCH_MISO 0

#endif
