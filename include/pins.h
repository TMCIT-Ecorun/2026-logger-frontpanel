#ifndef PINS_H
#define PINS_H

// Hardware Pin Definitions
#define DISP_SPI_BAUDRATE 40 * 1000 * 1000
#define PIN_DISP_CS  15
#define PIN_DISP_DC  13
#define PIN_DISP_RST 14
#define PIN_DISP_BL  6
#define PIN_DISP_SCK 10
#define PIN_DISP_MOSI 11

#define PIN_TOUCH_CS 5
#define PIN_TOUCH_SCK 10
#define PIN_TOUCH_MOSI 11
#define PIN_TOUCH_MISO 8

// XPT2046 calibration/orientation for the 320x240 landscape framebuffer.
// The ILI9341 is configured with MADCTL=0x28 (MV=1), so the touch axes are
// exchanged relative to the portrait ADC axes.
#define TOUCH_SWAP_XY   1
#define TOUCH_INVERT_X  1
#define TOUCH_INVERT_Y  1
#define TOUCH_RAW_X_MIN 335
#define TOUCH_RAW_X_MAX 1764
#define TOUCH_RAW_Y_MIN 245
#define TOUCH_RAW_Y_MAX 1684
#define TOUCH_PRESSURE_THRESHOLD 1200

// SIM7070G test wiring. Replace these when the modem wiring is finalized.
#define SIM7070_UART_TX_PIN 4
#define SIM7070_UART_RX_PIN 9
#define SIM7070_UART_BAUDRATE 115200
#define SIM7070_PWRKEY_PIN 1

#endif
