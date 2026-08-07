#include "pico/stdlib.h"
#include <stdio.h>

int main() {
    stdio_init_all();

    sleep_ms(1500);

    while (true) {
        printf("hello usb cdc\r\n");
        fflush(stdout);
        sleep_ms(1000);
    }
}
