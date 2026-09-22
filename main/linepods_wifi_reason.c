#include "linepods_wifi_reason.h"

#include <stdio.h>

linepods_wifi_failure_t linepods_wifi_classify_reason(uint8_t reason)
{
    switch (reason) {
    case 201: /* WIFI_REASON_NO_AP_FOUND */
    case 210: /* WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY */
    case 211: /* WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD */
    case 212: /* WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD */
        return LINEPODS_WIFI_FAILURE_NOT_FOUND;

    case 2:   /* WIFI_REASON_AUTH_EXPIRE */
    case 15:  /* WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT */
    case 16:  /* WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT */
    case 23:  /* WIFI_REASON_802_1X_AUTH_FAILED */
    case 202: /* WIFI_REASON_AUTH_FAIL */
    case 204: /* WIFI_REASON_HANDSHAKE_TIMEOUT */
        return LINEPODS_WIFI_FAILURE_AUTHENTICATION;

    case 10:  /* WIFI_REASON_DISASSOC_PWRCAP_BAD */
    case 11:  /* WIFI_REASON_DISASSOC_SUPCHAN_BAD */
    case 13:  /* WIFI_REASON_IE_INVALID */
    case 18:  /* WIFI_REASON_GROUP_CIPHER_INVALID */
    case 19:  /* WIFI_REASON_PAIRWISE_CIPHER_INVALID */
    case 20:  /* WIFI_REASON_AKMP_INVALID */
    case 21:  /* WIFI_REASON_UNSUPP_RSN_IE_VERSION */
    case 22:  /* WIFI_REASON_INVALID_RSN_IE_CAP */
    case 24:  /* WIFI_REASON_CIPHER_SUITE_REJECTED */
    case 29:  /* WIFI_REASON_BAD_CIPHER_OR_AKM */
    case 203: /* WIFI_REASON_ASSOC_FAIL */
    case 205: /* WIFI_REASON_CONNECTION_FAIL */
    case 208: /* WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG */
    case 209: /* WIFI_REASON_SA_QUERY_TIMEOUT */
        return LINEPODS_WIFI_FAILURE_COMPATIBILITY;

    case 200: /* WIFI_REASON_BEACON_TIMEOUT */
        return LINEPODS_WIFI_FAILURE_SIGNAL;

    default:
        return LINEPODS_WIFI_FAILURE_UNKNOWN;
    }
}

void linepods_wifi_format_failure(uint8_t reason, char *destination, size_t capacity)
{
    if (!destination || capacity == 0) return;

    const char *message;
    switch (linepods_wifi_classify_reason(reason)) {
    case LINEPODS_WIFI_FAILURE_NOT_FOUND:
        message = "未找到兼容的 2.4 GHz Wi-Fi（原因码 %u）";
        break;
    case LINEPODS_WIFI_FAILURE_AUTHENTICATION:
        message = "Wi-Fi 密码或认证方式不匹配（原因码 %u）";
        break;
    case LINEPODS_WIFI_FAILURE_COMPATIBILITY:
        message = "路由器拒绝连接或安全模式不兼容（原因码 %u）";
        break;
    case LINEPODS_WIFI_FAILURE_SIGNAL:
        message = "Wi-Fi 信号中断，请靠近路由器（原因码 %u）";
        break;
    default:
        message = "Wi-Fi 连接失败（原因码 %u）";
        break;
    }
    (void)snprintf(destination, capacity, message, (unsigned)reason);
}
