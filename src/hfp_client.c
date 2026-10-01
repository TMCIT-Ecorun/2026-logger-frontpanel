#include "hfp_client.h"

#include <stdio.h>
#include <string.h>

#include "btstack.h"
#include "classic/hfp_hf.h"

static uint8_t hfp_service_buffer[150];
static const uint8_t hfp_rfcomm_channel = 1;
static const uint8_t hfp_codecs[] = { HFP_CODEC_CVSD };
static const uint16_t hfp_indicators[] = { 0x01, 0x02 };

static hci_con_handle_t acl_handle = HCI_CON_HANDLE_INVALID;
static hci_con_handle_t sco_handle = HCI_CON_HANDLE_INVALID;
static bool hfp_initialized;
static bool scan_in_progress;
static bool scan_complete;
static GuiBluetoothDevice scan_devices[12];
static int scan_device_count;

static const uint16_t hfp_supported_features =
    (1u << HFP_HFSF_ESCO_S4) |
    (1u << HFP_HFSF_CLI_PRESENTATION_CAPABILITY) |
    (1u << HFP_HFSF_HF_INDICATORS) |
    (1u << HFP_HFSF_CODEC_NEGOTIATION) |
    (1u << HFP_HFSF_ENHANCED_CALL_STATUS) |
    (1u << HFP_HFSF_EC_NR_FUNCTION) |
    (1u << HFP_HFSF_REMOTE_VOLUME_CONTROL);

static void hfp_hci_packet_handler(uint8_t packet_type, uint16_t channel,
                                   uint8_t *packet, uint16_t size) {
    (void)channel;

    if (packet_type != HCI_EVENT_PACKET) {
        /* Incoming SCO audio is intentionally discarded until the real audio
         * input/output path is connected. */
        return;
    }

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                bd_addr_t local_addr;
                gap_local_bd_addr(local_addr);
                printf("[HFP] BTstack ready on %s\n", bd_addr_to_str(local_addr));
            }
            break;

        case HCI_EVENT_SCO_CAN_SEND_NOW: {
            if (sco_handle == HCI_CON_HANDLE_INVALID) break;

            int packet_length = hci_get_sco_packet_length_for_connection(sco_handle);
            if (packet_length < 3) break;

            hci_reserve_packet_buffer();
            uint8_t *sco_packet = hci_get_outgoing_packet_buffer();
            const int payload_length = packet_length - 3;

            /* CVSD carries 16-bit PCM samples in this HCI transport. Send
             * silence until the microphone path is attached. */
            memset(&sco_packet[3], 0, (size_t)payload_length);
            little_endian_store_16(sco_packet, 0, sco_handle);
            sco_packet[2] = (uint8_t)payload_length;
            hci_send_sco_packet_buffer(packet_length);
            hci_request_sco_can_send_now_event_for_con_handle(sco_handle);
            break;
        }

        default:
            break;
    }
}

static void hfp_packet_handler(uint8_t packet_type, uint16_t channel,
                               uint8_t *packet, uint16_t size) {
    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_HFP_META) return;

    switch (hci_event_hfp_meta_get_subevent_code(packet)) {
        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED:
            if (hfp_subevent_service_level_connection_established_get_status(packet) == ERROR_CODE_SUCCESS) {
                acl_handle = hfp_subevent_service_level_connection_established_get_acl_handle(packet);
                printf("[HFP] Service level connection established\n");
            } else {
                printf("[HFP] Service level connection failed: 0x%02x\n",
                       hfp_subevent_service_level_connection_established_get_status(packet));
            }
            break;

        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED:
            acl_handle = HCI_CON_HANDLE_INVALID;
            sco_handle = HCI_CON_HANDLE_INVALID;
            printf("[HFP] Service level connection released\n");
            break;

        case HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED:
            if (hfp_subevent_audio_connection_established_get_status(packet) == ERROR_CODE_SUCCESS) {
                sco_handle = hfp_subevent_audio_connection_established_get_sco_handle(packet);
                printf("[HFP] Audio connection established (SCO 0x%04x)\n", sco_handle);
                hci_request_sco_can_send_now_event_for_con_handle(sco_handle);
            } else {
                printf("[HFP] Audio connection failed: 0x%02x\n",
                       hfp_subevent_audio_connection_established_get_status(packet));
            }
            break;

        case HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED:
            sco_handle = HCI_CON_HANDLE_INVALID;
            printf("[HFP] Audio connection released\n");
            break;

        case HFP_SUBEVENT_CALL_ANSWERED:
            printf("[HFP] Call answered\n");
            break;

        case HFP_SUBEVENT_CALL_TERMINATED:
            printf("[HFP] Call terminated\n");
            break;

        default:
            break;
    }
}

static void hfp_scan_packet_handler(uint8_t packet_type, uint16_t channel,
                                    uint8_t *packet, uint16_t size) {
    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t event = hci_event_packet_get_type(packet);
    if (event == GAP_EVENT_INQUIRY_RESULT && scan_device_count < 12) {
        bd_addr_t addr;
        gap_event_inquiry_result_get_bd_addr(packet, addr);
        GuiBluetoothDevice *d = &scan_devices[scan_device_count];
        snprintf(d->address, sizeof(d->address), "%s", bd_addr_to_str(addr));
        if (gap_event_inquiry_result_get_name_available(packet)) {
            int len = gap_event_inquiry_result_get_name_len(packet);
            if (len >= (int)sizeof(d->name)) len = (int)sizeof(d->name) - 1;
            memcpy(d->name, gap_event_inquiry_result_get_name(packet), (size_t)len);
            d->name[len] = '\0';
        } else {
            snprintf(d->name, sizeof(d->name), "%s", d->address);
        }
        scan_device_count++;
    } else if (event == GAP_EVENT_INQUIRY_COMPLETE) {
        scan_in_progress = false;
        scan_complete = true;
    }
}

void hfp_client_init(void) {
    if (hfp_initialized) return;

    l2cap_init();
    rfcomm_init();
    sdp_init();

    if (hfp_hf_init(hfp_rfcomm_channel) != ERROR_CODE_SUCCESS) {
        printf("[HFP] hfp_hf_init failed\n");
        return;
    }

    hfp_hf_init_supported_features(hfp_supported_features);
    hfp_hf_init_hf_indicators(sizeof(hfp_indicators) / sizeof(hfp_indicators[0]), hfp_indicators);
    hfp_hf_init_codecs(sizeof(hfp_codecs), hfp_codecs);
    hfp_hf_register_packet_handler(hfp_packet_handler);

    memset(hfp_service_buffer, 0, sizeof(hfp_service_buffer));
    hfp_hf_create_sdp_record_with_codecs(
        hfp_service_buffer,
        sdp_create_service_record_handle(),
        hfp_rfcomm_channel,
        "EcoRun HFP HF",
        hfp_supported_features,
        sizeof(hfp_codecs),
        hfp_codecs);
    sdp_register_service(hfp_service_buffer);

    gap_set_local_name("EcoRun HFP HF 00:00:00:00:00:00");
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);
    gap_set_allow_role_switch(true);

    static btstack_packet_callback_registration_t hci_registration;
    hci_registration.callback = hfp_hci_packet_handler;
    hci_add_event_handler(&hci_registration);
    hci_register_sco_packet_handler(hfp_hci_packet_handler);

    static btstack_packet_callback_registration_t scan_registration;
    scan_registration.callback = hfp_scan_packet_handler;
    hci_add_event_handler(&scan_registration);

    hci_power_control(HCI_POWER_ON);
    hfp_initialized = true;
}

int hfp_client_scan(GuiBluetoothDevice *devices, int max_devices) {
    if (!hfp_initialized || devices == NULL || max_devices <= 0) return -1;
    if (!scan_in_progress && !scan_complete) {
        scan_device_count = 0;
        scan_in_progress = true;
        gap_inquiry_start(4);
    }
    int count = scan_device_count;
    if (count > max_devices) count = max_devices;
    for (int i = 0; i < count; ++i) devices[i] = scan_devices[i];
    return count;
}

bool hfp_client_scan_in_progress(void) {
    return scan_in_progress;
}

bool hfp_client_connect(const char *address) {
    if (!hfp_initialized || !address) return false;
    bd_addr_t bd_addr;
    if (sscanf_bd_addr(address, bd_addr) != 1) return false;
    return hfp_hf_establish_service_level_connection(bd_addr) == ERROR_CODE_SUCCESS;
}

void hfp_client_disconnect(void) {
    if (acl_handle != HCI_CON_HANDLE_INVALID) {
        hfp_hf_release_service_level_connection(acl_handle);
    }
}

bool hfp_client_is_connected(void) {
    return acl_handle != HCI_CON_HANDLE_INVALID;
}

bool hfp_client_is_audio_connected(void) {
    return sco_handle != HCI_CON_HANDLE_INVALID;
}

bool hfp_client_answer(void) {
    return hfp_client_is_connected() &&
           hfp_hf_answer_incoming_call(acl_handle) == ERROR_CODE_SUCCESS;
}

bool hfp_client_hangup(void) {
    return hfp_client_is_connected() &&
           hfp_hf_terminate_call(acl_handle) == ERROR_CODE_SUCCESS;
}

bool hfp_client_dial(const char *number) {
    if (!hfp_client_is_connected() || !number || !number[0]) return false;
    return hfp_hf_dial_number(acl_handle, (char *)number) == ERROR_CODE_SUCCESS;
}

bool hfp_client_audio_start(void) {
    return hfp_client_is_connected() &&
           hfp_hf_establish_audio_connection(acl_handle) == ERROR_CODE_SUCCESS;
}

bool hfp_client_audio_stop(void) {
    return hfp_client_is_audio_connected() &&
           hfp_hf_release_audio_connection(acl_handle) == ERROR_CODE_SUCCESS;
}
