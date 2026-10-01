#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*WebSocketBinaryCallback)(const uint8_t *data, size_t length);

typedef struct {
    const char *host;
    uint16_t port;
    const char *path;
    bool tls;
    WebSocketBinaryCallback on_binary;
} WebSocketConfig;

void websocket_client_init(const WebSocketConfig *config);
void websocket_client_task(void);
bool websocket_client_is_connected(void);
bool websocket_client_send_text(const char *text, size_t length);
bool websocket_client_send_binary(const uint8_t *data, size_t length);

#endif
