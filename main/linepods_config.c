#include "linepods_config.h"

#include "nvs.h"
#include <string.h>

#define LINEPODS_CONFIG_NAMESPACE "linepods"
#define LINEPODS_CONFIG_VERSION 1

bool linepods_config_valid(const linepods_config_t *config)
{
    if (!config) return false;
    size_t ssid_length = strnlen(config->ssid, sizeof(config->ssid));
    size_t password_length = strnlen(config->password, sizeof(config->password));
    size_t key_length = strnlen(config->api_key, sizeof(config->api_key));
    return ssid_length > 0 && ssid_length <= LINEPODS_SSID_MAX
        && password_length <= LINEPODS_WIFI_PASSWORD_MAX
        && key_length > 4 && key_length <= LINEPODS_API_KEY_MAX
        && strncmp(config->api_key, "wrk-", 4) == 0;
}

static esp_err_t read_string(nvs_handle_t handle, const char *key,
                             char *destination, size_t capacity)
{
    size_t length = capacity;
    esp_err_t error = nvs_get_str(handle, key, destination, &length);
    if (error == ESP_OK && length > capacity) return ESP_ERR_INVALID_SIZE;
    return error;
}

esp_err_t linepods_config_load(linepods_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    *config = (linepods_config_t) {0};

    nvs_handle_t handle;
    esp_err_t error = nvs_open(LINEPODS_CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (error != ESP_OK) return error;

    uint8_t version = 0;
    error = nvs_get_u8(handle, "version", &version);
    if (error == ESP_OK && version != LINEPODS_CONFIG_VERSION) error = ESP_ERR_INVALID_VERSION;
    if (error == ESP_OK) error = read_string(handle, "ssid", config->ssid, sizeof(config->ssid));
    if (error == ESP_OK) error = read_string(handle, "wifi_pwd", config->password,
                                              sizeof(config->password));
    if (error == ESP_OK) error = read_string(handle, "api_key", config->api_key,
                                              sizeof(config->api_key));
    nvs_close(handle);

    if (error == ESP_OK && !linepods_config_valid(config)) error = ESP_ERR_INVALID_STATE;
    if (error != ESP_OK) *config = (linepods_config_t) {0};
    return error;
}

esp_err_t linepods_config_save(const linepods_config_t *config)
{
    if (!linepods_config_valid(config)) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t error = nvs_open(LINEPODS_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;

    error = nvs_set_u8(handle, "version", LINEPODS_CONFIG_VERSION);
    if (error == ESP_OK) error = nvs_set_str(handle, "ssid", config->ssid);
    if (error == ESP_OK) error = nvs_set_str(handle, "wifi_pwd", config->password);
    if (error == ESP_OK) error = nvs_set_str(handle, "api_key", config->api_key);
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t linepods_config_load_last_sync(time_t *timestamp)
{
    if (!timestamp) return ESP_ERR_INVALID_ARG;
    *timestamp = 0;

    nvs_handle_t handle;
    esp_err_t error = nvs_open(LINEPODS_CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (error != ESP_OK) return error;
    int64_t stored = 0;
    error = nvs_get_i64(handle, "last_sync", &stored);
    nvs_close(handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (error == ESP_OK && stored > 0) *timestamp = (time_t)stored;
    return error;
}

esp_err_t linepods_config_save_last_sync(time_t timestamp)
{
    if (timestamp <= 0) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t error = nvs_open(LINEPODS_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_set_i64(handle, "last_sync", (int64_t)timestamp);
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t linepods_config_clear(void)
{
    nvs_handle_t handle;
    esp_err_t error = nvs_open(LINEPODS_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_erase_all(handle);
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}
