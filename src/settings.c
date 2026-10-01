#include "settings.h"

#include <stdio.h>
#include <string.h>
#include <cJSON.h>

#include "hardware/flash.h"
#include "pico/stdlib.h"

#ifndef PICO_FLASH_SIZE_BYTES
#error "PICO_FLASH_SIZE_BYTES must be defined by the selected Pico board"
#endif

#define SETTINGS_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define SETTINGS_MAGIC 0x46504346u /* FPCF */
#define SETTINGS_VERSION 1u
#define SETTINGS_JSON_MAX 1024u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t json_len;
    uint32_t checksum;
} SettingsHeader;

static uint32_t fnv1a32(const uint8_t *data, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

void settings_set_defaults(FrontPanelSettings *settings) {
    memset(settings, 0, sizeof(*settings));
    settings->brightness = 100;
    settings->usb_debug_enabled = true;
    touch_xpt2046_set_default_calibration(&settings->touch);
}

bool settings_load(FrontPanelSettings *settings) {
    settings_set_defaults(settings);

    const uint8_t *flash = (const uint8_t *)(XIP_BASE + SETTINGS_FLASH_OFFSET);
    const SettingsHeader *header = (const SettingsHeader *)flash;
    if (header->magic != SETTINGS_MAGIC ||
        header->version != SETTINGS_VERSION ||
        header->json_len == 0 ||
        header->json_len > SETTINGS_JSON_MAX ||
        sizeof(SettingsHeader) + header->json_len > FLASH_SECTOR_SIZE) {
        return false;
    }

    const char *json_text = (const char *)(flash + sizeof(SettingsHeader));
    if (fnv1a32((const uint8_t *)json_text, header->json_len) != header->checksum) {
        return false;
    }

    cJSON *root = cJSON_ParseWithLength(json_text, header->json_len);
    if (root == NULL) {
        return false;
    }

    cJSON *brightness = cJSON_GetObjectItemCaseSensitive(root, "brightness");
    cJSON *usb_debug = cJSON_GetObjectItemCaseSensitive(root, "usb_debug_enabled");
    cJSON *touch = cJSON_GetObjectItemCaseSensitive(root, "touch");
    cJSON *x_min = touch ? cJSON_GetObjectItemCaseSensitive(touch, "raw_x_min") : NULL;
    cJSON *x_max = touch ? cJSON_GetObjectItemCaseSensitive(touch, "raw_x_max") : NULL;
    cJSON *y_min = touch ? cJSON_GetObjectItemCaseSensitive(touch, "raw_y_min") : NULL;
    cJSON *y_max = touch ? cJSON_GetObjectItemCaseSensitive(touch, "raw_y_max") : NULL;

    if (cJSON_IsNumber(brightness) && brightness->valueint >= 0 && brightness->valueint <= 100) {
        settings->brightness = (uint8_t)brightness->valueint;
    }
    if (cJSON_IsBool(usb_debug)) {
        settings->usb_debug_enabled = cJSON_IsTrue(usb_debug);
    }

    if (cJSON_IsNumber(x_min) && cJSON_IsNumber(x_max) &&
        cJSON_IsNumber(y_min) && cJSON_IsNumber(y_max) &&
        x_min->valueint < x_max->valueint &&
        y_min->valueint < y_max->valueint &&
        x_min->valueint >= 0 && x_max->valueint <= 4095 &&
        y_min->valueint >= 0 && y_max->valueint <= 4095) {
        settings->touch.raw_x_min = (uint16_t)x_min->valueint;
        settings->touch.raw_x_max = (uint16_t)x_max->valueint;
        settings->touch.raw_y_min = (uint16_t)y_min->valueint;
        settings->touch.raw_y_max = (uint16_t)y_max->valueint;
    } else {
        cJSON_Delete(root);
        return false;
    }

    cJSON_Delete(root);
    return true;
}

bool settings_save(const FrontPanelSettings *settings) {
    char json[SETTINGS_JSON_MAX];
    int json_len = snprintf(
        json, sizeof(json),
        "{\"version\":%u,\"brightness\":%u,\"usb_debug_enabled\":%s,\"touch\":{\"raw_x_min\":%u,\"raw_x_max\":%u,\"raw_y_min\":%u,\"raw_y_max\":%u}}",
        SETTINGS_VERSION,
        settings->brightness,
        settings->usb_debug_enabled ? "true" : "false",
        settings->touch.raw_x_min,
        settings->touch.raw_x_max,
        settings->touch.raw_y_min,
        settings->touch.raw_y_max);

    if (json_len <= 0 || (size_t)json_len > SETTINGS_JSON_MAX ||
        sizeof(SettingsHeader) + (size_t)json_len > FLASH_SECTOR_SIZE) {
        return false;
    }

    static uint8_t page[FLASH_PAGE_SIZE];
    static uint8_t sector[FLASH_SECTOR_SIZE];
    memset(sector, 0xFF, sizeof(sector));

    SettingsHeader header = {
        .magic = SETTINGS_MAGIC,
        .version = SETTINGS_VERSION,
        .json_len = (uint16_t)json_len,
        .checksum = fnv1a32((const uint8_t *)json, (size_t)json_len),
    };
    memcpy(sector, &header, sizeof(header));
    memcpy(sector + sizeof(header), json, (size_t)json_len);

    flash_range_erase(SETTINGS_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    for (size_t offset = 0; offset < sizeof(sector); offset += sizeof(page)) {
        memcpy(page, sector + offset, sizeof(page));
        flash_range_program(SETTINGS_FLASH_OFFSET + (uint32_t)offset, page, sizeof(page));
    }
    return true;
}
