#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    LINEPODS_WIFI_FAILURE_UNKNOWN = 0,
    LINEPODS_WIFI_FAILURE_NOT_FOUND,
    LINEPODS_WIFI_FAILURE_AUTHENTICATION,
    LINEPODS_WIFI_FAILURE_COMPATIBILITY,
    LINEPODS_WIFI_FAILURE_SIGNAL,
} linepods_wifi_failure_t;

linepods_wifi_failure_t linepods_wifi_classify_reason(uint8_t reason);
void linepods_wifi_format_failure(uint8_t reason, char *destination, size_t capacity);
