#pragma once

#include "linepods_client.h"
#include "linepods_logic.h"
#include "linepods_pager.h"
#include "linepods_store.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

void linepods_ui_init(void);
void linepods_ui_show_welcome(void);
void linepods_ui_show_setup(const char *title, const char *ssid,
                           const char *password);
void linepods_ui_show_loading(const char *message);
void linepods_ui_show_sync_success(const linepods_content_t *content,
                                  size_t previous_item_count);
void linepods_ui_show_setup_failure(const char *message);
void linepods_ui_show_card(const linepods_content_t *content, size_t index,
                          size_t range_start, size_t range_count,
                          const char *status);
void linepods_ui_show_browse_menu(size_t selected_index);
void linepods_ui_show_book_picker(const linepods_book_range_t *range,
                                 size_t book_count);
void linepods_ui_show_settings(size_t selected_index,
                              const linepods_preferences_t *preferences,
                              const linepods_store_info_t *info);
void linepods_ui_show_settings_items(linepods_settings_section_t section,
                                    size_t selected_index,
                                    const linepods_preferences_t *preferences);
void linepods_ui_show_settings_confirmation(linepods_setting_t setting,
                                           size_t selected_index);
void linepods_ui_show_setting_value(linepods_setting_t setting,
                                   const char *value, size_t index,
                                   size_t count);
void linepods_ui_show_settings_info(const char *version,
                                   const linepods_store_info_t *info);
bool linepods_ui_prepare_detail_pager(const char *text, linepods_pager_t *pager);
void linepods_ui_show_detail(const linepods_content_t *content, size_t index,
                           const char *text, const linepods_pager_t *pager,
                           size_t page_index, size_t range_start,
                           size_t range_count);
void linepods_ui_show_error(const char *message);
void linepods_ui_set_battery(int percent);
void linepods_ui_set_time(time_t timestamp);
