#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

// --- Board / Controller Configuration ---
#define CFG_TUSB_MCU                OPT_MCU_RP2040
#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_HOST | OPT_MODE_FULL_SPEED)
#define CFG_TUSB_OS                 OPT_OS_NONE

// --- Host Mode Configurations ---
#define CFG_TUH_ENABLED             1
#define CFG_TUH_MAX_SPEED           OPT_MODE_FULL_SPEED

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
