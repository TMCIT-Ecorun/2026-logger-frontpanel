#ifndef SIM7070_H
#define SIM7070_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *apn;
    const char *username;
    const char *password;
    uint32_t baudrate;
    uint8_t uart_tx_pin;
    uint8_t uart_rx_pin;
} Sim7070Config;

void sim7070_init(const Sim7070Config *config);
bool sim7070_start_data_session(void);
bool sim7070_open_tcp(const char *host, uint16_t port, bool tls);
bool sim7070_close_tcp(void);
bool sim7070_send(const uint8_t *data, size_t length);
int sim7070_receive(uint8_t *data, size_t capacity);

#endif
