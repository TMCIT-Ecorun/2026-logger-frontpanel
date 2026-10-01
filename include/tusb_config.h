#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

// --- Board / Controller Configuration ---
#define CFG_TUSB_MCU                OPT_MCU_RP2040
#define CFG_TUSB_OS                 OPT_OS_NONE

// The RP2040 has one native USB controller. TinyUSB 0.18 supports selecting
// its role at runtime, so both stacks are compiled in and the application
// switches port 0 from device to host during startup.
#define CFG_TUD_ENABLED             1
#define CFG_TUH_ENABLED             1
#define TUD_OPT_RHPORT              0
#define TUH_OPT_RHPORT              0
#define CFG_TUD_MAX_SPEED           OPT_MODE_FULL_SPEED
#define CFG_TUH_MAX_SPEED           OPT_MODE_FULL_SPEED

// --- Device CDC Configuration ---
#define CFG_TUD_ENDPOINT0_SIZE      64
#define CFG_TUD_CDC                 1
#define CFG_TUD_CDC_NOTIFY          1
#define CFG_TUD_CDC_RX_BUFSIZE      512
#define CFG_TUD_CDC_TX_BUFSIZE      512
#define CFG_TUD_CDC_EP_BUFSIZE      64

// Host CDC Class Config
#define CFG_TUH_CDC                 1
#define CFG_TUH_CDC_RX_BUFSIZE      512
#define CFG_TUH_CDC_TX_BUFSIZE      512

// Endpoint & Hub Limits
#define CFG_TUH_HUB                 1
#define CFG_TUH_DEVICE_MAX          (1 + CFG_TUH_HUB*4)
#define CFG_TUH_ENDPOINT_MAX        16

#ifdef __cplusplus
}
#endif

#endif // _TUSB_CONFIG_H_
