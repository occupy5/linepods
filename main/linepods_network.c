#include "linepods_network.h"
#include "linepods_form.h"
#include "linepods_wifi_reason.h"

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT BIT1
#define WIFI_DISCONNECTED_BIT BIT2
#define WIFI_RETRY_LIMIT 5
#define CONFIG_FORM_MAX 512

static const char *TAG = "linepods_network";

static EventGroupHandle_t s_events;
static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static httpd_handle_t s_server;
static linepods_provision_callback_t s_callback;
static void *s_callback_user;
static bool s_initialized;
static bool s_radio_started;
static bool s_connecting;
static bool s_time_sync_started;
static unsigned s_retries;
static volatile uint8_t s_disconnect_reason;
static char s_setup_ssid[24];
static char s_setup_password[16];

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disconnected = data;
        xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(s_events, WIFI_DISCONNECTED_BIT);
        if (s_connecting && disconnected) {
            s_disconnect_reason = disconnected->reason;
            ESP_LOGW(TAG, "Station disconnected: reason=%u retry=%u/%u",
                     (unsigned)disconnected->reason, s_retries,
                     (unsigned)WIFI_RETRY_LIMIT);
        }
        if (s_connecting && s_retries++ < WIFI_RETRY_LIMIT) {
            (void)esp_wifi_connect();
        } else if (s_connecting) {
            xEventGroupSetBits(s_events, WIFI_FAILED_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_connecting = false;
        s_retries = 0;
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
    }
}

/* Start the Wi-Fi driver if it is currently suspended. Idempotent; callers may
 * invoke it on every network action so a suspended radio is restarted lazily. */
static esp_err_t ensure_radio_started(void)
{
    if (s_radio_started) return ESP_OK;

    esp_err_t error = esp_wifi_start();
    if (error != ESP_OK) return error;
    s_radio_started = true;

    /* Modem sleep lets the STA doze between DTIM beacons while still connected.
     * It is a tunable, not a correctness requirement: log and continue. */
    esp_err_t ps = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    if (ps != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi modem sleep unavailable: %s", esp_err_to_name(ps));
    }
    return ESP_OK;
}

static void start_time_sync(void)
{
    if (s_time_sync_started) return;
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_err_t error = esp_netif_sntp_init(&config);
    if (error == ESP_OK || error == ESP_ERR_INVALID_STATE) {
        s_time_sync_started = true;
        return;
    }
    /* Cached reading remains useful without a clock. A later connection will
     * retry initialization while the header keeps its explicit --:-- state. */
    ESP_LOGW(TAG, "SNTP initialization failed: %s", esp_err_to_name(error));
}

esp_err_t linepods_network_init(void)
{
    if (s_initialized) return ESP_OK;
    s_events = xEventGroupCreate();
    if (!s_events) return ESP_ERR_NO_MEM;

    esp_err_t error = esp_netif_init();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) return error;
    error = esp_event_loop_create_default();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) return error;

    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (!s_sta_netif || !s_ap_netif) return ESP_ERR_NO_MEM;

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    error = esp_wifi_init(&init);
    if (error != ESP_OK) return error;
    error = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL);
    if (error == ESP_OK) {
        error = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                           wifi_event, NULL);
    }
    if (error != ESP_OK) return error;
    error = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (error == ESP_OK) error = esp_wifi_set_mode(WIFI_MODE_STA);
    if (error != ESP_OK) return error;

    /* Keep the RF stack stopped until sync or provisioning actually needs it.
     * Cached reading and an RTC-backed clock do not require Wi-Fi after wake. */
    s_initialized = true;
    return ESP_OK;
}

esp_err_t linepods_network_suspend_radio(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    /* Never yank the driver out from under provisioning or an in-flight
     * connect; the caller retries once those finish. */
    if (s_server || s_connecting) return ESP_ERR_INVALID_STATE;
    if (!s_radio_started) return ESP_OK;

    /* Recreate SNTP on the next connection so every permitted network session
     * performs a fresh clock correction instead of waiting for its hourly timer. */
    if (s_time_sync_started) {
        esp_netif_sntp_deinit();
        s_time_sync_started = false;
    }
    esp_err_t error = esp_wifi_stop();
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi stop failed: %s", esp_err_to_name(error));
        return error;
    }
    s_radio_started = false;
    xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT | WIFI_FAILED_BIT
                        | WIFI_DISCONNECTED_BIT);
    s_disconnect_reason = 0;
    ESP_LOGI(TAG, "Wi-Fi radio suspended until the next network action");
    return ESP_OK;
}

bool linepods_network_is_provisioning(void)
{
    return s_server != NULL;
}

esp_err_t linepods_network_connect(const linepods_config_t *config, int timeout_ms)
{
    if (!s_initialized || !linepods_config_valid(config) || timeout_ms <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t error = ensure_radio_started();
    if (error != ESP_OK) return error;

    wifi_config_t station = {0};
    memcpy(station.sta.ssid, config->ssid, strlen(config->ssid));
    memcpy(station.sta.password, config->password, strlen(config->password));
    station.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    station.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    station.sta.threshold.rssi = -127;
    /* Also accept legacy WPA/WPA2 mixed routers while never accepting WEP. */
    station.sta.threshold.authmode = config->password[0]
                                   ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_OPEN;
    station.sta.pmf_cfg.capable = true;
    station.sta.pmf_cfg.required = false;
    station.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    s_connecting = false;
    xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT | WIFI_FAILED_BIT
                        | WIFI_DISCONNECTED_BIT);
    error = esp_wifi_disconnect();
    if (error == ESP_OK) {
        (void)xEventGroupWaitBits(s_events, WIFI_DISCONNECTED_BIT, pdTRUE,
                                  pdFALSE, pdMS_TO_TICKS(1000));
    }

    s_retries = 0;
    s_disconnect_reason = 0;
    xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT | WIFI_FAILED_BIT
                        | WIFI_DISCONNECTED_BIT);
    error = esp_wifi_set_config(WIFI_IF_STA, &station);
    if (error == ESP_OK) {
        s_connecting = true;
        error = esp_wifi_connect();
    }
    if (error != ESP_OK) {
        s_connecting = false;
        return error;
    }

    EventBits_t bits = xEventGroupWaitBits(
        s_events, WIFI_CONNECTED_BIT | WIFI_FAILED_BIT, pdTRUE, pdFALSE,
        pdMS_TO_TICKS(timeout_ms));
    s_connecting = false;
    if (bits & WIFI_CONNECTED_BIT) {
        start_time_sync();
        return ESP_OK;
    }
    return (bits & WIFI_FAILED_BIT) ? ESP_ERR_NOT_FOUND : ESP_ERR_TIMEOUT;
}

void linepods_network_format_connect_error(esp_err_t error,
                                         char *destination, size_t capacity)
{
    if (!destination || capacity == 0) return;
    if (s_disconnect_reason != 0) {
        linepods_wifi_format_failure(s_disconnect_reason, destination, capacity);
        return;
    }
    (void)snprintf(destination, capacity, "无法连接 Wi-Fi（%s）",
                   esp_err_to_name(error));
}

static esp_err_t page_handler(httpd_req_t *request)
{
    static const char PAGE[] =
        "<!doctype html><html lang=zh-CN><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>配置 Linepods</title><style>"
        "*{box-sizing:border-box}html{color-scheme:light}body{margin:0;background:#fff;"
        "color:#111;font:16px/1.6 -apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif}"
        "main{max-width:30rem;margin:auto;padding:40px 20px 56px}.brand{font-size:12px;"
        "font-weight:700;letter-spacing:.16em;color:#111}.card{margin-top:20px}"
        "h1{font-size:30px;line-height:1.2;letter-spacing:-.02em;margin:0 0 10px}"
        "p{margin:0 0 24px;color:#666}label{display:block;margin:20px 0 7px;"
        "font-weight:650}input{width:100%;border:1px solid #d8d8d8;border-radius:12px;"
        "background:#fff;padding:14px;font:inherit;color:#111;outline:none}"
        "input:focus{border-color:#111;box-shadow:0 0 0 1px #111}.hint{margin:24px 0 20px;"
        "padding:18px 0 0;border-top:1px solid #e5e5e5;color:#555}.hint strong{display:block;"
        "margin-bottom:5px;color:#111}.path{font-weight:650;color:#111}button{width:100%;"
        "margin-top:4px;border:0;border-radius:12px;background:#111;color:#fff;padding:15px;"
        "font:650 17px system-ui;cursor:pointer}.foot{text-align:center;font-size:13px;"
        "margin-top:16px;color:#666}</style><main><div class=brand>LINEPODS</div>"
        "<section class=card><h1>把划线装进口袋</h1>"
        "<p>填写常用的 2.4 GHz Wi-Fi 和微信读书 API Key。验证并首次同步成功后，"
        "Linepods 才会保存这份配置。</p><form method=post action=/configure>"
        "<label for=ssid>Wi-Fi 名称</label><input id=ssid name=ssid maxlength=32 "
        "autocomplete=off placeholder='仅支持 2.4 GHz Wi-Fi' required>"
        "<label for=password>Wi-Fi 密码</label><input id=password name=password "
        "type=password maxlength=64 autocomplete=current-password placeholder='开放网络可留空'>"
        "<label for=key>微信读书 API Key</label><input id=key name=key type=password "
        "maxlength=160 autocomplete=off placeholder='wrk-…' required>"
        "<div class=hint><strong>在哪里获取 API Key？</strong>打开微信读书 App，在 "
        "<span class=path>我 → 设置 → 微信读书技能 → 获取 API Key</span> 中找到并复制。"
        "</div><button type=submit>连接并同步划线</button></form>"
        "<div class=foot>提交后请回到 Linepods 屏幕查看验证结果</div>"
        "</section></main></html>";
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_send(request, PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t configure_handler(httpd_req_t *request)
{
    if (request->content_len <= 0 || request->content_len > CONFIG_FORM_MAX) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid form size");
        return ESP_FAIL;
    }
    char body[CONFIG_FORM_MAX + 1];
    size_t received = 0;
    while (received < (size_t)request->content_len) {
        int result = httpd_req_recv(request, body + received,
                                    request->content_len - received);
        if (result <= 0) {
            httpd_resp_send_err(request, HTTPD_408_REQ_TIMEOUT, "Receive timeout");
            return ESP_FAIL;
        }
        received += (size_t)result;
    }
    body[received] = '\0';

    linepods_config_t config = {0};
    bool valid = linepods_form_value(body, "ssid", config.ssid, sizeof(config.ssid))
              && linepods_form_value(body, "password", config.password,
                                   sizeof(config.password))
              && linepods_form_value(body, "key", config.api_key,
                                   sizeof(config.api_key))
              && linepods_config_valid(&config);
    memset(body, 0, sizeof(body));
    if (!valid) {
        memset(&config, 0, sizeof(config));
        static const char INVALID[] =
            "<!doctype html><meta charset=utf-8><meta name=viewport "
            "content='width=device-width,initial-scale=1'><style>body{max-width:32rem;"
            "margin:3rem auto;padding:0 1.2rem;font:17px/1.6 system-ui;color:#111;"
            "background:#fff}section{border:1px solid #ddd;border-radius:14px;padding:22px}"
            "a{color:#111;font-weight:650}</style>"
            "<section><h2>有几项内容需要检查</h2><p>请确认 Wi-Fi 名称没有留空，"
            "密码填写正确，并使用以 <strong>wrk-</strong> 开头的微信读书 API Key。</p>"
            "<a href='/'>返回修改</a></section>";
        httpd_resp_set_status(request, "400 Bad Request");
        httpd_resp_set_type(request, "text/html; charset=utf-8");
        httpd_resp_sendstr(request, INVALID);
        return ESP_FAIL;
    }

    bool accepted = s_callback && s_callback(&config, s_callback_user);
    memset(&config, 0, sizeof(config));
    if (!accepted) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        httpd_resp_set_type(request, "text/html; charset=utf-8");
        httpd_resp_sendstr(request,
            "<!doctype html><meta charset=utf-8><meta name=viewport "
            "content='width=device-width,initial-scale=1'><h2>Linepods 正在处理上一项任务</h2>"
            "<p>请稍等片刻，再返回配置页重新提交。</p><a href='/'>返回配置</a>");
        return ESP_FAIL;
    }
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_sendstr(request,
        "<!doctype html><meta charset=utf-8><meta name=viewport "
        "content='width=device-width,initial-scale=1'><style>body{max-width:32rem;"
        "margin:3rem auto;padding:0 1.2rem;font:17px/1.6 system-ui;color:#111;"
        "background:#fff}section{background:#fff;border:1px solid #ddd;"
        "border-radius:14px;padding:22px}</style><section><h2>配置已提交</h2>"
        "<p>Linepods 正在连接 Wi-Fi、验证 API Key，并取回你的划线。首次同步可能需要"
        "一点时间，请保持设备开启并回到设备屏幕查看最终结果。</p></section>");
}

static void make_ap_credentials(void)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_setup_ssid, sizeof(s_setup_ssid), "Linepods-%02X%02X",
             mac[4], mac[5]);
    static const char ALPHABET[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
    for (size_t i = 0; i < 10; i++) {
        s_setup_password[i] = ALPHABET[esp_random() % (sizeof(ALPHABET) - 1)];
    }
    s_setup_password[10] = '\0';
}

esp_err_t linepods_network_start_provisioning(linepods_provision_callback_t callback,
                                             void *user)
{
    if (!s_initialized || !callback) return ESP_ERR_INVALID_ARG;
    if (s_server) return ESP_OK;

    esp_err_t start_error = ensure_radio_started();
    if (start_error != ESP_OK) return start_error;

    make_ap_credentials();
    s_callback = callback;
    s_callback_user = user;

    wifi_config_t access_point = {0};
    snprintf((char *)access_point.ap.ssid, sizeof(access_point.ap.ssid), "%s",
             s_setup_ssid);
    snprintf((char *)access_point.ap.password, sizeof(access_point.ap.password), "%s",
             s_setup_password);
    access_point.ap.ssid_len = strlen(s_setup_ssid);
    access_point.ap.channel = 1;
    access_point.ap.max_connection = 1;
    access_point.ap.authmode = WIFI_AUTH_WPA2_PSK;
    access_point.ap.pmf_cfg.capable = true;
    access_point.ap.pmf_cfg.required = false;

    esp_err_t error = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (error == ESP_OK) error = esp_wifi_set_config(WIFI_IF_AP, &access_point);
    if (error != ESP_OK) return error;

    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.max_open_sockets = 3;
    server_config.backlog_conn = 2;
    server_config.lru_purge_enable = true;
    server_config.stack_size = 6144;
    server_config.uri_match_fn = httpd_uri_match_wildcard;
    error = httpd_start(&s_server, &server_config);
    if (error != ESP_OK) return error;

    const httpd_uri_t configure = {
        .uri = "/configure", .method = HTTP_POST, .handler = configure_handler,
    };
    const httpd_uri_t page = {
        .uri = "/*", .method = HTTP_GET, .handler = page_handler,
    };
    error = httpd_register_uri_handler(s_server, &configure);
    if (error == ESP_OK) error = httpd_register_uri_handler(s_server, &page);
    if (error != ESP_OK) {
        linepods_network_stop_provisioning();
        return error;
    }
    ESP_LOGI(TAG, "Temporary setup access point started");
    return ESP_OK;
}

void linepods_network_stop_provisioning(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    s_callback = NULL;
    s_callback_user = NULL;
    if (s_initialized) (void)esp_wifi_set_mode(WIFI_MODE_STA);
    memset(s_setup_password, 0, sizeof(s_setup_password));
}

const char *linepods_network_setup_ssid(void)
{
    return s_setup_ssid;
}

const char *linepods_network_setup_password(void)
{
    return s_setup_password;
}
