#pragma once

#include "linepods_client.h"
#include "linepods_logic.h"
#include "linepods_pager.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

void linepods_ui_init(void);
void linepods_ui_show_welcome(void);
void linepods_ui_show_setup(const char *ssid, const char *password);
void linepods_ui_show_loading(const char *message);
void linepods_ui_show_sync_success(const linepods_content_t *content);
void linepods_ui_show_setup_failure(const char *message);
void linepods_ui_show_card(const linepods_content_t *content, size_t index);
bool linepods_ui_prepare_detail_pager(const char *text, linepods_pager_t *pager);
void linepods_ui_show_detail(const linepods_content_t *content, size_t index,
                           const char *text, const linepods_pager_t *pager,
                           size_t page_index);
void linepods_ui_show_error(const char *message);
void linepods_ui_show_font_compare(size_t index);
void linepods_ui_set_battery(int percent);
void linepods_ui_set_time(time_t timestamp);
