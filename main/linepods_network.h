#pragma once

#include "esp_err.h"
#include "linepods_config.h"
#include <stdbool.h>
#include <stddef.h>

typedef bool (*linepods_provision_callback_t)(const linepods_config_t *config,
                                             void *user);

esp_err_t linepods_network_init(void);
esp_err_t linepods_network_connect(const linepods_config_t *config,
                                 int timeout_ms);
/* Stop the long-lived Wi-Fi driver after all network work and provisioning
 * have finished. A later linepods_network_connect()/start_provisioning() call
 * starts it again on demand. */
esp_err_t linepods_network_suspend_radio(void);
bool linepods_network_is_provisioning(void);
void linepods_network_format_connect_error(esp_err_t error,
                                         char *destination, size_t capacity);
esp_err_t linepods_network_start_provisioning(linepods_provision_callback_t callback,
                                             void *user);
void linepods_network_stop_provisioning(void);
const char *linepods_network_setup_ssid(void);
const char *linepods_network_setup_password(void);
