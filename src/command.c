#include "tusb.h"
#include <cJSON.h>

static void send_racecapture_command(cJSON *cmd_json) {
    char *json_out = cJSON_PrintUnformatted(cmd_json);
    if (json_out != NULL) {
        if (tuh_cdc_mounted(0)) {
            tuh_cdc_write(0, json_out, strlen(json_out));
            tuh_cdc_write(0, "\r\n", 2);
            tuh_cdc_write_flush(0);
        }
        cJSON_free(json_out);
    }
}

void cmd_stop_logging(void) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return;

    cJSON_AddStringToObject(root, "action", "stopLogging");

    send_racecapture_command(root);
    cJSON_Delete(root);
}

void cmd_start_logging(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "action", "startLogging");

    send_racecapture_command(root);
    cJSON_Delete(root);
}

void cmd_set_sample_rate(const char *channel_name, int rate_hz) {
    cJSON *root = cJSON_CreateObject();
    cJSON *cfg  = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "action", "setConfig");
    cJSON_AddNumberToObject(cfg, channel_name, rate_hz);
    cJSON_AddItemToObject(root, "config", cfg);

    send_racecapture_command(root);
    cJSON_Delete(root);
}
