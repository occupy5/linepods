#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define LINEPODS_SSID_MAX 32
#define LINEPODS_WIFI_PASSWORD_MAX 64
#define LINEPODS_API_KEY_MAX 160
#define LINEPODS_READER_ID_MAX 63

typedef struct {
    char ssid[LINEPODS_SSID_MAX + 1];
    char password[LINEPODS_WIFI_PASSWORD_MAX + 1];
    char api_key[LINEPODS_API_KEY_MAX + 1];
} linepods_config_t;

typedef struct {
    uint8_t browse_mode;
    char book_id[LINEPODS_READER_ID_MAX + 1];
    char quote_id[LINEPODS_READER_ID_MAX + 1];
} linepods_reader_state_t;

bool linepods_config_valid(const linepods_config_t *config);
esp_err_t linepods_config_load(linepods_config_t *config);
esp_err_t linepods_config_save(const linepods_config_t *config);
esp_err_t linepods_config_load_last_sync(time_t *timestamp);
esp_err_t linepods_config_save_last_sync(time_t timestamp);
esp_err_t linepods_config_load_reader_state(linepods_reader_state_t *state);
esp_err_t linepods_config_save_reader_state(const linepods_reader_state_t *state);
esp_err_t linepods_config_clear(void);
