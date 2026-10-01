#ifndef ECORUN_HFP_CLIENT_H
#define ECORUN_HFP_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include "gui/platform.h"

/* Initialise BTstack's HFP Hands-Free profile. Call after cyw43_arch_init(). */
void hfp_client_init(void);

/* Connect to an Audio Gateway using a Bluetooth address such as
 * "AA:BB:CC:DD:EE:FF". */
bool hfp_client_connect(const char *address);
void hfp_client_disconnect(void);

bool hfp_client_is_connected(void);
bool hfp_client_is_audio_connected(void);

bool hfp_client_answer(void);
bool hfp_client_hangup(void);
bool hfp_client_dial(const char *number);
bool hfp_client_audio_start(void);
bool hfp_client_audio_stop(void);

/* Start/collect a Classic Bluetooth inquiry scan. The first call starts a
 * scan and may return zero; subsequent calls return the current result list. */
int hfp_client_scan(GuiBluetoothDevice *devices, int max_devices);
bool hfp_client_scan_in_progress(void);

#endif
