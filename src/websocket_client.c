#include "websocket_client.h"

#include <stdio.h>
#include <string.h>

#include "pico/time.h"
#include "sim7070.h"

typedef enum { WS_OFFLINE, WS_TCP, WS_HANDSHAKE, WS_CONNECTED, WS_BACKOFF } WsState;

static WebSocketConfig g_config;
static WsState g_state;
static uint32_t g_state_since;
static uint8_t g_rx[1460];
static char g_http[1024];

extern void parse_telemetry_json_from_websocket(const char *json);

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }
static void set_state(WsState state) { g_state = state; g_state_since = now_ms(); }

static bool send_frame(uint8_t opcode, const uint8_t *payload, size_t length) {
    if (length > 125 || (length && !payload)) return false;
    uint8_t frame[131];
    static const uint8_t mask[4] = {0x73, 0x42, 0x19, 0xA7};
    frame[0] = (uint8_t)(0x80u | opcode);
    frame[1] = (uint8_t)(0x80u | length);
    memcpy(&frame[2], mask, sizeof(mask));
    for (size_t i = 0; i < length; ++i) frame[6 + i] = payload[i] ^ mask[i & 3u];
    return sim7070_send(frame, 6 + length);
}

static bool handshake(void) {
    char request[512];
    int written = snprintf(request, sizeof(request),
                           "GET %s HTTP/1.1\r\nHost: %s\r\n"
                           "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                           "Sec-WebSocket-Key: ZWNvcnVuLWZyb250LXBhbg==\r\n"
                           "Sec-WebSocket-Version: 13\r\n\r\n",
                           g_config.path, g_config.host);
    if (written < 0 || (size_t)written >= sizeof(request) ||
        !sim7070_send((const uint8_t *)request, (size_t)written)) return false;

    size_t used = 0;
    uint32_t deadline = now_ms() + 10000;
    while ((int32_t)(now_ms() - deadline) < 0 && used + 1 < sizeof(g_http)) {
        int received = sim7070_receive(g_rx, sizeof(g_rx));
        if (received < 0) return false;
        if (received == 0) continue;
        size_t copy = (size_t)received;
        if (copy > sizeof(g_http) - 1 - used) copy = sizeof(g_http) - 1 - used;
        memcpy(g_http + used, g_rx, copy);
        used += copy;
        g_http[used] = '\0';
        if (strstr(g_http, "\r\n\r\n")) return strstr(g_http, " 101 ") != NULL;
    }
    return false;
}

static void handle_frame(const uint8_t *frame, size_t length) {
    if (length < 2) return;
    uint8_t opcode = frame[0] & 0x0Fu;
    bool masked = (frame[1] & 0x80u) != 0;
    size_t payload_length = frame[1] & 0x7Fu;
    size_t header = 2;
    if (payload_length == 126) {
        if (length < 4) return;
        payload_length = ((size_t)frame[2] << 8) | frame[3];
        header = 4;
    } else if (payload_length == 127) {
        return;
    }
    if (masked) {
        if (length < header + 4) return;
        header += 4;
    }
    if (payload_length >= sizeof(g_rx) || length < header + payload_length) return;

    if (opcode == 0x8) {
        (void)send_frame(0x8, NULL, 0);
        set_state(WS_BACKOFF);
        return;
    }
    if (opcode == 0x9) {
        (void)send_frame(0xA, frame + header, payload_length);
        return;
    }
    if (opcode != 0x1 && opcode != 0x2) return;

    const uint8_t *source = frame + header;
    if (masked) {
        const uint8_t *mask = frame + header - 4;
        for (size_t i = 0; i < payload_length; ++i) g_rx[i] = source[i] ^ mask[i & 3u];
    } else {
        memcpy(g_rx, source, payload_length);
    }
    if (opcode == 0x1) {
        g_rx[payload_length] = '\0';
        parse_telemetry_json_from_websocket((const char *)g_rx);
    } else if (g_config.on_binary) {
        g_config.on_binary(g_rx, payload_length);
    }
}

void websocket_client_init(const WebSocketConfig *config) {
    g_config = *config;
    set_state(WS_OFFLINE);
}

void websocket_client_task(void) {
    switch (g_state) {
    case WS_OFFLINE:
        if (sim7070_start_data_session()) set_state(WS_TCP);
        else set_state(WS_BACKOFF);
        break;
    case WS_TCP:
        if (sim7070_open_tcp(g_config.host, g_config.port, g_config.tls)) set_state(WS_HANDSHAKE);
        else set_state(WS_BACKOFF);
        break;
    case WS_HANDSHAKE:
        if (handshake()) set_state(WS_CONNECTED);
        else { (void)sim7070_close_tcp(); set_state(WS_BACKOFF); }
        break;
    case WS_CONNECTED: {
        int received = sim7070_receive(g_rx, sizeof(g_rx));
        if (received > 0) handle_frame(g_rx, (size_t)received);
        else if (received < 0) { (void)sim7070_close_tcp(); set_state(WS_BACKOFF); }
        break;
    }
    case WS_BACKOFF:
        if (now_ms() - g_state_since >= 5000) set_state(WS_OFFLINE);
        break;
    }
}

bool websocket_client_is_connected(void) { return g_state == WS_CONNECTED; }

bool websocket_client_send_text(const char *text, size_t length) {
    return g_state == WS_CONNECTED && send_frame(0x1, (const uint8_t *)text, length);
}

bool websocket_client_send_binary(const uint8_t *data, size_t length) {
    /* The Python audio client sends one Opus packet per binary WebSocket
     * message. At 8 kbps / 60 ms the encoded packet is below the 125-byte
     * short-frame limit used by send_frame(). */
    return g_state == WS_CONNECTED && send_frame(0x2, data, length);
}
