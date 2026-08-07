#include "touch_xpt2046.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"

void touch_xpt2046_init(void) {
    // Initialize SPI0 bus at 2MHz for Touch Controller
    spi_init(TOUCH_SPI_PORT, TOUCH_SPI_BAUDRATE);
    gpio_set_function(PIN_TOUCH_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_TOUCH_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_TOUCH_MISO, GPIO_FUNC_SPI);

    // CS Pin Init
    gpio_init(PIN_TOUCH_CS);
    gpio_set_dir(PIN_TOUCH_CS, GPIO_OUT);
    gpio_put(PIN_TOUCH_CS, 1);

    // IRQ Pin Init (Optional: input pin)
    gpio_init(PIN_TOUCH_IRQ);
    gpio_set_dir(PIN_TOUCH_IRQ, GPIO_IN);
    gpio_pull_up(PIN_TOUCH_IRQ);
}

static uint16_t xpt2046_cmd(uint8_t cmd) {
    uint8_t rx[2] = {0};

    gpio_put(PIN_TOUCH_CS, 0);
    spi_write_blocking(TOUCH_SPI_PORT, &cmd, 1);
    spi_read_blocking(TOUCH_SPI_PORT, 0x00, rx, 2);
    gpio_put(PIN_TOUCH_CS, 1);

    return ((rx[0] << 8) | rx[1]) >> 3;
}

void touch_xpt2046_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    // Check IRQ pin if pressed (Active LOW)
    if (gpio_get(PIN_TOUCH_IRQ) == 0) {
        uint16_t raw_x = xpt2046_cmd(0xD0);
        uint16_t raw_y = xpt2046_cmd(0x90);

        data->state = LV_INDEV_STATE_PRESSED;

        // Map raw ADC values to screen space (320x240)
        data->point.x = (int32_t)(raw_x - 300) * 320 / (3800 - 300);
        data->point.y = (int32_t)(raw_y - 300) * 240 / (3800 - 300);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
