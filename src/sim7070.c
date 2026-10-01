#include "sim7070.h"

#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/time.h"

#define SIM_UART uart1
#define SIM_CID 0
#define SIM_PDP_INDEX 1
#define SIM_RECV_MODE 1
#define SIM_MAX_DATA 1460

static Sim7070Config g_config;
static bool g_initialized;
static bool g_data_active;
static bool g_tcp_open;

static bool read_byte_until(uint8_t *out, absolute_time_t deadline) {
    while (!time_reached(deadline)) {
        if (uart_is_readable(SIM_UART)) {
            *out = uart_getc(SIM_UART);
            return true;
        }
        tight_loop_contents();
    }
    return false;
}

static bool at_command(const char *command, char *response, size_t response_size, uint32_t timeout_ms) {
    while (uart_is_readable(SIM_UART)) (void)uart_getc(SIM_UART);
    uart_write_blocking(SIM_UART, (const uint8_t *)command, strlen(command));
    uart_putc_raw(SIM_UART, '\r');

    size_t used = 0;
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    uint8_t byte;
    while (read_byte_until(&byte, deadline)) {
        if (used + 1 < response_size) {
            response[used++] = (char)byte;
            response[used] = '\0';
        }
        if (used >= 2 && response[used - 2] == 'O' && response[used - 1] == 'K') return true;
        if (used >= 5 && strstr(response + used - 5, "ERROR") != NULL) return false;
    }
    return false;
}

static bool wait_for_ok(uint32_t timeout_ms) {
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    char response[128] = {0};
    size_t used = 0;
    uint8_t byte;
    while (read_byte_until(&byte, deadline)) {
        if (used + 1 < sizeof(response)) {
            response[used++] = (char)byte;
            response[used] = '\0';
        }
        if (used >= 2 && response[used - 2] == 'O' && response[used - 1] == 'K') return true;
        if (used >= 5 && strstr(response + used - 5, "ERROR") != NULL) return false;
    }
    return false;
}

void sim7070_init(const Sim7070Config *config) {
    g_config = *config;
    uart_init(SIM_UART, g_config.baudrate);
    gpio_set_function(g_config.uart_tx_pin, GPIO_FUNC_UART);
    gpio_set_function(g_config.uart_rx_pin, GPIO_FUNC_UART);
    uart_set_hw_flow(SIM_UART, false, false);
    uart_set_format(SIM_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(SIM_UART, true);
    g_initialized = true;
    g_data_active = false;
    g_tcp_open = false;
}

bool sim7070_start_data_session(void) {
    if (!g_initialized) return false;
    char response[512];
    char command[160];
    if (!at_command("AT", response, sizeof(response), 2000)) return false;
    (void)at_command("ATE0", response, sizeof(response), 2000);
    (void)at_command("AT+CMEE=2", response, sizeof(response), 2000);
    if (!at_command("AT+CFUN=1", response, sizeof(response), 5000)) return false;

    int written = snprintf(command, sizeof(command), "AT+CGDCONT=%u,\"IP\",\"%s\"",
                           SIM_PDP_INDEX, g_config.apn);
    if (written < 0 || (size_t)written >= sizeof(command) ||
        !at_command(command, response, sizeof(response), 3000)) return false;

    if (g_config.username && g_config.password && g_config.username[0]) {
        written = snprintf(command, sizeof(command), "AT+CGAUTH=%u,1,\"%s\",\"%s\"",
                           SIM_PDP_INDEX, g_config.username, g_config.password);
        if (written < 0 || (size_t)written >= sizeof(command) ||
            !at_command(command, response, sizeof(response), 3000)) return false;
    }

    /* Match the Python client: PDP context 1 is activated on CID 0. */
    (void)at_command("AT+CNACT=0,0", response, sizeof(response), 5000);
    written = snprintf(command, sizeof(command), "AT+CNACT=0,1");
    if (written < 0 || (size_t)written >= sizeof(command) ||
        !at_command(command, response, sizeof(response), 60000)) return false;
    g_data_active = true;
    return true;
}

bool sim7070_open_tcp(const char *host, uint16_t port, bool tls) {
    if (!g_data_active || !host) return false;
    char command[180];
    char response[256];

    (void)at_command("AT+CACLOSE=0", response, sizeof(response), 5000);
    if (tls) {
        int written = snprintf(command, sizeof(command), "AT+CSSLCFG=\"SSLVERSION\",0,3");
        if (written < 0 || (size_t)written >= sizeof(command) ||
            !at_command(command, response, sizeof(response), 5000)) return false;
        written = snprintf(command, sizeof(command), "AT+CSSLCFG=\"IGNORERTCTIME\",0,1");
        if (written < 0 || (size_t)written >= sizeof(command) ||
            !at_command(command, response, sizeof(response), 5000)) return false;
        written = snprintf(command, sizeof(command), "AT+CSSLCFG=\"SNI\",0,\"%s\"", host);
        if (written < 0 || (size_t)written >= sizeof(command) ||
            !at_command(command, response, sizeof(response), 5000)) return false;
        written = snprintf(command, sizeof(command), "AT+CSSLCFG=\"CIPHERSUITE\",0,0,0xC02B");
        if (written < 0 || (size_t)written >= sizeof(command) ||
            !at_command(command, response, sizeof(response), 5000)) return false;
        if (!at_command("AT+CASSLCFG=0,\"SSL\",1", response, sizeof(response), 5000)) return false;
        if (!at_command("AT+CASSLCFG=0,\"CRINDEX\",0", response, sizeof(response), 5000)) return false;
    }

    int written = snprintf(command, sizeof(command),
                           "AT+CAOPEN=%u,%u,\"TCP\",\"%s\",%u,%u",
                           SIM_CID, 0u, host, port, SIM_RECV_MODE);
    if (written < 0 || (size_t)written >= sizeof(command) ||
        !at_command(command, response, sizeof(response), 60000)) return false;
    g_tcp_open = strstr(response, "+CAOPEN: 0,0") != NULL;
    return g_tcp_open;
}

bool sim7070_close_tcp(void) {
    if (!g_tcp_open) return true;
    char response[128];
    bool ok = at_command("AT+CACLOSE=0", response, sizeof(response), 10000);
    g_tcp_open = false;
    return ok;
}

bool sim7070_send(const uint8_t *data, size_t length) {
    if (!g_tcp_open || !data || length == 0 || length > SIM_MAX_DATA) return false;
    char command[64];
    char response[128];
    int written = snprintf(command, sizeof(command), "AT+CASEND=%u,%u,10000", SIM_CID, (unsigned)length);
    if (written < 0 || (size_t)written >= sizeof(command)) return false;
    while (uart_is_readable(SIM_UART)) (void)uart_getc(SIM_UART);
    uart_write_blocking(SIM_UART, (const uint8_t *)command, strlen(command));
    uart_putc_raw(SIM_UART, '\r');
    /* CASEND first returns a prompt/length response; accept OK only after data is sent. */
    absolute_time_t deadline = make_timeout_time_ms(3000);
    uint8_t byte;
    size_t used = 0;
    while (read_byte_until(&byte, deadline)) {
        if (used + 1 < sizeof(response)) response[used++] = (char)byte;
        if (byte == '>') break;
        if (byte == '\n' && used >= 5 && strstr(response, "ERROR")) return false;
    }
    if (byte != '>') return false;
    uart_write_blocking(SIM_UART, data, length);
    return wait_for_ok(10000);
}

int sim7070_receive(uint8_t *data, size_t capacity) {
    if (!g_tcp_open || !data || capacity == 0) return -1;
    if (capacity > SIM_MAX_DATA) capacity = SIM_MAX_DATA;

    const char prefix[] = "+CAURC: \"recv\",0,";
    size_t matched = 0;
    absolute_time_t deadline = make_timeout_time_ms(15000);
    uint8_t byte;
    while (matched < sizeof(prefix) - 1) {
        if (!read_byte_until(&byte, deadline)) return 0;
        matched = (byte == (uint8_t)prefix[matched]) ? matched + 1 : 0;
    }

    size_t length = 0;
    bool have_digit = false;
    while (true) {
        if (!read_byte_until(&byte, deadline)) return -1;
        if (byte >= '0' && byte <= '9') {
            have_digit = true;
            length = length * 10u + (size_t)(byte - '0');
            if (length > capacity) return -1;
        } else if (byte == '\r' && have_digit) {
            if (!read_byte_until(&byte, deadline) || byte != '\n') return -1;
            break;
        } else {
            return -1;
        }
    }
    for (size_t i = 0; i < length; ++i) {
        if (!read_byte_until(&data[i], deadline)) return -1;
    }
    if (!read_byte_until(&byte, deadline) || byte != '\r') return -1;
    if (!read_byte_until(&byte, deadline) || byte != '\n') return -1;
    return (int)length;
}
