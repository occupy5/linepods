#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "linepods_client.h"
#include "linepods_config.h"
#include "linepods_logic.h"
#include "linepods_network.h"
#include "linepods_pager.h"
#include "linepods_power_policy.h"
#include "linepods_status.h"
#include "linepods_store.h"
#include "linepods_sync_policy.h"
#include "linepods_ui.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define APP_QUEUE_DEPTH 8
#define FETCH_TASK_STACK 8192
#define BATTERY_REFRESH_MS 30000
#define CLOCK_REFRESH_MS 1000
/* 深睡唤醒后 CW2017 要先重做首次 SOC 换算，这段窗口内 bsp_battery_soc() 返回
 * -1。用更短的间隔重试，等读数可信后再回到常规刷新周期。 */
#define BATTERY_RETRY_MS 1000
#define IDLE_POLL_MS 1000
#define AUTO_SYNC_TTL_SECONDS (12u * 60u * 60u)
#define RADIO_SUSPEND_GRACE_MS (10u * 1000u)

/* Backlight fractions for each power phase. The panel stays initialized; only
 * the LEDC duty changes, so waking is a single register write. */
#define BACKLIGHT_ACTIVE 90
#define BACKLIGHT_DIM 20
#define BACKLIGHT_OFF 0

/* Idle ladder. Tune against measured standby current; these are chosen so a
 * reader who pauses for a minute keeps a readable screen, and a device left
 * alone overnight parks in deep sleep instead of burning the 520 mAh cell. */
#define POWER_DIM_AFTER_MS (10u * 1000u)
#define POWER_SCREEN_OFF_AFTER_MS (30u * 1000u)
#define POWER_RADIO_OFF_AFTER_MS (60u * 1000u)
#define POWER_DEEP_SLEEP_AFTER_MS (5u * 60u * 1000u)

static const linepods_power_policy_config_t s_power_config = {
    .dim_after_ms = POWER_DIM_AFTER_MS,
    .screen_off_after_ms = POWER_SCREEN_OFF_AFTER_MS,
    .radio_off_after_ms = POWER_RADIO_OFF_AFTER_MS,
    .deep_sleep_after_ms = POWER_DEEP_SLEEP_AFTER_MS,
};

static const char *TAG = "linepods";

typedef enum {
    APP_EVENT_INPUT = 0,
    APP_EVENT_PROVISION_CONFIG,
    APP_EVENT_FETCH_SUCCESS,
    APP_EVENT_FETCH_ERROR,
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    union {
        struct {
            bsp_btn_t button;
            bsp_btn_ev_t action;
        } input;
        linepods_config_t config;
        struct {
            bool provisioned;
            bool had_cache;
            char message[160];
        } fetch;
    } data;
} app_event_t;

typedef struct {
    linepods_config_t config;
    bool provisioned;
    bool had_cache;
} fetch_context_t;

static QueueHandle_t s_app_queue;
static volatile bool s_input_ready;
static volatile bool s_fetching;
static volatile bool s_activity_pending;
/* Written by the app task when the idle ladder blanks the display. The button
 * callback reads it to treat the next press as a wake gesture. */
static volatile bool s_display_blank;
/* True while the wake gesture is still in progress, so its trailing CLICK/LONG
 * is consumed instead of scrolling the card the user cannot see yet. */
static volatile bool s_wake_swallow;
static linepods_power_policy_t s_power;
static linepods_power_state_t s_power_applied = LINEPODS_POWER_ACTIVE;
static bool s_battery_available;
static bool s_deep_sleep_wakeup;
static bool s_record_sync_timestamp;
static uint64_t s_radio_suspend_due_ms;
static linepods_config_t s_active_config;
static linepods_config_t s_retry_config;
static bool s_retry_is_provisioned;
static linepods_content_t s_content;
static linepods_logic_t s_logic;
static char *s_detail_text;
static size_t s_detail_item_index = SIZE_MAX;
static linepods_pager_t s_detail_pager;

static uint64_t now_ms(void);

static void release_detail(void)
{
    free(s_detail_text);
    s_detail_text = NULL;
    s_detail_item_index = SIZE_MAX;
    s_detail_pager = (linepods_pager_t) {0};
}

static void ui_loading(const char *message)
{
    if (!bsp_lvgl_lock(500)) return;
    linepods_ui_show_loading(message);
    bsp_lvgl_unlock();
}

static void ui_error(const char *message)
{
    linepods_logic_set_view(&s_logic, LINEPODS_VIEW_ERROR);
    if (!bsp_lvgl_lock(500)) return;
    linepods_ui_show_error(message);
    bsp_lvgl_unlock();
}

static void ui_welcome(void)
{
    linepods_logic_set_view(&s_logic, LINEPODS_VIEW_WELCOME);
    if (!bsp_lvgl_lock(500)) return;
    linepods_ui_show_welcome();
    bsp_lvgl_unlock();
}

static void ui_setup_failure(const char *message)
{
    linepods_logic_set_view(&s_logic, LINEPODS_VIEW_ERROR);
    if (!bsp_lvgl_lock(500)) return;
    linepods_ui_show_setup_failure(message);
    bsp_lvgl_unlock();
}

static void ui_card(void)
{
    if (s_content.item_index != s_logic.quote_index) {
        esp_err_t error = linepods_store_read_item(s_logic.quote_index,
                                                 &s_content.item);
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "Cannot read item %u: %s",
                     (unsigned)s_logic.quote_index, esp_err_to_name(error));
            ui_error("无法从设备存储读取这条划线");
            return;
        }
        s_content.item_index = s_logic.quote_index;
    }
    if (!bsp_lvgl_lock(500)) return;
    linepods_ui_show_card(&s_content, s_logic.quote_index);
    bsp_lvgl_unlock();
}

static void ui_detail_page(void)
{
    if (!s_detail_text || s_detail_item_index != s_logic.quote_index) return;
    if (!bsp_lvgl_lock(500)) return;
    linepods_ui_show_detail(&s_content, s_logic.quote_index, s_detail_text,
                          &s_detail_pager, s_logic.detail_page_index);
    bsp_lvgl_unlock();
}

static bool open_detail_item(void)
{
    if (s_content.item_index != s_logic.quote_index) {
        esp_err_t item_error = linepods_store_read_item(s_logic.quote_index,
                                                      &s_content.item);
        if (item_error != ESP_OK) {
            ui_error("无法从设备存储读取这条划线");
            return false;
        }
        s_content.item_index = s_logic.quote_index;
    }
    char *text = NULL;
    size_t length = 0;
    esp_err_t error = linepods_store_read_text(s_logic.quote_index, &text, &length);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Cannot read quote %u: %s", (unsigned)s_logic.quote_index,
                 esp_err_to_name(error));
        ui_error(error == ESP_ERR_NO_MEM
               ? "内存不足，无法打开这条完整划线"
               : "无法从设备存储读取这条划线");
        return false;
    }

    if (!bsp_lvgl_lock(500)) {
        free(text);
        return false;
    }
    release_detail();
    s_detail_text = text;
    s_detail_item_index = s_logic.quote_index;
    bool paginated = linepods_ui_prepare_detail_pager(s_detail_text,
                                                     &s_detail_pager);
    if (!paginated) {
        bsp_lvgl_unlock();
        release_detail();
        ui_error("这条划线太长，无法生成阅读分页");
        return false;
    }
    linepods_logic_detail_ready(&s_logic, s_detail_pager.page_count);
    linepods_ui_show_detail(&s_content, s_logic.quote_index, s_detail_text,
                          &s_detail_pager, s_logic.detail_page_index);
    bsp_lvgl_unlock();
    ESP_LOGI(TAG, "Opened quote %u: bytes=%u pages=%u",
             (unsigned)s_logic.quote_index, (unsigned)length,
             (unsigned)s_detail_pager.page_count);
    return true;
}

static void fetch_task(void *argument)
{
    fetch_context_t *context = argument;
    app_event_t event = {
        .type = APP_EVENT_FETCH_ERROR,
        .data.fetch.provisioned = context->provisioned,
        .data.fetch.had_cache = context->had_cache,
    };

    esp_err_t error = linepods_network_connect(&context->config, 20000);
    if (error == ESP_OK) {
        error = linepods_client_sync(&context->config, event.data.fetch.message,
                                   sizeof(event.data.fetch.message));
    } else {
        linepods_network_format_connect_error(error, event.data.fetch.message,
                                            sizeof(event.data.fetch.message));
    }
    if (error == ESP_OK) event.type = APP_EVENT_FETCH_SUCCESS;
    (void)xQueueSend(s_app_queue, &event, portMAX_DELAY);
    memset(context, 0, sizeof(*context));
    free(context);
    vTaskDelete(NULL);
}

static esp_err_t start_fetch(const linepods_config_t *config, bool provisioned,
                             bool had_cache)
{
    if (s_fetching) return ESP_ERR_INVALID_STATE;
    fetch_context_t *context = calloc(1, sizeof(*context));
    if (!context) return ESP_ERR_NO_MEM;
    context->config = *config;
    context->provisioned = provisioned;
    context->had_cache = had_cache;
    s_retry_config = *config;
    s_retry_is_provisioned = provisioned;
    s_radio_suspend_due_ms = 0;
    s_record_sync_timestamp = false;
    s_fetching = true;

    BaseType_t created = xTaskCreate(fetch_task, "linepods_fetch", FETCH_TASK_STACK,
                                     context, 5, NULL);
    if (created != pdPASS) {
        s_fetching = false;
        memset(context, 0, sizeof(*context));
        free(context);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void schedule_radio_suspend(bool record_sync_timestamp)
{
    s_record_sync_timestamp = record_sync_timestamp;
    s_radio_suspend_due_ms = now_ms() + RADIO_SUSPEND_GRACE_MS;
}

static bool provisioning_callback(const linepods_config_t *config, void *user)
{
    (void)user;
    if (!s_app_queue || s_fetching) return false;
    app_event_t event = {
        .type = APP_EVENT_PROVISION_CONFIG,
        .data.config = *config,
    };
    return xQueueSend(s_app_queue, &event, 0) == pdTRUE;
}

static void start_setup(void)
{
    esp_err_t error = linepods_network_start_provisioning(provisioning_callback, NULL);
    if (error != ESP_OK) {
        char message[128];
        snprintf(message, sizeof(message), "无法启动设置热点（%s）",
                 esp_err_to_name(error));
        ui_error(message);
        return;
    }
    linepods_logic_set_view(&s_logic, LINEPODS_VIEW_SETUP);
    if (!bsp_lvgl_lock(500)) return;
    linepods_ui_show_setup(linepods_network_setup_ssid(),
                         linepods_network_setup_password());
    bsp_lvgl_unlock();
}

static bool input_to_logic(const app_event_t *event, linepods_input_t *input)
{
    if (event->data.input.action == BSP_BTN_CLICK) {
        if (event->data.input.button == BSP_BTN_UP) *input = LINEPODS_INPUT_UP_CLICK;
        else if (event->data.input.button == BSP_BTN_DOWN) *input = LINEPODS_INPUT_DOWN_CLICK;
        else if (event->data.input.button == BSP_BTN_OK) *input = LINEPODS_INPUT_OK_CLICK;
        else return false;
        return true;
    }
    if (event->data.input.action == BSP_BTN_LONG) {
        if (event->data.input.button == BSP_BTN_UP) *input = LINEPODS_INPUT_UP_LONG;
        else if (event->data.input.button == BSP_BTN_DOWN) *input = LINEPODS_INPUT_DOWN_LONG;
        else if (event->data.input.button == BSP_BTN_OK) *input = LINEPODS_INPUT_OK_LONG;
        else return false;
        return true;
    }
    return false;
}

static void handle_input(const app_event_t *event)
{
    linepods_input_t input;
    if (!input_to_logic(event, &input)) return;
    linepods_action_t action = linepods_logic_handle(&s_logic, input);
    switch (action) {
    case LINEPODS_ACTION_SHOW_CARD:
        release_detail();
        ui_card();
        break;
    case LINEPODS_ACTION_SHOW_DETAIL:
        ui_detail_page();
        break;
    case LINEPODS_ACTION_SHOW_DETAIL_NEW_ITEM:
        (void)open_detail_item();
        break;
    case LINEPODS_ACTION_SHOW_FONT_COMPARE:
        if (bsp_lvgl_lock(500)) {
            linepods_ui_show_font_compare(s_logic.font_compare_index);
            bsp_lvgl_unlock();
        }
        break;
    case LINEPODS_ACTION_SYNC:
        if (!s_fetching) {
            ui_loading("正在同步全部书籍与划线…");
            if (start_fetch(&s_active_config, false,
                            s_content.item_count > 0) != ESP_OK) {
                ui_card();
            }
        }
        break;
    case LINEPODS_ACTION_RETRY:
        if (!s_fetching) {
            ui_loading("正在重新连接并取回摘录…");
            if (start_fetch(&s_retry_config, s_retry_is_provisioned,
                            s_content.item_count > 0) != ESP_OK) {
                ui_error("没有足够内存开始同步，请重新启动设备");
            }
        }
        break;
    case LINEPODS_ACTION_START_SETUP:
        if (!s_fetching) start_setup();
        break;
    default:
        break;
    }
}

static void handle_event(const app_event_t *event)
{
    switch (event->type) {
    case APP_EVENT_INPUT:
        handle_input(event);
        break;
    case APP_EVENT_PROVISION_CONFIG:
        s_activity_pending = true;  /* show the verification progress */
        if (!s_fetching) {
            ui_loading("正在验证 Wi-Fi 和微信读书 Key…");
            if (start_fetch(&event->data.config, true, false) != ESP_OK) {
                ui_error("没有足够内存开始验证，请稍后重试");
            }
        }
        break;
    case APP_EVENT_FETCH_SUCCESS:
        s_fetching = false;
        s_activity_pending = true;  /* light the screen for the new content */
        release_detail();
        if (linepods_store_load_content(&s_content) != ESP_OK) {
            ui_error("完整缓存已写入，但重新读取失败");
            break;
        }
        if (event->data.fetch.provisioned) {
            if (linepods_config_save(&s_retry_config) != ESP_OK) {
                ui_error("内容已取回，但配置保存失败");
                break;
            }
            s_active_config = s_retry_config;
        }
        linepods_network_stop_provisioning();
        linepods_logic_set_quotes(&s_logic, s_content.item_count);
        if (s_content.item_count == 0) {
            ui_error("连接成功，但还没有找到划线。请先在微信读书中划线，再按确定重试。");
        } else if (event->data.fetch.provisioned) {
            linepods_logic_set_view(&s_logic, LINEPODS_VIEW_SYNC_SUCCESS);
            if (bsp_lvgl_lock(500)) {
                linepods_ui_show_sync_success(&s_content);
                bsp_lvgl_unlock();
            }
        } else {
            ui_card();
        }
        schedule_radio_suspend(true);
        break;
    case APP_EVENT_FETCH_ERROR:
        s_fetching = false;
        s_activity_pending = true;  /* light the screen for the error card */
        if (event->data.fetch.had_cache && s_content.item_count > 0) {
            ESP_LOGW(TAG, "Background sync failed; keeping cache: %s",
                     event->data.fetch.message);
            ui_card();
        } else if (event->data.fetch.provisioned) {
            ui_setup_failure(event->data.fetch.message[0]
                           ? event->data.fetch.message
                           : "暂时无法完成首次同步，请检查配置后再试。");
        } else {
            ui_error(event->data.fetch.message[0]
                   ? event->data.fetch.message : "同步失败，请稍后重试");
        }
        if (!linepods_network_is_provisioning()) schedule_radio_suspend(false);
        break;
    }
}

static uint64_t now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

/* Apply a reversible display/radio phase. Only the app task calls this, and
 * every branch is idempotent so re-applying the same phase is harmless. */
static void apply_power_state(linepods_power_state_t state)
{
    switch (state) {
    case LINEPODS_POWER_DIM:
        bsp_display_backlight(BACKLIGHT_DIM);
        s_display_blank = false;
        break;
    case LINEPODS_POWER_SCREEN_OFF:
        bsp_display_backlight(BACKLIGHT_OFF);
        s_display_blank = true;
        break;
    case LINEPODS_POWER_RADIO_OFF: {
        bsp_display_backlight(BACKLIGHT_OFF);
        s_display_blank = true;
        esp_err_t error = linepods_network_suspend_radio();
        if (error != ESP_OK) {
            ESP_LOGW(TAG, "Deferring Wi-Fi suspend: %s", esp_err_to_name(error));
        }
        break;
    }
    case LINEPODS_POWER_ACTIVE:
    default:
        bsp_display_backlight(BACKLIGHT_ACTIVE);
        s_display_blank = false;
        break;
    }
}

/* Terminal power-down. After bsp_audio_prepare_deep_sleep()/
 * bsp_i2c_prepare_deep_sleep() release the shared I2S/I2C pins they cannot be
 * restored in this run, so this function either deep-sleeps or restarts; it
 * never returns to the caller under normal operation. Ordering mirrors
 * demo_low_power.c: gauge -> codec -> pin release -> panel -> sleep. */
static void enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "Idle timeout reached; entering deep sleep (wake on any key)");
    (void)linepods_network_suspend_radio();
    bsp_display_backlight(BACKLIGHT_OFF);

    gpio_config_t wake_pin = {
        .pin_bit_mask = 1ULL << BSP_BTN_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,   /* board has an external 10k pull-up */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t error = gpio_config(&wake_pin);
    if (error == ESP_OK) {
        /* Pass a pin *bitmask*: GPIO_NUM_0 (0) would arm nothing and leave the
         * device unable to wake. Any key pulls the shared node below the digital
         * low threshold, so a low-level wake covers all three buttons. */
        error = esp_deep_sleep_enable_gpio_wakeup(1ULL << BSP_BTN_GPIO,
                                                  ESP_GPIO_WAKEUP_GPIO_LOW);
    }
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Cannot arm GPIO wakeup (%s); staying awake",
                 esp_err_to_name(error));
        s_power_applied = LINEPODS_POWER_ACTIVE;
        linepods_power_policy_record_activity(&s_power, now_ms());
        apply_power_state(LINEPODS_POWER_ACTIVE);
        return;
    }

    error = bsp_battery_sleep();
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "CW2017 suspend failed: %s", esp_err_to_name(error));
    }
    error = bsp_audio_sleep();
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "ES8311 suspend failed: %s", esp_err_to_name(error));
    }
    error = bsp_audio_prepare_deep_sleep();
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "I2S pin release failed: %s", esp_err_to_name(error));
    }
    error = bsp_i2c_prepare_deep_sleep();
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "I2C pin release failed: %s", esp_err_to_name(error));
    }

    /* Block LVGL between the final flush and the panel sleep so nothing draws
     * into a powered-down display. If it will not lock, restart to recover. */
    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "LVGL busy before deep sleep; restarting to restore peripherals");
        esp_restart();
    }
    error = bsp_display_prepare_deep_sleep();
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "ST7789 suspend failed: %s", esp_err_to_name(error));
    }

    esp_deep_sleep_start();
    /* Reached only if sleep did not take effect; the buses are already torn
     * down, so a clean restart is the only safe recovery. */
    ESP_LOGE(TAG, "esp_deep_sleep_start returned unexpectedly; restarting");
    esp_restart();
}

static void app_task(void *argument)
{
    (void)argument;
    linepods_logic_init(&s_logic);
    linepods_power_policy_init(&s_power, &s_power_config, now_ms());
    esp_err_t loaded = linepods_config_load(&s_active_config);
    if (loaded == ESP_OK) {
        bool has_cache = linepods_store_load_content(&s_content) == ESP_OK;
        if (has_cache) {
            linepods_logic_set_quotes(&s_logic, s_content.item_count);
            ui_card();
        } else {
            ui_loading("正在同步全部书籍与划线…");
        }
        time_t system_time = time(NULL);
        time_t last_sync = 0;
        esp_err_t timestamp_error = linepods_config_load_last_sync(&last_sync);
        if (timestamp_error != ESP_OK) {
            ESP_LOGW(TAG, "Cannot load last sync time: %s",
                     esp_err_to_name(timestamp_error));
        }
        linepods_sync_decision_t sync_decision = linepods_sync_policy_decide(
            has_cache, s_deep_sleep_wakeup,
            linepods_status_time_valid(system_time), (int64_t)system_time,
            (int64_t)last_sync, AUTO_SYNC_TTL_SECONDS);
        bool auto_sync = linepods_sync_policy_should_sync(sync_decision);
        ESP_LOGI(TAG, "Boot sync decision=%s wake=%s cache=%s",
                 linepods_sync_policy_decision_name(sync_decision),
                 s_deep_sleep_wakeup ? "deep-sleep" : "cold",
                 has_cache ? "yes" : "no");
        if (auto_sync
            && start_fetch(&s_active_config, false, has_cache) != ESP_OK) {
            if (has_cache) {
                ESP_LOGW(TAG, "Cannot start background sync; keeping cache");
                ui_card();
            } else {
                ui_error("无法启动同步任务，请重新启动设备");
            }
        }
    } else {
        ESP_LOGI(TAG, "No saved configuration; waiting for user to start setup");
        ui_welcome();
    }

    TickType_t last_battery = 0;
    TickType_t last_clock = 0;
    time_t displayed_minute = (time_t)-1;
    bool battery_soc_valid = false;
    for (;;) {
        app_event_t event;
        if (xQueueReceive(s_app_queue, &event, pdMS_TO_TICKS(IDLE_POLL_MS)) == pdTRUE) {
            handle_event(&event);
            if (event.type == APP_EVENT_PROVISION_CONFIG) {
                memset(&event.data.config, 0, sizeof(event.data.config));
            }
        }

        if (s_activity_pending) {
            s_activity_pending = false;
            linepods_power_policy_record_activity(&s_power, now_ms());
        }

        bool provisioning = linepods_network_is_provisioning();
        bool busy = s_fetching;

        uint64_t current_ms = now_ms();
        if (s_radio_suspend_due_ms != 0 && current_ms >= s_radio_suspend_due_ms
            && !busy && !provisioning) {
            if (s_record_sync_timestamp) {
                time_t sync_time = time(NULL);
                if (linepods_status_time_valid(sync_time)) {
                    esp_err_t saved = linepods_config_save_last_sync(sync_time);
                    if (saved != ESP_OK) {
                        ESP_LOGW(TAG, "Cannot save last sync time: %s",
                                 esp_err_to_name(saved));
                    }
                } else {
                    ESP_LOGW(TAG, "System clock is not valid after sync; age remains unknown");
                }
                s_record_sync_timestamp = false;
            }
            esp_err_t suspend_error = linepods_network_suspend_radio();
            if (suspend_error == ESP_OK) {
                s_radio_suspend_due_ms = 0;
            } else {
                s_radio_suspend_due_ms = current_ms + 1000;
            }
        }

        /* The display may always blank - a wake press brings the setup
         * credentials back with no navigation - but the radio must stay up while
         * the setup AP or an in-flight transfer still needs it, and deep sleep
         * waits for the radio-off phase below. */
        linepods_power_state_t state = linepods_power_policy_update(
            &s_power, now_ms(), true, /* allow_dim */
            true,                     /* allow_screen_off */
            !busy && !provisioning);  /* allow_radio_off */
        if (state != s_power_applied) {
            s_power_applied = state;
            apply_power_state(state);
        }

        TickType_t now = xTaskGetTickCount();
        if (last_clock == 0
            || now - last_clock >= pdMS_TO_TICKS(CLOCK_REFRESH_MS)) {
            time_t system_time = time(NULL);
            time_t minute = system_time / 60;
            if (minute != displayed_minute) {
                if (bsp_lvgl_lock(250)) {
                    linepods_ui_set_time(system_time);
                    bsp_lvgl_unlock();
                    displayed_minute = minute;
                }
            }
            last_clock = now;
        }

        uint32_t battery_period = battery_soc_valid ? BATTERY_REFRESH_MS
                                                    : BATTERY_RETRY_MS;
        if (s_battery_available && s_power_applied == LINEPODS_POWER_ACTIVE
            && (last_battery == 0
                || now - last_battery >= pdMS_TO_TICKS(battery_period))) {
            int percent = bsp_battery_soc();
            /* 换算完成前保留占位符，绝不让瞬时值覆盖已显示的真实电量。 */
            if (percent >= 0) {
                battery_soc_valid = true;
                if (bsp_lvgl_lock(250)) {
                    linepods_ui_set_battery(percent);
                    bsp_lvgl_unlock();
                }
            }
            last_battery = now;
        }

        if (s_power_applied == LINEPODS_POWER_RADIO_OFF && !busy && !provisioning
            && linepods_power_policy_deep_sleep_due(&s_power, now_ms(), true)) {
            enter_deep_sleep();
        }
    }
}

static void on_button(bsp_btn_t button, bsp_btn_ev_t action, void *user)
{
    (void)user;
    if (!s_input_ready || !s_app_queue) return;
    /* Runs on the shared esp_timer task: only flag state and enqueue. */
    s_activity_pending = true;

    /* Every gesture begins with a press-down, which is never itself a command.
     * If the display is blank, that whole gesture is the wake gesture and is
     * consumed end to end - so waking by holding a key cannot also fire the
     * long-press action (sync / setup) on the screen the user has not seen. */
    if (action == BSP_BTN_PRESS) {
        s_wake_swallow = s_display_blank;
        return;
    }
    if (s_wake_swallow) return;

    app_event_t event = {
        .type = APP_EVENT_INPUT,
        .data.input = {.button = button, .action = action},
    };
    (void)xQueueSend(s_app_queue, &event, 0);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Linepods for AI Passport");
    esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    s_deep_sleep_wakeup = wakeup == ESP_SLEEP_WAKEUP_GPIO;
    ESP_LOGI(TAG, "Wake cause=%d deep_sleep_button=%s", (int)wakeup,
             s_deep_sleep_wakeup ? "yes" : "no");
    /* Linepods serves a Chinese audience; SNTP supplies UTC and POSIX TZ turns
     * it into the device's local civil time without changing the system clock. */
    (void)setenv("TZ", "CST-8", 1);
    tzset();
    esp_err_t error = nvs_flash_init();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed without erasing data: %s",
                 esp_err_to_name(error));
        return;
    }
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display initialization failed (MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(90);
    if (bsp_lvgl_lock(1000)) {
        linepods_ui_init();
        bsp_lvgl_unlock();
    }

    s_battery_available = bsp_battery_init() == ESP_OK;
    error = linepods_store_init();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Linepods cache initialization failed: %s",
                 esp_err_to_name(error));
        ui_error("划线存储初始化失败，请重新刷写完整固件");
        return;
    }
    error = linepods_network_init();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Network initialization failed: %s", esp_err_to_name(error));
        ui_error("网络模块初始化失败，请重新启动设备");
        return;
    }

    s_app_queue = xQueueCreate(APP_QUEUE_DEPTH, sizeof(app_event_t));
    if (!s_app_queue) {
        ui_error("内存不足，无法创建应用队列");
        return;
    }
    error = bsp_button_init(on_button, NULL);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Button initialization failed: %s", esp_err_to_name(error));
        ui_error("按键初始化失败");
        return;
    }
    if (xTaskCreate(app_task, "linepods_app", 6144, NULL, 5, NULL) != pdPASS) {
        ui_error("内存不足，无法启动应用");
        return;
    }
    s_input_ready = true;
}
