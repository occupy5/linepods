#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LINEPODS_FONT_COMPARE_COUNT 4
#define LINEPODS_BROWSE_OPTION_COUNT 3

typedef enum {
    LINEPODS_VIEW_WELCOME = 0,
    LINEPODS_VIEW_SETUP,
    LINEPODS_VIEW_LOADING,
    LINEPODS_VIEW_CARD,
    LINEPODS_VIEW_DETAIL,
    LINEPODS_VIEW_BROWSE_MENU,
    LINEPODS_VIEW_BOOK_PICKER,
    LINEPODS_VIEW_FONT_COMPARE,
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
    LINEPODS_ACTION_SHOW_FONT_COMPARE,
    LINEPODS_ACTION_SYNC,
    LINEPODS_ACTION_RETRY,
    LINEPODS_ACTION_START_SETUP,
} linepods_action_t;

typedef enum {
    LINEPODS_BROWSE_ALL = 0,
    LINEPODS_BROWSE_BOOK,
} linepods_browse_mode_t;

typedef struct {
    linepods_view_t view;
    size_t quote_count;
    size_t quote_index;
    size_t range_start;
    size_t range_count;
    size_t detail_page_index;
    size_t detail_page_count;
    size_t font_compare_index;
    size_t browse_menu_index;
    size_t book_index;
    size_t book_count;
    linepods_browse_mode_t browse_mode;
    bool detail_open_from_end;
} linepods_logic_t;

void linepods_logic_init(linepods_logic_t *logic);
void linepods_logic_set_quotes(linepods_logic_t *logic, size_t quote_count);
void linepods_logic_set_all_scope(linepods_logic_t *logic);
void linepods_logic_set_book_scope(linepods_logic_t *logic, size_t range_start,
                                  size_t range_count, size_t book_index);
void linepods_logic_set_book_count(linepods_logic_t *logic, size_t book_count);
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
