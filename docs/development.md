# Development notes

## 2026-09-27: ILI9341 / XPT2046 PIO migration

- Display wiring is already on the PIO path: ILI9341 uses PIO0/SM0 with DMA.
  - SCK: GPIO10
  - MOSI: GPIO11
  - CS: GPIO15
  - DC: GPIO13
  - RST: GPIO14
  - BL: GPIO6
- Touch wiring is also PIO now; XPT2046 shares the display SPI bus and no
  longer uses `hardware_spi`/SPI0.
  - SCK: GPIO10
  - MOSI/DIN: GPIO11
  - MISO/DO: GPIO8
  - CS: GPIO5
  - IRQ: not connected/used
- Touch uses PIO1/SM0 and a generated `src/touch_xpt2046.pio.h` header. The
  PIO transaction sends the 8-bit XPT2046 command plus 16 clocks of response
  data in SPI mode 0 at the configured 2 MHz rate.
- Touch detection is polled from the LVGL input callback; no XPT2046 IRQ/GPIO
  interrupt is configured or used. Pressure (Z1/Z2) is sampled to distinguish
  a touch from an untouched panel.
- While running, the touch driver prints one raw X/Y/Z sample every 500 ms to
  UART. A `fail` counter records PIO transactions that did not complete within
  the 250 us timeout. This separates a missing SPI/PIO response from a mapping
  or pressure threshold problem; an oscilloscope or logic analyzer is still
  required to confirm electrical SCK/MOSI/MISO/CS waveforms.
- Display remains 320x240 landscape with MADCTL `0x28`.
- Touch mapping currently swaps X/Y for the landscape orientation, inverts
  the screen Y axis, and uses the observed panel calibration range
  X=335..1764 / Y=245..1684 before mapping.
- Display speed API remains `display_ili9341_set_speed()`, default 40 MHz and
  maximum 62.5 MHz.

### Verification / next hardware step

- Build with `cmake --build build -j2` and run `git diff --check` after changes.
- The next required check is flashing the resulting UF2 to the Pico and
  confirming that the display no longer shows vertical stripes and that touch
  coordinates match the panel. If needed, adjust `TOUCH_INVERT_X/Y` or the
  raw calibration limits in `include/pins.h`.

## 2026-10-01: USB debug reliability and automatic host switch

- TinyUSB starts on RP2040 USB root port 0 as a CDC debug device.
- The original short debug window is preserved: if no PC opens the CDC
  interface within 3 seconds, the firmware automatically switches the root
  port to USB host mode.
- If a PC opens the CDC interface during that window, the device remains in CDC
  debug mode until an explicit `HOST` command is sent.
- Send the line `HOST` over the debug CDC connection when USB host mode is
  actually required. The device stack is then deinitialized and the same root
  port is initialized as a USB host.
- The CDC device descriptor uses the standard single-CDC device class instead
  of the composite/MISC class combination.
- RP2040 host operation also requires the hardware to provide USB VBUS to the
  downstream device. The TinyUSB RP2040 host driver assumes VBUS is supplied;
  firmware alone cannot make an arbitrary board provide 5 V on the USB port.

## 2026-09-30: SIM7070G WebSocket transport

- Added `src/sim7070.c` / `include/sim7070.h` for SIM7070G AT command control.
- The modem uses UART1 at 115200 bps with GPIO4=TX and GPIO9=RX.
- SIM7070G PWRKEY is wired to GPIO1. At boot GPIO1 is driven LOW for 1 second,
  then released to Hi-Z (input, with internal pulls disabled).
- The initial SORACOM configuration is `soracom.io` with `sora`/`sora` authentication.
  The APN and credentials are centralized in `src/main.c` for later replacement.
- Added `src/websocket_client.c` / `include/websocket_client.h`. It opens a SIM7070G
  TCP socket with receive URCs enabled, performs the WebSocket HTTP Upgrade, masks
  client frames, handles text/binary, ping/pong, and close frames, and forwards text
  JSON to the existing telemetry parser.
- The modem/TLS sequence now matches the working Python client in
  `ecorun2026/client/modem_manager.py`: CID 0/PDP context 1, `recv_mode=1`,
  `CASEND` with a 10-second input window, and SIM7070G TLS 1.2/SNI/cipher settings.
- The front panel endpoint is `wss://maruo-desktop.tailecc88.ts.net/ws/audio`.
