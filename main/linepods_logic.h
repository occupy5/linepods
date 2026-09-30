#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LINEPODS_READING_OPTION_COUNT 3
#define LINEPODS_SETTINGS_SECTION_COUNT 3

typedef enum {
    LINEPODS_VIEW_WELCOME = 0,
    LINEPODS_VIEW_SETUP,
    LINEPODS_VIEW_LOADING,
    LINEPODS_VIEW_CARD,
    LINEPODS_VIEW_DETAIL,
    LINEPODS_VIEW_BROWSE_MENU,
    LINEPODS_VIEW_BOOK_PICKER,
    LINEPODS_VIEW_SETTINGS,
    LINEPODS_VIEW_SETTINGS_ITEMS,
    LINEPODS_VIEW_SETTINGS_CONFIRM,
    LINEPODS_VIEW_SETTINGS_VALUE,
    LINEPODS_VIEW_SETTINGS_INFO,
    LINEPODS_VIEW_SYNC_SUCCESS,
    LINEPODS_VIEW_ERROR,
} linepods_view_t;

typedef enum {
    LINEPODS_INPUT_UP_CLICK = 0,
    LINEPODS_INPUT_DOWN_CLICK,
    LINEPODS_INPUT_OK_CLICK,
    LINEPODS_INPUT_UP_LONG,
    LINEPODS_INPUT_DOWN_LONG,
    LINEPODS_INPUT_OK_LONG,
} linepods_input_t;

typedef enum {
    LINEPODS_ACTION_NONE = 0,
    LINEPODS_ACTION_SHOW_CARD,
    LINEPODS_ACTION_SHOW_DETAIL,
    LINEPODS_ACTION_SHOW_DETAIL_NEW_ITEM,
    LINEPODS_ACTION_SHOW_BROWSE_MENU,
    LINEPODS_ACTION_SHOW_BOOK_PICKER,
    LINEPODS_ACTION_SELECT_RANDOM,
    LINEPODS_ACTION_SELECT_ALL,
    LINEPODS_ACTION_SELECT_BOOK,
    LINEPODS_ACTION_SHOW_SETTINGS,
    LINEPODS_ACTION_UPDATE_WIFI,
    LINEPODS_ACTION_UPDATE_API_KEY,
    LINEPODS_ACTION_CLEAR_CONFIG,
    LINEPODS_ACTION_CLEAR_CACHE,
    LINEPODS_ACTION_EDIT_SCREEN_TIMEOUT,
    LINEPODS_ACTION_EDIT_DEEP_SLEEP,
    LINEPODS_ACTION_SHOW_SETTINGS_VALUE,
    LINEPODS_ACTION_SAVE_SCREEN_TIMEOUT,
    LINEPODS_ACTION_SAVE_DEEP_SLEEP,
    LINEPODS_ACTION_SHOW_SETTINGS_INFO,
    LINEPODS_ACTION_SYNC,
    LINEPODS_ACTION_RETRY,
    LINEPODS_ACTION_START_SETUP,
} linepods_action_t;

typedef enum {
    LINEPODS_BROWSE_ALL = 0,
    LINEPODS_BROWSE_BOOK,
} linepods_browse_mode_t;

typedef enum {
    LINEPODS_SETTING_UPDATE_WIFI = 0,
    LINEPODS_SETTING_UPDATE_API_KEY,
    LINEPODS_SETTING_CLEAR_CONFIG,
    LINEPODS_SETTING_CLEAR_CACHE,
    LINEPODS_SETTING_SCREEN_TIMEOUT,
    LINEPODS_SETTING_DEEP_SLEEP,
    LINEPODS_SETTING_INFO,
} linepods_setting_t;

typedef enum {
    LINEPODS_SETTINGS_SECTION_CONNECTION = 0,
    LINEPODS_SETTINGS_SECTION_POWER,
    LINEPODS_SETTINGS_SECTION_DATA,
} linepods_settings_section_t;

typedef struct {
    linepods_view_t view;
    size_t quote_count;
    size_t quote_index;
    size_t range_start;
    size_t range_count;
    size_t detail_page_index;
    size_t detail_page_count;
    size_t browse_menu_index;
    size_t book_index;
    size_t book_count;
    size_t settings_index;
    size_t settings_item_index;
    size_t settings_value_index;
    size_t settings_value_count;
    size_t confirm_index;
    linepods_setting_t pending_setting;
    linepods_browse_mode_t browse_mode;
    bool detail_open_from_end;
} linepods_logic_t;

void linepods_logic_init(linepods_logic_t *logic);
void linepods_logic_set_quotes(linepods_logic_t *logic, size_t quote_count);
void linepods_logic_set_all_scope(linepods_logic_t *logic);
void linepods_logic_set_book_scope(linepods_logic_t *logic, size_t range_start,
                                  size_t range_count, size_t book_index);
void linepods_logic_set_book_count(linepods_logic_t *logic, size_t book_count);
void linepods_logic_open_setting_value(linepods_logic_t *logic,
                                      linepods_setting_t setting,
                                      size_t value_index, size_t value_count);
bool linepods_logic_set_quote_index(linepods_logic_t *logic, size_t quote_index);
size_t linepods_logic_select_random(linepods_logic_t *logic, uint32_t random_value);
void linepods_logic_set_view(linepods_logic_t *logic, linepods_view_t view);
linepods_action_t linepods_logic_handle(linepods_logic_t *logic, linepods_input_t input);

/* Supplies the page count after the newly selected quote has been loaded and
 * paginated. Entering it through UP selects its last page; all other paths
 * select the first page. */
void linepods_logic_detail_ready(linepods_logic_t *logic, size_t page_count);

/* Copies UTF-8 without splitting the final code point. The result is always
 * NUL-terminated when destination_size is non-zero. Returns copied bytes. */
size_t linepods_utf8_copy(char *destination, size_t destination_size,
                        const char *source);
